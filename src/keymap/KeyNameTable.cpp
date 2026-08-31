/*
 * KeyNameTable.cpp
 *
 * 阶段 01 仅解析：
 *
 *     a-z / A-Z / 0-9 / Space / Enter / Backspace / 0xNN
 */

#include "KeyNameTable.h"

namespace ekeys {

namespace {

constexpr uint8_t HID_KEY_A           = 0x04;
constexpr uint8_t HID_KEY_Z           = 0x1D;
constexpr uint8_t HID_KEY_1           = 0x1E;
constexpr uint8_t HID_KEY_0           = 0x27;
constexpr uint8_t HID_KEY_ENTER       = 0x28;
constexpr uint8_t HID_KEY_BACKSPACE   = 0x2A;
constexpr uint8_t HID_KEY_SPACE       = 0x2C;
constexpr uint8_t HID_MOD_LSHIFT      = 0x02;

bool parseLiteralNumber(const String &name, uint8_t &out)
{
    if (name.length() == 0) {
        return false;
    }

    bool hex = false;
    if (name.length() >= 2 && name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) {
        hex = true;
    }

    const char *p = hex ? name.c_str() + 2 : name.c_str();
    if (*p == '\0') {
        return false;
    }

    uint32_t v = 0;
    while (*p) {
        char c = *p++;
        uint8_t d;
        if (c >= '0' && c <= '9') {
            d = static_cast<uint8_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = static_cast<uint8_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = static_cast<uint8_t>(c - 'A' + 10);
        } else {
            return false;
        }
        v = hex ? (v << 4) | d : v * 10 + d;
        if (v > 0xFF) {
            return false;
        }
    }
    out = static_cast<uint8_t>(v);
    return true;
}

}  // namespace

uint8_t resolveKeyName(const String &name)
{
    uint8_t literal = 0;
    if (parseLiteralNumber(name, literal)) {
        return literal;
    }
    if (name.length() == 1) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') {
            return static_cast<uint8_t>(HID_KEY_A + (c - 'a'));
        }
        if (c >= 'A' && c <= 'Z') {
            return static_cast<uint8_t>(HID_KEY_A + (c - 'A'));
        }
        if (c >= '1' && c <= '9') {
            return static_cast<uint8_t>(HID_KEY_1 + (c - '1'));
        }
        if (c == '0') {
            return HID_KEY_0;
        }
        if (c == ' ') {
            return HID_KEY_SPACE;
        }
        if (c == '\n') {
            return HID_KEY_ENTER;
        }
    }
    if (name == "Enter")     return HID_KEY_ENTER;
    if (name == "Backspace") return HID_KEY_BACKSPACE;
    if (name == "Space")     return HID_KEY_SPACE;
    return 0;
}

ResolvedKey resolveKeyWithModifier(const String &name)
{
    ResolvedKey r{0, 0};
    if (name.length() == 1) {
        char c = name[0];
        if (c >= 'A' && c <= 'Z') {
            r.keycode  = static_cast<uint8_t>(HID_KEY_A + (c - 'A'));
            r.modifier = HID_MOD_LSHIFT;
            return r;
        }
    }
    if (name == "Shift+Enter" || name == "Shift+Space") {
        r.keycode  = (name == "Shift+Enter") ? HID_KEY_ENTER : HID_KEY_SPACE;
        r.modifier = HID_MOD_LSHIFT;
        return r;
    }
    r.keycode = resolveKeyName(name);
    return r;
}

}  // namespace ekeys
