#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "keyscan/KeyScanConfig.h"
#include "keyscan/KeyEvent.h"
#include "keyscan/MatrixScanner.h"

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

/*
 * Fatal error: print once, then halt silently so the message is not
 * drowned out by repeated output.
 */
static void halt(const char *msg)
{
    Serial.printf("[FATAL] %s\n", msg);
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
 * KeyScan — minimal viable version
 * ------------------------------------------------------------
 *
 * 单独的 FreeRTOS 任务跑 1ms 周期的矩阵扫描，
 * 事件直接通过 Serial 打印，方便硬件验证。
 */

static KeyScanConfig g_keyCfg{};
static MatrixScanner g_matrix{g_keyCfg};
static KeyEventList g_eventBuf;
static TaskHandle_t g_scanTaskHandle = nullptr;

/* Scan task pushes the currently-pressed key id (0 = none) to this queue.
   The UI loop drains it to refresh the on-screen label.
   容量 4 足够 —— 同一时刻最多一个键按下，1ms tick 下基本只装 1 条。 */
static QueueHandle_t g_pressedKeyQueue = nullptr;

static void scanTaskEntry(void * /*arg*/)
{
    g_matrix.begin();
    g_eventBuf.reserve(32);
    Serial.println("[KeyScan] task started, 1ms tick, 5ms debounce");

    uint8_t lastReportedKeyId = 0; // 0 = 无按键

    const TickType_t period = pdMS_TO_TICKS(SCAN_INTERVAL_MS);
    TickType_t lastWake = xTaskGetTickCount();
    while (true)
    {
        g_eventBuf.clear();
        g_matrix.poll(g_eventBuf);
        for (const auto &ev : g_eventBuf)
        {
            Serial.printf("[KeyScan] %s\n", ev.toString().c_str());
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
            // 非阻塞写入；UI 队列满时丢弃，最坏情况 UI 显示会延迟一帧
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
        Serial.println("[KeyScan] FATAL: failed to create pressed-key queue");
        return;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        scanTaskEntry, "KeyScan", 4096, nullptr, 2, &g_scanTaskHandle, 0);
    if (ok != pdPASS)
    {
        Serial.println("[KeyScan] FATAL: failed to create scan task");
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

    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "EKeys");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 15, 10);

    /* Currently pressed key: "--" means idle, otherwise "KEY<n>".
       Updated from the keyscan task via a FreeRTOS queue. */
    lv_obj_t *key_label = lv_label_create(screen);
    lv_label_set_text(key_label, "KEY: --");
    lv_obj_set_style_text_color(key_label, lv_color_hex(0x00FFCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(key_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(key_label, LV_ALIGN_TOP_LEFT, 15, 50);

    g_key_label = key_label;
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

    Serial.println();
    Serial.println("==============================");
    Serial.println("EKeys");
    Serial.println("==============================");

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

    Serial.println("LVGL initialized.");
    startKeyScanTask();
}

/*
 * ------------------------------------------------------------
 * Arduino loop
 * ------------------------------------------------------------
 */

void loop()
{
    static uint32_t last_ms = millis();
    uint32_t now = millis();

    if (now != last_ms)
    {
        lv_tick_inc(now - last_ms);
        last_ms = now;
    }

    /* Drain the keyscan queue: update the "currently pressed key" label.
       队列里只装边沿事件，所以排空时永远显示最新状态。 */
    if (g_key_label != NULL && g_pressedKeyQueue != nullptr)
    {
        uint8_t latestKeyId = UINT8_MAX; // sentinel = "no update this tick"
        uint8_t v;
        while (xQueueReceive(g_pressedKeyQueue, &v, 0) == pdTRUE)
        {
            latestKeyId = v;
        }
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

    lv_timer_handler();
    delay(5);
}