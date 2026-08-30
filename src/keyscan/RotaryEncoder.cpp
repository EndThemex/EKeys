#include "RotaryEncoder.h"
#include "KeyScanConfig.h"
#include <ESP32Encoder.h>
#include <OneButton.h>

namespace ekeys {

// 静态实例（仅本文件内可见）。EC11 不需要多实例。
static ESP32Encoder g_encoder;
// 用指针而不是值：OneButton 会在构造时立即 pinMode()，那时 GPIO 还没
// ready；用指针延后到 begin() 里再 new。OneButton 库没有 reset()/setup()
// 之外的"换引脚"接口，所以一次性配置比"默认构造 + 赋值"更安全。
static OneButton   *g_button = nullptr;

// 按钮事件标志位：OneButton 回调（ISR 时间）置位，poll() 读取并清空。
// 三个标志位互斥（一次按下最多触发一种），用 volatile bool 即可。
static volatile bool g_clickFlag    = false;
static volatile bool g_doubleFlag   = false;
static volatile bool g_longPressFlag = false;

void RotaryEncoder::begin()
{
    const auto cfg = KeyScanConfig{};

    if (cfg.encoderAPin < 0 || cfg.encoderBPin < 0)
    {
        Serial.println("[Encoder] disabled: pins not configured");
        return;
    }

    if (ESP.getFreeHeap() < 8 * 1024)
    {
        Serial.println("[Encoder] FATAL: heap too low, encoder disabled");
        return;
    }

    /* --- 旋转：A/B 用 PCNT 硬件计数器 ---
     *
     * 用 attachSingleEdge（A 相上升沿 only）而不是 attachHalfQuad：
     * EC11 一个机械档位会产生 2 个 A 相边沿，halfQuad 会计 2 步，
     * 导致 poll() 发出 2 个 EncoderRotate 事件、UI 切 2 个灯效。
     * singleEdge 严格 1 步 = 1 个 UI 事件 = 1 次灯效切换。 */
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    g_encoder.attachSingleEdge(cfg.encoderAPin, cfg.encoderBPin);
    g_encoder.setFilter(1023);
    g_encoder.setCount(0);
    lastCount_ = 0;

    /* --- 按钮：OneButton 内部上拉 --- */
    g_button = new OneButton(cfg.encoderSWPin, /*activeLow=*/true);
    g_button->attachClick([]() { g_clickFlag    = true; });
    g_button->attachDoubleClick([]() { g_doubleFlag = true; });
    g_button->attachLongPressStart([]() { g_longPressFlag = true; });

    Serial.printf("[Encoder] ready: CLK=%d DT=%d SW=%d\n",
                  cfg.encoderAPin, cfg.encoderBPin, cfg.encoderSWPin);
}

void RotaryEncoder::tick()
{
    if (g_button != nullptr) g_button->tick();
}

void RotaryEncoder::poll(KeyEventList& out)
{
    const auto cfg = KeyScanConfig{};
    if (cfg.encoderAPin < 0) return;

    tick();

    const uint32_t now = millis();

    /* --- 旋转边沿：检测 PCNT 计数变化，按步数生成 1 个或多个事件
     *
     * 一次 poll 之间可能累计多步（如快速旋转），必须把每一步都发出，
     * 否则 UI 显示会丢。lastCount_ 在循环里逐次前进来避免重复发。 */
    int32_t dlt = g_encoder.getCount() - lastCount_;
    while (dlt != 0)
    {
        KeyEvent ev{};
        ev.type         = KeyEventType::EncoderRotate;
        ev.keyId        = ROTARY_KEY_ID;
        ev.encoderDelta = (dlt > 0) ? 1 : -1;
        ev.timestamp_ms = now;
        out.push_back(ev);

        lastCount_ += ev.encoderDelta;
        dlt = g_encoder.getCount() - lastCount_;
    }

    /* --- 按钮事件（OneButton 回调置位，poll 取出并清除） --- */
    if (g_clickFlag)
    {
        g_clickFlag = false;
        KeyEvent ev{};
        ev.type         = KeyEventType::EncoderClick;
        ev.keyId        = ROTARY_KEY_ID;
        ev.encoderDelta = 1;        // 1 = single click
        ev.timestamp_ms = now;
        out.push_back(ev);
    }
    if (g_doubleFlag)
    {
        g_doubleFlag = false;
        KeyEvent ev{};
        ev.type         = KeyEventType::EncoderClick;
        ev.keyId        = ROTARY_KEY_ID;
        ev.encoderDelta = 2;        // 2 = double click
        ev.timestamp_ms = now;
        out.push_back(ev);
    }
    if (g_longPressFlag)
    {
        g_longPressFlag = false;
        KeyEvent ev{};
        ev.type         = KeyEventType::EncoderClick;
        ev.keyId        = ROTARY_KEY_ID;
        ev.encoderDelta = 3;        // 3 = long press start
        ev.timestamp_ms = now;
        out.push_back(ev);
    }

    /* 诊断：每 200ms 打印一次 SW 引脚电平，便于在没有 click 事件时
       区分"硬件没按" vs "OneButton 没识别"。 */
    static uint32_t lastDbgMs = 0;
    if (cfg.encoderSWPin >= 0 && (now - lastDbgMs) >= 200)
    {
        lastDbgMs = now;
        int level = digitalRead(cfg.encoderSWPin);
        Serial.printf("[EncDiag] SW=%d level=%d\n",
                      cfg.encoderSWPin, level);
    }
}

}  // namespace ekeys
