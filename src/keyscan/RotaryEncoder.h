#pragma once
#include <Arduino.h>
#include "IKeySource.h"

namespace ekeys {

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
class RotaryEncoder : public IKeySource {
public:
    RotaryEncoder() = default;

    void begin() override;
    void poll(KeyEventList& out) override;

    // OneButton tick —— 必须在每次 poll() 中调用，因为它基于时间窗口
    // 判别 click/double-click。在 poll() 内已处理，这里仅供测试桩调用。
    void tick();

private:
    int32_t  lastCount_      = 0;
    uint32_t lastClickTime_  = 0;    // OneButton 内置状态，这里仅供日志
    bool     lastClickDown_  = false;
    bool     lastDoubleDown_ = false;
};

}  // namespace ekeys
