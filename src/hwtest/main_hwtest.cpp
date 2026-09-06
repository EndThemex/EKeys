/*
 * main_hwtest.cpp
 *
 * 硬件测试程序入口（[env:hwtest]）。
 *
 *   - 5V 升压使能 (IO3) → 背光 → 屏幕 → 各驱动 begin
 *   - 启动矩阵扫描 + 旋钮循环
 *   - HwTestMenu 状态机入口
 *
 * 烧录：
 *     pio run -e hwtest -t upload
 *
 * 烧回主固件：
 *     pio run -e esp32-s3-wroom-1-n16r8 -t upload
 */

#include <Arduino.h>

#include "HwTestMenu.h"
#include "display/Backlight.h"
#include "display/DisplayDriver.h"
#include "input/MatrixScanner.h"
#include "input/RotaryEncoder.h"
#include "logging/LogManager.h"
#include "rgb/RGBDriver.h" // T2 Keys 按键亮灯反馈依赖（非 RGB 测试项）
#include "hardware/PinMap.h"

namespace
{
    constexpr uint32_t kScanPeriodMs = 5;
    constexpr uint32_t kBootSerialDelayMs = 200;

    ekeys::MatrixScanner *g_matrix = nullptr;
    ekeys::RotaryEncoder *g_rotary = nullptr;

    // 旋钮事件 FIFO（避免同一帧 click/ESC 互相覆盖）
    constexpr uint8_t kKeyQueueSize = 16;
    volatile uint8_t g_key_queue[kKeyQueueSize];
    volatile uint8_t g_key_head = 0;
    volatile uint8_t g_key_tail = 0;
}

namespace ekeys
{
    namespace hwtest
    {
        MatrixScanner *getMatrixScanner() { return g_matrix; }
    } // namespace hwtest
} // namespace ekeys

static void enqueueKey(uint8_t key)
{
    uint8_t next = (g_key_head + 1) % kKeyQueueSize;
    if (next != g_key_tail)
    {
        g_key_queue[g_key_head] = key;
        g_key_head = next;
    }
}

static uint8_t dequeueKey()
{
    if (g_key_head == g_key_tail)
    {
        return 0xFF;
    }
    uint8_t k = g_key_queue[g_key_tail];
    g_key_tail = (g_key_tail + 1) % kKeyQueueSize;
    return k;
}

static void onEncoderCallback(uint8_t key)
{
    // 中断上下文：只入队，不打 Serial（避免 UART 锁死）
    enqueueKey(key);
}

// UART0 镜像串口（避开 USB CDC：避免占用 USB 通道）
//   RXD0 = GPIO44 (input)
//   TXD0 = GPIO43 (output)
#define HWTEST_LOG_SERIAL Serial1

// 把 LogManager 默认的 Serial 输出重定向到 UART0。
// （保留 LogManager 生产实现不动；这里通过 Arduino 全局 Serial 引用 =
//  替换 Serial.begin 的对象不可行，改为测试程序自身用 HWTEST_LOG_SERIAL 打印，
//  并通过 Sink 把 LOG_INFO 也镜像到 UART0。）
static ekeys::LogManager::Sink g_prev_sink = nullptr;

static void uartSink(ekeys::LogLevel level, const char *tag, const char *message)
{
    const char *p = "?";
    switch (level)
    {
        case ekeys::LogLevel::Debug:   p = "D"; break;
        case ekeys::LogLevel::Info:    p = "I"; break;
        case ekeys::LogLevel::Warning: p = "W"; break;
        case ekeys::LogLevel::Error:   p = "E"; break;
    }
    HWTEST_LOG_SERIAL.print('[');
    HWTEST_LOG_SERIAL.print(p);
    HWTEST_LOG_SERIAL.print(']');
    HWTEST_LOG_SERIAL.print(tag ? tag : "?");
    HWTEST_LOG_SERIAL.print(": ");
    HWTEST_LOG_SERIAL.println(message);
}

void setup()
{
    HWTEST_LOG_SERIAL.begin(115200, SERIAL_8N1, 44, 43);
    delay(kBootSerialDelayMs);
    // 测试期间把 LOG_INFO 镜像到 UART0；不触碰 USB CDC（Serial）
    // 默认 LogManager 仍会输出到 Serial（USB CDC），sink 额外走 UART0
    ekeys::LogManager::setSink(uartSink);

    LOG_INFO("HWT", "===== EKeys hardware test (env:hwtest) =====");

    /* 1. 5V 升压使能（RGB / 功放等 5V 负载供电） */
    pinMode(kPinBoost5VEnable, OUTPUT);
    digitalWrite(kPinBoost5VEnable, HIGH);

    /* 2. 背光 */
    ekeys::Backlight::instance().begin();

    /* 3. 屏幕 */
    if (!ekeys::DisplayDriver::instance().begin(40000000))
    {
        LOG_ERROR("HWT", "display begin failed");
        while (true)
        {
            delay(1000);
        }
    }
    ekeys::DisplayDriver::instance().fillScreen(RGB565_BLACK);

    /* 4. 矩阵按键 */
    static ekeys::MatrixScanner matrix;
    matrix.begin();
    g_matrix = &matrix;

    /* 5. 旋钮 */
    static ekeys::RotaryEncoder rotary;
    rotary.setCallback(onEncoderCallback);
    rotary.begin();
    g_rotary = &rotary;
    LOG_INFO("HWT", "rotary pins: SW=%d A=%d B=%d", kPinEc11Sw, kPinEc11A, kPinEc11B);

    /* 6. RGB（默认不上电，避免开机一瞬间误亮；测试项各自使能） */
    ekeys::RGBDriver::instance().begin();

    /* 7. 菜单状态机 */
    ekeys::hwtest::HwTestMenu::instance().begin(
        ekeys::DisplayDriver::instance().gfx());

    LOG_INFO("HWT", "setup ok");
}

void loop()
{
    /* 持续喂矩阵扫描（5ms 节流） */
    static uint32_t last_scan = 0;
    uint32_t now = millis();
    if (now - last_scan >= kScanPeriodMs)
    {
        last_scan = now;
        if (g_matrix != nullptr)
        {
            g_matrix->scan();
        }
    }

    /* 旋钮 loop（OneButton tick + 旋转判定） */
    if (g_rotary != nullptr)
    {
        g_rotary->loop();
    }

    /* 主状态机 tick：每轮主循环都调用。
 * key=KEY_NONE 时测试项 loop() 持续刷新（T2 按键消费/T1 刷新/T4 录音推进/T5 喂数据）；
 * 有事件时 menu 先处理 ROT_ESC 等再转发给测试项。 */
    uint8_t key = dequeueKey();
    if (key != ekeys::hwtest::KEY_NONE)
    {
        HWTEST_LOG_SERIAL.print("[HWT] deq key=0x");
        HWTEST_LOG_SERIAL.println(key, HEX);
    }
    ekeys::hwtest::HwTestMenu::instance().tick(key);

    delay(1);

    static uint32_t last_heartbeat = 0;
    static uint32_t tick_count = 0;
    tick_count++;
    if (now - last_heartbeat > 5000)
    {
        last_heartbeat = now;
        LOG_INFO("HWT", "heartbeat tick=%lu q[h=%u t=%u]",
                 (unsigned long)tick_count, g_key_head, g_key_tail);
    }
}