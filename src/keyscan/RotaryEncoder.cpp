#include "RotaryEncoder.h"
#include "KeyScanConfig.h"
#include <ESP32Encoder.h>
#include <OneButton.h>

namespace ekeys
{

    // 静态实例（仅本文件内可见）。EC11 不需要多实例。
    static ESP32Encoder g_encoder;
    // 用指针而不是值：OneButton 会在构造时立即 pinMode()，那时 GPIO 还没
    // ready；用指针延后到 begin() 里再 new。OneButton 库没有 reset()/setup()
    // 之外的"换引脚"接口，所以一次性配置比"默认构造 + 赋值"更安全。
    static OneButton *g_button = nullptr;

    /* 文件内可见的事件层（L2）辅助：尝试发一个旋转事件，返回是否成功。
     *
     * 返回 false 的两种情况：
     *   - 同向合并：上次事件距今 < ENC_MIN_STEP_MS，且方向相同 → 吞掉（合并）。
     *   - 反向静默：上次事件距今 < ENC_QUIET_MS，且方向相反 → 吞掉（去抖）。
     *
     * 成功路径会更新 lastEmitDir_ / lastEmitMs_ 并把事件追加到 out。
     * 这是 RotaryEncoder 的私有成员，能直接访问 lastEmitDir_ / lastEmitMs_。 */
    bool RotaryEncoder::emitRotateEvent(KeyEventList &out, int8_t dir, uint32_t now)
    {
        if (dir == 0)
            return false;
        if (lastEmitDir_ != 0)
        {
            uint32_t dt = now - lastEmitMs_;
            if (dir == lastEmitDir_ && dt < ENC_MIN_STEP_MS)
            {
                /* 同向太密 → 合并：吞掉本次 */
                return false;
            }
            if (dir != lastEmitDir_ && dt < ENC_QUIET_MS)
            {
                /* 反向静默期太短 → 当成抖动：吞掉 */
                return false;
            }
        }

        KeyEvent ev{};
        ev.type = KeyEventType::EncoderRotate;
        ev.keyId = ROTARY_KEY_ID;
        ev.encoderDelta = dir;
        ev.timestamp_ms = now;
        out.push_back(ev);
        lastEmitDir_ = dir;
        lastEmitMs_ = now;
        return true;
    }

    // 按钮事件标志位：OneButton 回调（ISR 时间）置位，poll() 读取并清空。
    // 三个标志位互斥（一次按下最多触发一种），用 volatile bool 即可。
    static volatile bool g_clickFlag = false;
    static volatile bool g_doubleFlag = false;
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
        /* 单击/双击超时窗口：默认 400ms 把"单击"压到 ~150ms，
         * 让单次按下感觉更跟手；双击仍可在 150ms 内连击两次识别。
         * 长按阈值仍保留默认 800ms。 */
        g_button->setClickTicks(2);   // 2 × tickMs(默认 50ms) ≈ 100~150ms
        g_button->attachClick([]()
                              { g_clickFlag = true; });
        g_button->attachDoubleClick([]()
                                    { g_doubleFlag = true; });
        g_button->attachLongPressStart([]()
                                       { g_longPressFlag = true; });

