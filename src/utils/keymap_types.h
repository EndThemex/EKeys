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

} // namespace ekeys

#endif // EKEYS_UTILS_KEYMAP_TYPES_H
