#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "keyscan/KeyScanConfig.h"
#include "keyscan/KeyEvent.h"
#include "keyscan/IKeySource.h"
#include "keyscan/MatrixScanner.h"
#include "keyscan/RotaryEncoder.h"
#include "rgb/RGBLightControl.h"
#include "ble/BleKeyboardSink.h"
#include "ui/Pages.h"
#include "ui/PageManager.h"
#include "ui/MenuPage.h"
#include "ui/RgbPage.h"
#include "ui/TomatoPage.h"
#include "ui/StatusPage.h"
#include "ui/BlePage.h"

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

/* Serial 日志串行化。不能用 portENTER_CRITICAL（关中断）包住 Serial.printf：
 * uart 驱动在 TX FIFO 满时会等待 UART 中断释放信号量，关中断期间中断无法响应，
 * printf 永久阻塞 -> CPU 中断看门狗超时（Interrupt wdt timeout）。
 * 改用 FreeRTOS 互斥量：阻塞时可正常调度，UART 中断不受影响。
 * 先 vsnprintf 到栈缓冲再写入，保证单行原子且持锁时间短。 */
#include <stdarg.h>
#include <stdio.h>
static SemaphoreHandle_t g_serialMutex = nullptr;
static void serialPrintf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0)
        return;
    if (n >= (int)sizeof(buf))
        n = sizeof(buf) - 1;
    bool locked = (g_serialMutex != nullptr &&
                   xSemaphoreTake(g_serialMutex, pdMS_TO_TICKS(2000)) == pdTRUE);
    Serial.write((const uint8_t *)buf, (size_t)n);
    if (locked)
        xSemaphoreGive(g_serialMutex);
}
#define SERIAL_PRINTF(...) serialPrintf(__VA_ARGS__)

static void halt(const char *msg)
{
    SERIAL_PRINTF("[FATAL] %s\n", msg);
    Serial.flush();
    while (true)
    {
        delay(1000);
    }
}

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
 */
#define TFT_WIDTH 142
#define TFT_HEIGHT 428
/* 旋转 1 后：428 x 142，与 ui/Pages.h 里的 SCREEN_W_PX/SCREEN_H_PX 一致 */

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
#define LVGL_BUFFER_LINES 20
static lv_color_t lv_buf1[SCREEN_W_PX * LVGL_BUFFER_LINES];
static lv_color_t lv_buf2[SCREEN_W_PX * LVGL_BUFFER_LINES];

static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width = (area->x2 - area->x1 + 1);
    uint32_t height = (area->y2 - area->y1 + 1);
#if LV_COLOR_16_SWAP != 0
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
#endif
    lv_disp_flush_ready(disp_drv);
}

