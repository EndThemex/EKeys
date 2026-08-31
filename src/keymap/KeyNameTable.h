/*
 * KeyNameTable.h
 *
 * 字符 / 键名字符串 → HID keycode 映射（阶段 01 精简版）。
 *
 * 仅含 FEATURE_DOC §3.1 中本阶段必需的部分：
 *
 *     a-z / 0-9 / Enter / Backspace / Space
 *
 * 后续阶段按 §3.1 列表补齐。
 *
 * 也支持 "0xNN" / 十进制字面量解析（FEATURE_DOC §3.1 末）。
 */

#ifndef EKEYS_KEYMAP_KEY_NAME_TABLE_H
#define EKEYS_KEYMAP_KEY_NAME_TABLE_H

#include <Arduino.h>
#include <stdint.h>

namespace ekeys {

/*
 * 解析键名 → HID usage ID。
 *
 *   name = "a"        → 0x04 (HID_KEY_A)
 *   name = "Enter"    → 0x28 (HID_KEY_ENTER)
 *   name = "0x52"     → 0x52
 *   name = "82"       → 0x52
 *
 * 未匹配返回 0x00。调用方应跳过 0x00。
 *
 * 注意：本期 `name` 区分大小写（"A" 与 "a" 不同）。
 */
uint8_t resolveKeyName(const String &name);

/*
 * 直接 HID keycode → 内置修饰键掩码（HID Boot Keyboard 规范）。
 *
 * 大写 A-Z 自动追加 Shift。
 */
struct ResolvedKey {
    uint8_t keycode;
    uint8_t modifier;  // HID 修饰键位（Left Ctrl = 0x01 等）
};
ResolvedKey resolveKeyWithModifier(const String &name);

}  // namespace ekeys

#endif  // EKEYS_KEYMAP_KEY_NAME_TABLE_H