        Serial.printf("[Encoder] ready: CLK=%d DT=%d SW=%d\n",
                      cfg.encoderAPin, cfg.encoderBPin, cfg.encoderSWPin);
    }

    void RotaryEncoder::tick()
    {
        if (g_button != nullptr)
            g_button->tick();
    }

    void RotaryEncoder::poll(KeyEventList &out)
    {
        const auto cfg = KeyScanConfig{};
        if (cfg.encoderAPin < 0)
            return;

        tick();

        const uint32_t now = millis();

        /* --- 旋转边沿：检测 PCNT 计数变化，按步数生成 1 个或多个事件
         *
         * 两层滤波：
         *   L1 信号层：每个 poll 看到的 PCNT 净步数若 ≥ ENC_BURST_THRESHOLD
         *              才视为"一次有效旋转"；否则先存到 pending_ 窗口继续观察。
         *   L2 事件层：相邻两次发出的旋转事件间隔 < ENC_MIN_STEP_MS 视为同一次
         *              动作（避免 BLE/UI 抖率突高）；反向抖动若距上次 ≥ ENC_QUIET_MS
         *              视为新一次旋转（用 ENC_QUIET_MS > DEBOUNCE_MS 保证能
         *              覆盖慢来回的跨 poll 抖动）。
         *
         * 实现要点：
         *   - lastCount_ 永远逐次推进，绝不"跳过" PCNT 计数；
         *   - pending_ 窗口只在累积步数 < 阈值时存在，触发后立即清空；
         *   - 触发后只发 1 个 KeyEvent.delta=+1 或 -1（净方向），步数感交给
         *     下一帧的 BLE 输出层合并，不在源头丢失。 */
        int32_t dlt = g_encoder.getCount() - lastCount_;
        while (dlt != 0)
        {
            int8_t stepDir = (dlt > 0) ? 1 : -1;
            lastCount_ += stepDir;
            dlt = g_encoder.getCount() - lastCount_;

            /* —— L1 信号层：更新 pending 窗口 —— */
            if (pendingDir_ == 0)
            {
                pendingDir_ = stepDir;
                pendingStartMs_ = now;
                pendingSteps_ = stepDir;
            }
            else if (stepDir == pendingDir_)
            {
                pendingSteps_ += stepDir;
            }
            else
            {
                /* 窗口内出现反向步：可能是慢来回抖动。
                 * 若静默期内且净步数仍小 → 把反向那一步抵消掉（吃进负数），
                 * 不切换方向；若净步数已经够阈值 → 视为新一次旋转，
                 * 先 flush 上一次 pending（如果已够阈值）再开新窗口。 */
                int32_t absPending = pendingSteps_ >= 0 ? pendingSteps_ : -pendingSteps_;
                bool shouldFlushPending =
                    (absPending >= ENC_BURST_THRESHOLD) ||
                    ((now - pendingStartMs_) >= ENC_QUIET_MS);

                if (shouldFlushPending)
                {
                    if (emitRotateEvent(out, pendingDir_, now))
                    {
                        /* flush 成功后才允许切换方向（lastEmitDir_/Ms_ 已更新） */
                        pendingDir_ = stepDir;
                        pendingStartMs_ = now;
                        pendingSteps_ = stepDir;
                    }
                    else
                    {
                        /* L2 拒绝（同向合并 / 反向静默）：吞掉这次的 stepDir，
                         * pending 维持原方向累加器。 */
                        pendingSteps_ += stepDir; // 抵销
                        continue;
                    }
                }
                else
                {
                    /* 窗口还在观察中：反向步视为抖动，吃进 pendingSteps_ */
                    pendingSteps_ += stepDir; // 抵销
                    continue;
                }
            }

            /* —— L1 阈值检查：净步数 ≥ 阈值才发事件 —— */
            int32_t absPending = pendingSteps_ >= 0 ? pendingSteps_ : -pendingSteps_;
            bool reachedThreshold = (absPending >= ENC_BURST_THRESHOLD);
            bool windowTimedOut = ((now - pendingStartMs_) >= ENC_QUIET_MS);

            if (reachedThreshold || windowTimedOut)
            {
                if (emitRotateEvent(out, pendingDir_, now))
                {
                    /* 成功后清 pending。残留步数（如 3 步事件只发 1 次，
                     * 下次再触发会按 1 步算）保留在 lastCount_ 里就够了。 */
                    pendingDir_ = 0;
                    pendingSteps_ = 0;
                }
                else
                {
                    /* L2 拒绝（最小间隔未到）：保留 pending，等下一帧再试。
                     * 注意 lastCount_ 已经吃掉这步，但 pendingSteps_ 没消耗，
                     * 所以下一帧累积值不变，相当于"延迟发布"。 */
                }
            }
        }

        /* —— L2 兜底：pending 窗口超时未触发阈值 → 强制 flush —— */
        if (pendingDir_ != 0 && (now - pendingStartMs_) >= ENC_QUIET_MS)
        {
            int32_t absPending = pendingSteps_ >= 0 ? pendingSteps_ : -pendingSteps_;
            if (absPending > 0)
            {
                emitRotateEvent(out, pendingDir_, now);
                pendingDir_ = 0;
                pendingSteps_ = 0;
            }
        }

        /* --- 按钮事件（OneButton 回调置位，poll 取出并清除） --- */
        if (g_clickFlag)
        {
            g_clickFlag = false;
            KeyEvent ev{};
            ev.type = KeyEventType::EncoderClick;
            ev.keyId = ROTARY_KEY_ID;
            ev.encoderDelta = 1; // 1 = single click
            ev.timestamp_ms = now;
            out.push_back(ev);
        }
        if (g_doubleFlag)
        {
            g_doubleFlag = false;
            KeyEvent ev{};
            ev.type = KeyEventType::EncoderClick;
            ev.keyId = ROTARY_KEY_ID;
            ev.encoderDelta = 2; // 2 = double click
            ev.timestamp_ms = now;
            out.push_back(ev);
        }
        if (g_longPressFlag)
        {
            g_longPressFlag = false;
            KeyEvent ev{};
            ev.type = KeyEventType::EncoderClick;
            ev.keyId = ROTARY_KEY_ID;
            ev.encoderDelta = 3; // 3 = long press start
            ev.timestamp_ms = now;
            out.push_back(ev);
        }

        /* 诊断：每 200ms 打印一次 SW 引脚电平，便于在没有 click 事件时
           区分"硬件没按" vs "OneButton 没识别"。
           默认关闭，需要时在编译宏里加 -DEKEYS_DEBUG_DIAG 打开。 */
#if EKEYS_DEBUG_DIAG
        static uint32_t lastDbgMs = 0;
        if (cfg.encoderSWPin >= 0 && (now - lastDbgMs) >= 200)
        {
            lastDbgMs = now;
            int level = digitalRead(cfg.encoderSWPin);
            Serial.printf("[EncDiag] SW=%d level=%d\n",
                          cfg.encoderSWPin, level);
        }
#endif
    }

} // namespace ekeys