static bool lvgl_display_init()
{
    lv_disp_draw_buf_init(&draw_buf, lv_buf1, lv_buf2, SCREEN_W_PX * LVGL_BUFFER_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_W_PX;
    disp_drv.ver_res = SCREEN_H_PX;
    /* 必须手动挂上 draw_buf：lv_disp_drv_register() 不会自动赋值，
     * 漏掉会导致注册的驱动 draw_buf==NULL，首次刷新在
     * lv_refr.c:608 (draw_buf->last_area) 空指针崩溃 (LoadProhibited, EXCVADDR=0x18)。 */
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = my_disp_flush;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    if (disp == NULL)
    {
        halt("lv_disp_drv_register() returned NULL");
    }

    /* LVGL 默认把屏幕背景色硬编码为 lv_color_white()，
     * 主题 (LV_USE_THEME_DEFAULT) 也会给屏幕应用浅色背景。
     * 必须在 lv_disp_drv_register() 之后立即把屏幕背景改成深色，
     * 否则在没有其他对象覆盖的区域会出现全白屏幕。 */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    return true;
}

/*
 * ------------------------------------------------------------
 * PageManager + Pages
 * ------------------------------------------------------------
 */
static RGBLightControl g_rgb;
static BleKeyboardSink g_bleKbd;
static MenuPage g_menu;
static RgbPage g_rgb_page{g_rgb};
static TomatoPage g_tomato_page{g_rgb};
static StatusPage g_status_page{g_rgb};
static BlePage g_ble_page{g_bleKbd};

static PageManager g_pm;

static void registerAllPages()
{
    g_pm.registerPage(&g_menu);
    g_pm.registerPage(&g_rgb_page);
    g_pm.registerPage(&g_tomato_page);
    g_pm.registerPage(&g_status_page);
    g_pm.registerPage(&g_ble_page);
}

/*
 * ------------------------------------------------------------
 * FreeRTOS helpers
 * ------------------------------------------------------------
 */

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
 * 事件直接通过 Serial 打印，并按事件类型分发到 PageManager / RGB。
 */
static KeyScanConfig g_keyCfg{};
static MatrixScanner g_matrix{g_keyCfg};
static RotaryEncoder g_encoder;
static std::vector<IKeySource *> g_sources{static_cast<IKeySource *>(&g_matrix),
                                           static_cast<IKeySource *>(&g_encoder)};
static KeyEventList g_eventBuf;
static TaskHandle_t g_scanTaskHandle = nullptr;

/* 路由队列：scanTask 写入，main loop 消费 */
static QueueHandle_t g_keyPressQueue = nullptr;      // 按下：uint8_t keyId
static QueueHandle_t g_keyReleaseQueue = nullptr;    // 释放：uint8_t keyId
static QueueHandle_t g_encoderRotateQueue = nullptr; // 旋转：int8_t delta
static QueueHandle_t g_encoderClickQueue = nullptr;  // 单击：空消息（占位）
static QueueHandle_t g_encoderUiQueue = nullptr;     // 单击/双击/长按（统一 UI 用）

struct EncoderUiMsg
{
    uint8_t kind; // 1=rotate, 2=click, 3=double, 4=long
    int8_t delta; // 仅 rotate
};

static void scanTaskEntry(void * /*arg*/)
{
    for (auto *src : g_sources)
        src->begin();
    g_eventBuf.reserve(32);
    SERIAL_PRINTF("[KeyScan] task started, 1ms tick, 5ms debounce\n");

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

            switch (ev.type)
            {
            case KeyEventType::Press:
                if (ev.keyId >= 1 && ev.keyId <= 9 && g_keyPressQueue != nullptr)
                {
                    uint8_t k = ev.keyId;
                    xQueueSend(g_keyPressQueue, &k, 0);
                }
                break;
            case KeyEventType::Release:
                if (ev.keyId >= 1 && ev.keyId <= 9 && g_keyReleaseQueue != nullptr)
                {
                    uint8_t k = ev.keyId;
                    xQueueSend(g_keyReleaseQueue, &k, 0);
                }
                break;
            case KeyEventType::EncoderRotate:
                if (g_encoderRotateQueue != nullptr)
                {
                    int8_t d = ev.encoderDelta;
                    xQueueSend(g_encoderRotateQueue, &d, 0);
                }
                if (g_encoderUiQueue != nullptr)
                {
                    EncoderUiMsg m{1, ev.encoderDelta};
                    xQueueSend(g_encoderUiQueue, &m, 0);
                }
                break;
            case KeyEventType::EncoderClick:
                if (g_encoderUiQueue != nullptr)
                {
                    EncoderUiMsg m{};
                    if (ev.encoderDelta == 1)
                        m.kind = 2;
                    else if (ev.encoderDelta == 2)
                        m.kind = 3;
                    else if (ev.encoderDelta == 3)
                        m.kind = 4;
                    xQueueSend(g_encoderUiQueue, &m, 0);
                }
                if (ev.encoderDelta == 1 && g_encoderClickQueue != nullptr)
                {
                    uint8_t dummy = 1;
                    xQueueSend(g_encoderClickQueue, &dummy, 0);
                }
                break;
            }
        }

        vTaskDelayUntil(&lastWake, period);
    }
}

