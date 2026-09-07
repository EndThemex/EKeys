/*
 * keymap_types.h
 *
 * 键映射相关 POD 类型（FEATURE_DOC §3.1）。
 *
 * 阶段 01 临时使用 Arduino String，便于阶段 03 替换为
 * src/utils/BoundedString.h 中的固定容量字符串。
 */

#ifndef EKEYS_UTILS_KEYMAP_TYPES_H
#define EKEYS_UTILS_KEYMAP_TYPES_H

#include <Arduino.h>
#include <array>

#include "input/MatrixScanner.h" // kMatrixKeyCount

namespace ekeys
{

  constexpr uint8_t kKeyMappingNormalCount = 6;
  constexpr uint8_t kKeyMappingMacrosCount = 5;

  /*
   * 单个物理键 / 特殊输入的映射。
   *
   * function_key:
   *   单个功能字符串（如 "KEY_FUNCTION_ASR"、"MEDIA_PLAY"）；
   *   非空时优先使用，并忽略 normal_key / macros_key。
   *
   * normal_key[]:
   *   普通键序列，最多 6 个，支持 "+" 分隔同时按下。
   *
   * macros_key[]:
   *   宏键序列，最多 5 个，按顺序先压后弹。
   *
   * valid:
   *   仅供 KeyResolver 内部标注"已加载"状态。
   */
  struct KeyMapping
  {
    String function_key;
    std::array<String, kKeyMappingNormalCount> normal_key;
    std::array<String, kKeyMappingMacrosCount> macros_key;
    bool valid;
  };

  /*
   * 默认映射表（FEATURE_DOC §3.1）：Key ID 1~11 → "a"~"k"。
   * 运行时回退 / 协议 0x05 上报 / 存储层补段共用，唯一事实源。
   */
  inline constexpr const char *kDefaultKeyMapping[kMatrixKeyCount + 1] = {
      "",
      "a", "b", "c", "d", "e",
      "f", "g", "h", "i", "j",
      "k"};

  /*
   * 用默认映射（a~k，走 function_key 通道）填满 1~11 号键，
   * 下标 0 置为无效占位。
   */
  inline void keymapFillDefaults(
      std::array<KeyMapping, kMatrixKeyCount + 1> &out)
  {
    out[0] = KeyMapping{};
    for (uint8_t i = 1; i <= kMatrixKeyCount; ++i)
    {
      KeyMapping m{};
      m.function_key = kDefaultKeyMapping[i];
      m.valid = true;
      out[i] = m;
    }
  }

} // namespace ekeys

#endif // EKEYS_UTILS_KEYMAP_TYPES_H
