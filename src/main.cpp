#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "keyscan/KeyScanConfig.h"
#include "keyscan/KeyEvent.h"
#include "keyscan/IKeySource.h"
#include "keyscan/MatrixScanner.h"
#include "keyscan/RotaryEncoder.h"
#include "rgb/RGBLightControl.h"

using namespace ekeys;

#define TFT_MOSI 42
#define TFT_SCLK 41
#define TFT_DC 45
#define TFT_RST 46
#define TFT_CS 40
#define TFT_BL 37

#define TFT_MISO GFX_NOT_DEFINED

/*
 * ------------------------------------------------------------
 * Global state
 * ------------------------------------------------------------
 */

/* Handle to the on-screen "currently pressed key" label */
static lv_obj_t *g_key_label = NULL;
static lv_obj_t *g_encoder_label = NULL;
static lv_obj_t *g_rgb_label = NULL;

/* RGB 灯：单例。开始 Off，按 keyId=2 单击开/关；旋钮旋转切灯效。 */
static RGBLightControl g_rgb;

/* Thread-safe Serial.printf wrapper. Both the Arduino main loop() (Core 1)
   and the keyscan task (Core 0) may print, so guard against interleaved
   bytes in the same line. Must be defined before halt() uses it. */
static portMUX_TYPE g_serialMux = portMUX_INITIALIZER_UNLOCKED;
#define SERIAL_PRINTF(fmt, ...)            \
    do                                     \
    {                                      \
        portENTER_CRITICAL(&g_serialMux);  \
        Serial.printf(fmt, ##__VA_ARGS__); \
        portEXIT_CRITICAL(&g_serialMux);   \
    } while (0)

/*
 * Fatal error: print once, then halt silently so the message is not
 * drowned out by repeated output.
 */
static void halt(const char *msg)
{
    SERIAL_PRINTF("[FATAL] %s\n", msg);
    Serial.flush();
    while (true)
    {
        delay(1000);
    }
}

/*
 * Allocation helper: returns NULL on OOM after halting. Lets the call
 * sites stay tidy:
 *
 *     Arduino_GFX *gfx = alloc(new Arduino_NV3007(...));
 */
template <typename T>
static T *alloc(T *p)
{
    if (p == nullptr)
    {
        halt("out of memory");
    }
    return p;
}

/*
 * ------------------------------------------------------------
 * Display configuration
 * ------------------------------------------------------------
 *
 * Native:
 *
 *     142 x 428
 *
 * After rotation 1:
 *
 *     428 x 142
 */
#define TFT_WIDTH 142
#define TFT_HEIGHT 428
#define SCREEN_WIDTH 428
#define SCREEN_HEIGHT 142

/*
 * ------------------------------------------------------------
 * SPI bus & NV3007 display
 * ------------------------------------------------------------
 */
#define SPI_FAST_HZ 10000000

Arduino_DataBus *bus = alloc(new Arduino_ESP32SPI(
    TFT_DC,
    TFT_CS,
    TFT_SCLK,
    TFT_MOSI,
    TFT_MISO));

Arduino_GFX *gfx = alloc(new Arduino_NV3007(
    bus,
    TFT_RST,
    1,          // rotation
    false,      // IPS
    TFT_WIDTH,  // 142
    TFT_HEIGHT, // 428
    12,         // column offset 1
    0,          // row offset 1
    14,         // column offset 2
    0,          // row offset 2
    nv3007_279_init_operations,
    sizeof(nv3007_279_init_operations)));

/*
 * ------------------------------------------------------------
 * LVGL configuration
 * ------------------------------------------------------------
 */

static lv_disp_draw_buf_t draw_buf;

/*
 * 428 x 20 x RGB565
 *
 * Approximately:
 *
 *     428 * 20 * 2 = 17.1 KB
 *
 * Using partial rendering keeps RAM usage low.
 */
#define LVGL_BUFFER_LINES 20

static lv_color_t lv_buf1[SCREEN_WIDTH * LVGL_BUFFER_LINES];
static lv_color_t lv_buf2[SCREEN_WIDTH * LVGL_BUFFER_LINES];

/*
 * ------------------------------------------------------------
 * Display flush
 * ------------------------------------------------------------
 */

static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width = (area->x2 - area->x1 + 1);
    uint32_t height = (area->y2 - area->y1 + 1);

    /*
     * LVGL RGB565 byte order
     *
     * LV_COLOR_16_SWAP = 0
     *
     * Therefore use draw16bitRGBBitmap().
     */
#if LV_COLOR_16_SWAP != 0
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
#endif

    lv_disp_flush_ready(disp_drv);
}

/*
 * ------------------------------------------------------------
 * FreeRTOS helpers
 * ------------------------------------------------------------
 */

