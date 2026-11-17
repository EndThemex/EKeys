/*
 * KeyNameTable.h
 *
 * 字符 / 键名字符串 → HID keycode 映射（FEATURE_DOC §3.1）。
 *
 * 支持：
 *
 *     a-z / A-Z / 0-9 / NUM_0~NUM_9
 *     Space / Enter / Esc / Backspace / Tab / CapsLock
 *     F1~F12 / PrintScreen / ScrollLock / Pause / Menu
 *     Insert / Delete / Home / End / PageUp / PageDown
 *     方向键 Up/Down/Left/Right（及 ← ↑ → ↓ / ArrowXxx 别名）
 *     符号 , . ; ' [ ] \ / - = `
 *     修饰键 Ctrl/Shift/Alt/Win（左右双侧）
 *
 * 也支持 "0xNN" / 十进制字面量解析（FEATURE_DOC §3.1 末）。
 */

#ifndef EKEYS_KEYMAP_KEY_NAME_TABLE_H
#define EKEYS_KEYMAP_KEY_NAME_TABLE_H

#include <Arduino.h>
#include <stdint.h>

namespace ekeys
{

    /*
     * 解析键名 → HID usage ID。
     *
     *   name = "a"        → 0x04 (HID_KEY_A)
     *   name = "Enter"    → 0x28 (HID_KEY_ENTER)
     *   name = "Ctrl"     → 0xE0 (Left Ctrl，修饰键 usage code)
     *   name = "0x52"     → 0x52
     *   name = "82"       → 0x52
     *
     * 修饰键名（大小写不敏感）：
     *   Ctrl/Control/Ctrl_L/LCtrl → 0xE0
     *   Shift/Shift_L/LShift       → 0xE1
     *   Alt/Option/Alt_L/LAlt      → 0xE2
     *   Win/GUI/Meta/Cmd/Super/LGUI → 0xE3
     *   Ctrl_R/RCtrl               → 0xE4
     *   Shift_R/RShift             → 0xE5
     *   Alt_R/RAlt                 → 0xE6
     *   Win_R/RGUI                 → 0xE7
     *
     * 未匹配返回 0x00。调用方应跳过 0x00。
     *
     * 注意：普通键名区分大小写（"A" 与 "a" 不同，大写经
     * resolveKeyWithModifier 自动附带 Shift）。
     */
    uint8_t resolveKeyName(const String &name);

    /*
     * 直接 HID keycode → 内置修饰键掩码（HID Boot Keyboard 规范）。
     *
     * 大写 A-Z 自动追加 Shift；支持 "Mod+key" 单槽组合键：
     *   "Ctrl+c"   → {0x06, 0x01}          （modifier 为位掩码，0x01=LCtrl）
     *   "Ctrl+Shift+A" → {0x04, 0x05}
     * 前缀段必须全为修饰键名，否则整串按普通键解析。
     */
    struct ResolvedKey
    {
        uint8_t keycode;
        uint8_t modifier; // HID 修饰键位（Left Ctrl = 0x01 等）
    };
    ResolvedKey resolveKeyWithModifier(const String &name);

    /*
     * 存储的键名 → 可读显示名（仅供 UI 展示，不影响 HID 解析）。
     *
     *   "0x04"       → "a"
     *   "0xE0"       → "Ctrl"
     *   "0xFF"       → "0xFF"（未知 usage 保留十六进制）
     *   "a" / "Ctrl" → 原样返回（已是可读名）
     *   "4"          → "4"（单字符按字符键处理，不再当作十进制字面量）
     */
    String keyDisplayName(const String &name);

} // namespace ekeys

#endif // EKEYS_KEYMAP_KEY_NAME_TABLE_H
