/*
 * MatrixScanner.h
 *
 * 按键矩阵扫描器（FEATURE_DOC §2.1）。
 *
 * 物理 3 行 × 4 列 = 12 个位置，ROW0 仅 3 个键（COL3 缺），
 * 实际可用的应用键 ID 为 1~11。
 *
 * 每键独立消抖状态机：
 *
 *     IDLE → DEBOUNCE_PRESS → PRESSED → DEBOUNCE_RELEASE → IDLE
 *
 * 状态从 `stableState_` 转移到 `releasedKeys_/pressedKeys_`，调用方
 * 在每个边沿上消费；同一应用键不会出现重复的 PRESSED 边沿，直到
 * 用户松开。
 */

#ifndef EKEYS_INPUT_MATRIX_SCANNER_H
#define EKEYS_INPUT_MATRIX_SCANNER_H

#include <Arduino.h>

#include "utils/event_types.h"

namespace ekeys
{

  constexpr uint8_t kMatrixRowCount = 3;
  constexpr uint8_t kMatrixColCount = 4;
  constexpr uint8_t kMatrixKeyCount = 11; // 应用键 ID 1~11
  constexpr uint8_t kDebounceTimeMs = 10;

  /*
   * 单键状态机。
   *
   * 在 IDLE 时检测到按下 → DEBOUNCE_PRESS → 持续到 DEBOUNCE_TIME_MS 后
   * 仍是按下 → 翻转 stableState_ = PRESSED 并加入 pressedKeys_。
   */
  struct MatrixKeyState
  {
    enum class Phase : uint8_t
    {
      Idle = 0,
      DebouncePress,
      Pressed,
      DebounceRelease,
    } phase;
    bool stable_pressed;
    uint32_t phase_started_ms;
  };

  class MatrixScanner
  {
  public:
    MatrixScanner();

    /*
     * 初始化引脚（行输入上拉、列输出高）。
     * 必须在 setup() 中调用一次。
     */
    void begin();

    /*
     * 扫描一轮矩阵，更新所有按键状态机。
     * 建议以 5ms 周期调用。
     */
    void scan();

    /*
     * 当前稳定电平。true = 按下。
     */
    bool getStableState(uint8_t keyId) const;

    /*
     * 本轮扫描新增的"按下"事件。keyId 范围 1~11。
     * 调用方负责消费，例如 KeyResolver::press()。
     */
    void getPressedKeys(uint8_t out[], uint8_t &count) const;

    /*
     * 本轮扫描新增的"松开"事件。
     */
    void getReleasedKeys(uint8_t out[], uint8_t &count) const;

    /*
     * 应用键 ID → 物理 (row, col) 映射。
     *
     *   ID = 1~3    → ROW0, COL0~COL2
     *   ID = 4~7    → ROW1, COL0~COL3
     *   ID = 8~11   → ROW2, COL0~COL3
     */
    static void keyIdToRowCol(uint8_t keyId, uint8_t &row, uint8_t &col);

  private:
    bool readMatrixCell(uint8_t row, uint8_t col) const;
    void dispatchEdge(uint8_t keyId, bool pressed);

    MatrixKeyState states_[kMatrixKeyCount + 1]; // 下标 1~11
    uint8_t pressedKeys_[kMatrixKeyCount];
    uint8_t releasedKeys_[kMatrixKeyCount];
    uint8_t pressedCount_;
    uint8_t releasedCount_;
  };

} // namespace ekeys

#endif // EKEYS_INPUT_MATRIX_SCANNER_H