/* Destructively drain a FreeRTOS queue, invoking fn(value) for each item.
   The template lets the caller keep the queue element type private. */
template <typename T, typename Fn>
static void drainQueue(QueueHandle_t q, Fn &&fn)
{
    if (q == nullptr)
        return;
    T v;
    while (xQueueReceive(q, &v, 0) == pdTRUE)
    {
        fn(v);
    }
}

/*
 * ------------------------------------------------------------
 * KeyScan — minimal viable version
 * ------------------------------------------------------------
 *
 * 单独的 FreeRTOS 任务跑 1ms 周期的矩阵扫描，
 * 事件直接通过 Serial 打印，方便硬件验证。
 */

static KeyScanConfig g_keyCfg{};
static MatrixScanner g_matrix{g_keyCfg};
static RotaryEncoder g_encoder;
/* Polled in order every cycle. Initializer-list ctor requires implicit
   base-class pointer conversion, which std::initializer_list<T*> does not
   perform — build the vector element-by-element instead. */
static std::vector<IKeySource *> g_sources{static_cast<IKeySource *>(&g_matrix),
                                           static_cast<IKeySource *>(&g_encoder)};
static KeyEventList g_eventBuf;
static TaskHandle_t g_scanTaskHandle = nullptr;

/* keyId=2 (ROW0/COL1) 的"按下"事件从 keyscan 任务传给主循环。
   volatile bool 即可，1ms 周期下 set/clear 自然同步。 */
static volatile bool g_toggleRgbFlag = false;

/* Scan task pushes the currently-pressed key id (0 = none) to this queue.
   The UI loop drains it to refresh the on-screen label.
   容量 4 足够 —— 同一时刻最多一个键按下，1ms tick 下基本只装 1 条。 */
static QueueHandle_t g_pressedKeyQueue = nullptr;

/* 编码器事件专用队列：UI 单独消费 → 显示 "ENC: +1" / "ENC: click" 等。
   容量 8：旋转一帧可能产生 1-4 步；单击/双击互不同时。 */
struct EncoderUiMsg
{
    uint8_t kind; // 1 = rotate, 2 = click, 3 = double, 4 = long
    int8_t delta; // ±1 / kind=rotate 时有意义
};
static QueueHandle_t g_encoderQueue = nullptr;

static void scanTaskEntry(void * /*arg*/)
{
    for (auto *src : g_sources)
        src->begin();
    g_eventBuf.reserve(32);
    SERIAL_PRINTF("[KeyScan] task started, 1ms tick, 5ms debounce\n");

    uint8_t lastReportedKeyId = 0; // 0 = 无按键

    const TickType_t period = pdMS_TO_TICKS(SCAN_INTERVAL_MS);
    TickType_t lastWake = xTaskGetTickCount();
    while (true)
    {
        g_eventBuf.clear();
        for (auto *src : g_sources)
            src->poll(g_eventBuf);

        for (const auto &ev : g_eventBuf)
        {
            SERIAL_PRINTF("[KeyScan] %s\n", ev.toString().c_str());

            // keyId=2 的"按下"事件：用于切换 RGB 灯总开关
            if (ev.type == KeyEventType::Press && ev.keyId == 2)
            {
                g_toggleRgbFlag = true;
            }

            // 编码器事件另外入 UI 队列
            if (g_encoderQueue != nullptr)
            {
                EncoderUiMsg msg{};
                if (ev.type == KeyEventType::EncoderRotate)
                {
                    msg.kind = 1; // rotate
                    msg.delta = ev.encoderDelta;
                    xQueueSend(g_encoderQueue, &msg, 0);
                }
                else if (ev.type == KeyEventType::EncoderClick)
                {
                    // RotaryEncoder::poll() 中：
                    //   单击 → encoderDelta = 1
                    //   双击 → encoderDelta = 2
                    //   长按 → encoderDelta = 3
                    // 映射到 UI 协议的 2/3/4（1 留给 rotate）。
                    switch (ev.encoderDelta)
                    {
                    case 1:
                        msg.kind = 2;
                        break; // click
                    case 2:
                        msg.kind = 3;
                        break; // double
                    case 3:
                        msg.kind = 4;
                        break; // long
                    default:
                        continue; // 未知值，丢弃
                    }
                    msg.delta = 0;
                    xQueueSend(g_encoderQueue, &msg, 0);
                }
            }
        }

        /* 计算当前按下的键（按 keyId 升序取第一个）；只在变化时上报
           —— 避免每毫秒重复写队列导致 UI 反复重绘。 */
        const uint32_t mask = g_matrix.stableMask();
        uint8_t currentKeyId = 0;
        for (uint8_t k = 1; k <= KEY_NUM; ++k)
        {
            if (mask & (1UL << (k - 1)))
            {
                currentKeyId = k;
                break;
            }
        }
        if (currentKeyId != lastReportedKeyId && g_pressedKeyQueue != nullptr)
        {
            xQueueSend(g_pressedKeyQueue, &currentKeyId, 0);
            lastReportedKeyId = currentKeyId;
        }

        vTaskDelayUntil(&lastWake, period);
    }
}