static void startKeyScanTask()
{
    g_keyPressQueue = xQueueCreate(8, sizeof(uint8_t));
    g_keyReleaseQueue = xQueueCreate(8, sizeof(uint8_t));
    g_encoderRotateQueue = xQueueCreate(8, sizeof(int8_t));
    g_encoderClickQueue = xQueueCreate(4, sizeof(uint8_t));
    g_encoderUiQueue = xQueueCreate(16, sizeof(EncoderUiMsg));
    if (!g_keyPressQueue || !g_keyReleaseQueue ||
        !g_encoderRotateQueue || !g_encoderClickQueue ||
        !g_encoderUiQueue)
    {
        halt("[KeyScan] failed to create queues");
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
 * Main loop: 消费队列 → 路由到 PageManager → 推进 LVGL
 * ------------------------------------------------------------
 */

static constexpr uint32_t RGB_TICK_MS = 30;

void loop()
{
    static uint32_t last_ms = millis();
    static uint32_t last_rgb_tick_ms = 0;
    static uint8_t prev_pressed = 0;

    /* 旋钮单击/旋转 BLE 按键的"延后 release"标记。
     * 在 BLE HID 协议里，press 与 release 之间必须留出至少一个 report 周期
     * (~ms 级)，Host 才能识别为一次有效按放。否则可能被当成"按住不放"
     * 或干脆丢失。所以本帧 press、下一帧开头 release。
     * 注意：只 release 旋钮那次按下的单键，绝不调 releaseAll()，
     * 否则会清掉正在按的矩阵键（HID 报告里的其他键槽）—— 这是之前
     * "蓝牙和硬件按键冲突"的根因，已修。 */
    static bool s_pendingEncRelease = false;

    const uint32_t now = millis();
    if (now != last_ms)
    {
        lv_tick_inc(now - last_ms);
        last_ms = now;
    }

    /* ---- 帧开头：先把上一帧旋钮 BLE 按下的键 release ---- */
    if (s_pendingEncRelease)
    {
        g_bleKbd.encoderRelease();
        s_pendingEncRelease = false;
    }

    /* ---- 推进 RGB 动态灯效 ---- */
    if (now - last_rgb_tick_ms >= RGB_TICK_MS)
    {
        last_rgb_tick_ms = now;
        g_rgb.tick();
    }

    /* ---- 处理按键按下/释放 ----
     *
     * 多键并发：用本帧集合缓冲，避免 drainQueue 只保留最后一个 keyId。
     * - 按下：UI + BLE 都按
     * - 释放：BLE release
     * - 全部释放完 → 清 prev_pressed_，下一帧可重新触发
     */
    uint8_t pressed_keys[8];
    uint8_t pressed_n = 0;
    drainQueue<uint8_t>(g_keyPressQueue, [&](uint8_t k)
                        { if (pressed_n < 8) pressed_keys[pressed_n++] = k; });
    for (uint8_t i = 0; i < pressed_n; ++i)
    {
        uint8_t k = pressed_keys[i];
        if (k == prev_pressed)
            continue; // 防重复
        prev_pressed = k;
        g_pm.handleKeyPress(k);
        g_bleKbd.pressKey(k);
    }

    uint8_t released_keys[8];
    uint8_t released_n = 0;
    drainQueue<uint8_t>(g_keyReleaseQueue, [&](uint8_t k)
                        { if (released_n < 8) released_keys[released_n++] = k; });
    for (uint8_t i = 0; i < released_n; ++i)
    {
        uint8_t k = released_keys[i];
        g_bleKbd.releaseKey(k);
        if (k == prev_pressed)
            prev_pressed = 0;
    }

    /* ---- 处理旋钮事件 ----
     *
     * 旋转（rotate）：
     *   1) 调 page->onEncoder(d)，让 UI 决定如何响应（菜单切项 / 调参数）
     *   2) 若页面 consumesEncoder()=false（Menu/Status/BLE），
     *      额外发 BLE 方向键（顺时针→Right / 逆时针→Left）作为"前进/后退"。
     * 单击/双击/长按：
     *   BLE 上报 Enter/Esc/Tab（仅 BLE，不绑 UI 动作）。
     * 所有旋钮 BLE 按键都走"下一帧 release"，避免与矩阵键冲突 + 保证 HID 时序。 */
    drainQueue<int8_t>(g_encoderRotateQueue, [&](int8_t d)
                       {
        /* 1) UI 路由（切菜单 / 调参数 / 纯展示页面 no-op） */
        g_pm.handleEncoderRotate(d);
        /* 2) 若当前页面"不消费"旋转（MenuPage/StatusPage/BlePage），
         *    同步发 BLE 方向键（前进/后退）。 */
        Page *p = g_pm.current();
        if (p != nullptr && !p->consumesEncoder())
        {
            g_bleKbd.encoderRotate(d);
            s_pendingEncRelease = true; // 下一帧释放
        } });
    drainQueue<EncoderUiMsg>(g_encoderUiQueue, [&](const EncoderUiMsg &m)
                             {
        /* 单击/双击/长按 → BLE 上报对应键（Enter/Esc/Tab） */
        if (m.kind >= 2 && m.kind <= 4) {
            g_bleKbd.encoderClick((int8_t)(m.kind - 1)); // 2→1, 3→2, 4→3
            s_pendingEncRelease = true; // 下一帧释放（不 releaseAll！）
        }
        /* UI 动作仍然不绑定（与之前设计一致） */ });

    /* ---- 当前页面每帧 service tick ---- */
    if (Page *p = g_pm.current())
    {
        if (p->id() == PAGE_TOMATO)
        {
            g_tomato_page.serviceTick();
        }
        else if (p->id() == PAGE_STATUS)
        {
            g_status_page.serviceTick();
        }
        else if (p->id() == PAGE_BLE)
        {
            g_ble_page.serviceTick();
        }
    }

    lv_timer_handler();
    g_bleKbd.tick();
    delay(5);
}

/*
 * ------------------------------------------------------------
 * Arduino setup
 * ------------------------------------------------------------
 */
void setup()
{
    g_serialMutex = xSemaphoreCreateMutex();
    Serial.begin(115200);
    delay(500);

    SERIAL_PRINTF("\n=== EKeys ===\n");

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    if (!gfx->begin(SPI_FAST_HZ))
    {
        halt("gfx->begin() failed");
    }

    lv_init();
    if (!lvgl_display_init())
    {
        halt("LVGL display init failed");
    }

    g_rgb.begin();
    SERIAL_PRINTF("[RGB] init: NUM_LEDS=%d, pin=%d\n", NUM_LEDS, LED_PIN);

    g_bleKbd.begin();
#if EKEYS_ENABLE_BLE
    SERIAL_PRINTF("[BLE] enabled, advertising as '%s'\n", EKEYS_DEVICE_NAME);
#else
    SERIAL_PRINTF("[BLE] disabled (EKEYS_ENABLE_BLE=0)\n");
#endif

    g_pm.begin();
    registerAllPages();
    g_pm.push(PAGE_MENU);
    SERIAL_PRINTF("[UI] pages registered, started at MENU\n");

    startKeyScanTask();
}