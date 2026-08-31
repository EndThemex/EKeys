/*
 * event_types.h
 *
 * 输入事件的统一类型（FEATURE_DOC §2 / ARCHITECTURE §3.3）。
 *
 * 本期（阶段 01）只需要矩阵按键 + 旋钮 + 语音触发占位，
 * 实际触发者主要是 MatrixScanner 与（未来的）RotaryEncoder。
 */

#ifndef EKEYS_UTILS_EVENT_TYPES_H
#define EKEYS_UTILS_EVENT_TYPES_H

#include <stdint.h>

namespace ekeys
{

  /*
   * 输入来源。
   *
   * 阶段 01 仅使用 MatrixKey；其它来源的枚举值提前定义，
   * 避免后续阶段修改所有路由代码。
   */
  enum class InputSource : uint8_t
  {
    MatrixKey = 0,
    Ec11Knob = 1,
    Slider = 2,
    ModBKnob = 3,
    AsrTrigger = 4,
  };

  /*
   * 边沿事件。
   *
   * id:
   *   - MatrixKey: 应用键 ID（1~11）
   *   - Ec11Knob: 0 = LEFT / 1 = RIGHT / 2 = CLICK
   *   - Slider: 0 = SLIDER1 / 1 = SLIDER2
   *   - ModBKnob: 0 = LEFT / 1 = RIGHT
   *   - AsrTrigger: 保留 0
   *
   * pressed:
   *   - true  = 按下 / 正向边沿
   *   - false = 释放 / 反向边沿
   *
   * ts_ms:
   *   - millis() 时间戳，调试用。
   */
  struct InputEvent
  {
    uint8_t id;
    InputSource src;
    bool pressed;
    uint32_t ts_ms;
  };

} // namespace ekeys

#endif // EKEYS_UTILS_EVENT_TYPES_H