static void startKeyScanTask()
{
    g_pressedKeyQueue = xQueueCreate(4, sizeof(uint8_t));
    if (g_pressedKeyQueue == nullptr)
    {
        halt("[KeyScan] failed to create pressed-key queue");
    }
    g_encoderQueue = xQueueCreate(8, sizeof(EncoderUiMsg));
    if (g_encoderQueue == nullptr)
    {
        halt("[KeyScan] failed to create encoder queue");
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        scanTaskEntry, "KeyScan", 4096, nullptr, 2, &g_scanTaskHandle, 0);
    if (ok != pdPASS)
    {
        halt("[KeyScan] failed to create scan task");
    }
}

/*
 * ------------------------------------------------------------
 * LVGL display initialization
 * ------------------------------------------------------------
 */

static bool lvgl_display_init()
{
    /* Initialize draw buffers */
    lv_disp_draw_buf_init(&draw_buf, lv_buf1, lv_buf2, SCREEN_WIDTH * LVGL_BUFFER_LINES);

    /* Initialize display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    /* Logical resolution */
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;

    /* Partial rendering buffer */
    disp_drv.draw_buf = &draw_buf;

    /* Flush callback */
    disp_drv.flush_cb = my_disp_flush;

    /* Register LVGL display */
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    if (disp == NULL)
    {
        halt("lv_disp_drv_register() returned NULL");
    }
    return true;
}

/*
 * ------------------------------------------------------------
 * Create LVGL UI
 * ------------------------------------------------------------
 */

