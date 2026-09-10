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
   *   普通键序列，最多 6 个，支持 "+" 分隔同时按下；
   *   修饰键名（Ctrl/Shift/Alt/Win）单独成槽即组合键（如 "Ctrl"+"c"）。
   *
   * macros_key[]:
   *   宏键序列，最多 5 个，按顺序先压后弹；
   *   仅存储/协议透传，KeyResolver 尚未实现宏播放。
   *
   * combo1_* / combo2_*:
   *   FUN 组合层通道（FUN1 优先于 FUN2）。设置二级页 / 协议 0x06 配置的
   *   fun_key1 / fun_key2 对应物理键按住时，其它键改为触发对应组合层
   *   （优先级同单击：function > text > normal）；FUN 键本身不产生 HID 输出。
   *
   * valid:
   *   仅供 KeyResolver 内部标注"已加载"状态。
   */
  struct KeyMapping
  {
    String function_key;
    String text_key;
    std::array<String, kKeyMappingNormalCount> normal_key;
    std::array<String, kKeyMappingMacrosCount> macros_key;
    String combo1_function_key;
    String combo1_text_key;
    std::array<String, kKeyMappingNormalCount> combo1_normal_key;
    String combo2_function_key;
    String combo2_text_key;
    std::array<String, kKeyMappingNormalCount> combo2_normal_key;
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
