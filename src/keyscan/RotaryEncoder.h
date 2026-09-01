#pragma once
#include <Arduino.h>
#include "IKeySource.h"

namespace ekeys
{

    /* 旋转编码器（EC11）+ 集成按钮
     *
     * 引脚全部来自 KeyScanConfig：
     *   encoderAPin  -> CLK（A 相）
     *   encoderBPin  -> DT  （B 相）
     *   encoderSWPin -> SW  （按下）
     *
     * 实现 IKeySource，可与 MatrixScanner 并联在同一 KeyEventList。
     *
     * 设计要点：
     *   - 用 ESP32Encoder（PCNT 硬件计数器）做旋转检测，避免抖动并
     *     释放 CPU；A/B 引脚对矩阵 GPIO 无冲突。
     *   - 用 OneButton 库做单击/双击判别。OneButton 必须在每个 tick
     *     调用 tick()，所以 scanTask 每周期都额外 tick 一次。
     *   - 仅在状态变化时写事件（不每 ms 都上报）。
     */
    class RotaryEncoder : public IKeySource
    {
    public:
        RotaryEncoder() = default;

        void begin() override;
        void poll(KeyEventList &out) override;

        // OneButton tick —— 必须在每次 poll() 中调用，因为它基于时间窗口
        // 判别 click/double-click。在 poll() 内已处理，这里仅供测试桩调用。
        void tick();

        /* 内部事件层（L2）辅助：尝试发出一次旋转事件，返回是否成功。
         * false = 同向合并 / 反向静默被吞；true = 已追加到 out 并更新状态。 */
        bool emitRotateEvent(KeyEventList &out, int8_t dir, uint32_t now);

    private:
        int32_t lastCount_ = 0;
        uint32_t lastClickTime_ = 0; // OneButton 内置状态，这里仅供日志
        bool lastClickDown_ = false;
        bool lastDoubleDown_ = false;

        /* ---- 旋转抖动过滤状态 ----
         *
         * 三层滤波状态机（L1 信号层 + L2 事件层）：
         *   lastEmitDir_ / lastEmitMs_
         *     上次"接受"事件的旋转方向 + 时间（事件层时间窗基准）。
         *   pendingDir_ / pendingStartMs_ / pendingSteps_
         *     待定事件：当前 PCNT 净方向还没攒够 ENC_BURST_THRESHOLD，
         *     先存起来继续观察，避免把慢来回的 ±1 当成一次旋转发出去。
         *   burstSum_
         *     待定窗口内的 PCNT 净步数累积（取绝对值，判 ENC_BURST_THRESHOLD）。
         *
         * 关键：不直接修改 ESP32Encoder::getCount() 返回的物理计数。
         * 抖动过滤只影响"是否上报 KeyEvent"，lastCount_ 仍逐次推进，
         * 防止下次 poll 重复处理同一段 PCNT 历史。 */
        int8_t lastEmitDir_ = 0;  // 上次发出的方向：0=无, +1=顺, -1=逆
        uint32_t lastEmitMs_ = 0; // 上次发出的 millis()

        int8_t pendingDir_ = 0;       // 待定方向：0=无, +1=顺, -1=逆
        uint32_t pendingStartMs_ = 0; // 待定窗口开始时间
        int32_t pendingSteps_ = 0;    // 待定窗口内的净步数（带符号）
    };

} // namespace ekeys