static void create_ui()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), LV_PART_MAIN);

    /*
     * 屏幕: SCREEN_WIDTH × SCREEN_HEIGHT (横屏 428 × 142)
     * 4 行标签全部用 LV_ALIGN_LEFT_MID 相对屏幕垂直中线均匀分布，
     * 边距固定 PAD_X = 8，Y 偏移按行数算：
     *   行 0 (Title): -3*ROW_H/2
     *   行 1 (KEY):  -1*ROW_H/2
     *   行 2 (ENC):  +1*ROW_H/2
     *   行 3 (RGB):  +3*ROW_H/2
     * 改分辨率/字体时只要改 ROW_H，整个布局自动适应。
     */
    constexpr int16_t PAD_X = 8;
    constexpr int16_t ROW_H = 28; // 行间距 = 字体高 + 8 padding
    const int16_t y_title = -(3 * ROW_H) / 2;
    const int16_t y_key = -(1 * ROW_H) / 2;
    const int16_t y_enc = +(1 * ROW_H) / 2;
    const int16_t y_rgb = +(3 * ROW_H) / 2;

    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "EKeys");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, PAD_X, y_title);

    /* Currently pressed key: "--" means idle, otherwise "KEY<n>".
       Updated from the keyscan task via a FreeRTOS queue. */
    lv_obj_t *key_label = lv_label_create(screen);
    lv_label_set_text(key_label, "KEY: --");
    lv_obj_set_style_text_color(key_label, lv_color_hex(0x00FFCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(key_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(key_label, LV_ALIGN_LEFT_MID, PAD_X, y_key);

    /* Encoder status: shows last rotate/click event; idle = "--" */
    lv_obj_t *enc_label = lv_label_create(screen);
    lv_label_set_text(enc_label, "ENC: --");
    lv_obj_set_style_text_color(enc_label, lv_color_hex(0xFFCC00), LV_PART_MAIN);
    lv_obj_set_style_text_font(enc_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(enc_label, LV_ALIGN_LEFT_MID, PAD_X, y_enc);

    /* RGB status: 当前灯效名。Off=灯全灭。 */
    lv_obj_t *rgb_label = lv_label_create(screen);
    lv_label_set_text(rgb_label, "RGB: Off");
    lv_obj_set_style_text_color(rgb_label, lv_color_hex(0xFF66CC), LV_PART_MAIN);
    lv_obj_set_style_text_font(rgb_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(rgb_label, LV_ALIGN_LEFT_MID, PAD_X, y_rgb);

    g_key_label = key_label;
    g_encoder_label = enc_label;
    g_rgb_label = rgb_label;
}

/*
 * ------------------------------------------------------------
 * Arduino setup
 * ------------------------------------------------------------
 */

void setup()
{
    Serial.begin(115200);
    delay(500);

    SERIAL_PRINTF("\n=== EKeys ===\n");

    /* Backlight (active-LOW module: LOW = on, HIGH = off) */
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    if (!gfx->begin(SPI_FAST_HZ))
    {
        halt("gfx->begin() failed");
    }

    /* Initialize LVGL */
    lv_init();
    if (!lvgl_display_init())
    {
        halt("LVGL display init failed");
    }
    create_ui();

    SERIAL_PRINTF("LVGL initialized.\n");

    g_rgb.begin(); // 初始化 FastLED，初始 Off
    SERIAL_PRINTF("[RGB] init: NUM_LEDS=%d, pin=%d\n", NUM_LEDS, LED_PIN);

    startKeyScanTask();
}

/*
 * ------------------------------------------------------------
 * Arduino loop
 * ------------------------------------------------------------
 */

/* Last time the encoder label received an update. Used to clear stale
   text back to "--" after 1500 ms of inactivity. */
static constexpr uint32_t ENC_LABEL_TIMEOUT_MS = 1500;

/* RGB 灯效 tick 节流：动态灯效（Rainbow/Wave/Pulse）每 ~30ms 推进一帧 */
static constexpr uint32_t RGB_TICK_MS = 30;

void loop()
{
    static uint32_t last_ms = millis();
    static uint32_t last_enc_ms = 0;
    static uint32_t last_rgb_tick_ms = 0;

    const uint32_t now = millis();
    if (now != last_ms)
    {
        lv_tick_inc(now - last_ms);
        last_ms = now;
    }

    /* ---- keyId=2 切换 RGB 总开关 ---- */
    if (g_toggleRgbFlag)
    {
        g_toggleRgbFlag = false;
        g_rgb.setEnabled(!g_rgb.isEnabled());
    }

    /* ---- 推进 RGB 动态灯效 ---- */
    if (now - last_rgb_tick_ms >= RGB_TICK_MS)
    {
        last_rgb_tick_ms = now;
        g_rgb.tick();
    }

    /* Drain pressed-key queue: only the last value matters (边沿去重) */
    if (g_key_label != NULL)
    {
        uint8_t latestKeyId = UINT8_MAX; // sentinel = "no update this tick"
        drainQueue<uint8_t>(g_pressedKeyQueue,
                            [&latestKeyId](uint8_t v)
                            { latestKeyId = v; });
        if (latestKeyId != UINT8_MAX)
        {
            if (latestKeyId == 0)
            {
                lv_label_set_text(g_key_label, "KEY: --");
            }
            else
            {
                lv_label_set_text_fmt(g_key_label, "KEY: KEY%u", latestKeyId);
            }
        }
    }

    /* Drain encoder queue: keep the latest event's timestamp for timeout.
       同时处理 rotate(切灯效) 和 click(未使用) 等。 */
    if (g_encoder_label != NULL)
    {
        drainQueue<EncoderUiMsg>(g_encoderQueue, [&](const EncoderUiMsg &msg)
                                 {
            last_enc_ms = now;
            switch (msg.kind)
            {
                case 1: // rotate: 切灯效
                    if (msg.delta > 0) g_rgb.cycleEffect(+1);
                    else               g_rgb.cycleEffect(-1);
                    /* 切完立刻刷一次静态灯效标签 */
                    if (g_rgb_label != NULL) {
                        lv_label_set_text_fmt(g_rgb_label, "RGB: %s%s",
                                              RGBLightControl::effectName(g_rgb.currentEffect()),
                                              g_rgb.isEnabled() ? "" : " (off)");
                    }
                    lv_label_set_text_fmt(g_encoder_label, "ENC: %+d", msg.delta);
                    break;
                case 2: lv_label_set_text(g_encoder_label, "ENC: click");               break;
                case 3: lv_label_set_text(g_encoder_label, "ENC: double");              break;
                case 4: lv_label_set_text(g_encoder_label, "ENC: long");                break;
            } });
        /* 超时未收到事件 → 清除标签 */
        if (last_enc_ms != 0 && (now - last_enc_ms) >= ENC_LABEL_TIMEOUT_MS)
        {
            lv_label_set_text(g_encoder_label, "ENC: --");
            last_enc_ms = 0;
        }
    }

    /* 灯效使能状态/灯效名变化时刷新标签（仅在每帧最多刷一次） */
    if (g_rgb_label != NULL)
    {
        static bool prevEnabled = false;
        static LightEffect prevEffect = LightEffect::Off;
        if (g_rgb.isEnabled() != prevEnabled ||
            g_rgb.currentEffect() != prevEffect)
        {
            prevEnabled = g_rgb.isEnabled();
            prevEffect = g_rgb.currentEffect();
            lv_label_set_text_fmt(g_rgb_label, "RGB: %s%s",
                                  RGBLightControl::effectName(prevEffect),
                                  prevEnabled ? "" : " (off)");
        }
    }

    lv_timer_handler();
    delay(5);
}