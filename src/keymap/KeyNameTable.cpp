/*
 * KeyNameTable.cpp
 *
 * 阶段 01 仅解析：
 *
 *     a-z / A-Z / 0-9 / Space / Enter / Backspace / 0xNN
 */

#include "KeyNameTable.h"

namespace ekeys
{

    namespace
    {

        constexpr uint8_t HID_KEY_A = 0x04;
        constexpr uint8_t HID_KEY_Z = 0x1D;
        constexpr uint8_t HID_KEY_1 = 0x1E;
        constexpr uint8_t HID_KEY_0 = 0x27;
        constexpr uint8_t HID_KEY_ENTER = 0x28;
        constexpr uint8_t HID_KEY_BACKSPACE = 0x2A;
        constexpr uint8_t HID_KEY_SPACE = 0x2C;
        constexpr uint8_t HID_MOD_LSHIFT = 0x02;

        /*
         * 修饰键 usage code（0xE0~0xE7）。
         * USB/BLE 输出层（pressRaw / t-vk press）对 0xE0~0xE7 有特判，
         * 直接当 keycode 按下即置位 HID 报告的 modifier byte，不占普通键槽。
         * 因此 normal_key 通道拆槽后的 "Ctrl"+"c" 两槽依次 press 即为标准组合键。
         */
        constexpr uint8_t HID_KEY_LCTRL = 0xE0;
        constexpr uint8_t HID_KEY_LSHIFT = 0xE1;
        constexpr uint8_t HID_KEY_LALT = 0xE2;
        constexpr uint8_t HID_KEY_LGUI = 0xE3;

        /*
         * 修饰键名 → usage code（大小写不敏感）。
         * 仅左侧修饰键；未匹配返回 0x00。
         */
        uint8_t resolveModifierName(const String &name)
        {
            if (name.equalsIgnoreCase("Ctrl") || name.equalsIgnoreCase("Control") ||
                name.equalsIgnoreCase("Ctrl_L"))
            {
                return HID_KEY_LCTRL;
            }
            if (name.equalsIgnoreCase("Shift") || name.equalsIgnoreCase("Shift_L"))
            {
                return HID_KEY_LSHIFT;
            }
            if (name.equalsIgnoreCase("Alt") || name.equalsIgnoreCase("Option") ||
                name.equalsIgnoreCase("Alt_L"))
            {
                return HID_KEY_LALT;
            }
            if (name.equalsIgnoreCase("Win") || name.equalsIgnoreCase("GUI") ||
                name.equalsIgnoreCase("Meta") || name.equalsIgnoreCase("Cmd") ||
                name.equalsIgnoreCase("Super"))
            {
                return HID_KEY_LGUI;
            }
            return 0;
        }

        bool parseLiteralNumber(const String &name, uint8_t &out)
        {
            if (name.length() == 0)
            {
                return false;
            }

            bool hex = false;
            if (name.length() >= 2 && name[0] == '0' && (name[1] == 'x' || name[1] == 'X'))
            {
                hex = true;
            }

            const char *p = hex ? name.c_str() + 2 : name.c_str();
            if (*p == '\0')
            {
                return false;
            }

            uint32_t v = 0;
            while (*p)
            {
                char c = *p++;
                uint8_t d;
                if (c >= '0' && c <= '9')
                {
                    d = static_cast<uint8_t>(c - '0');
                }
                else if (c >= 'a' && c <= 'f')
                {
                    d = static_cast<uint8_t>(c - 'a' + 10);
                }
                else if (c >= 'A' && c <= 'F')
                {
                    d = static_cast<uint8_t>(c - 'A' + 10);
                }
                else
                {
                    return false;
                }
                v = hex ? (v << 4) | d : v * 10 + d;
                if (v > 0xFF)
                {
                    return false;
                }
            }
            out = static_cast<uint8_t>(v);
            return true;
        }

    } // namespace

    uint8_t resolveKeyName(const String &name)
    {
        uint8_t literal = 0;
        if (parseLiteralNumber(name, literal))
        {
            return literal;
        }
        const uint8_t mod = resolveModifierName(name);
        if (mod)
        {
            return mod;
        }
        if (name.length() == 1)
        {
            char c = name[0];
            if (c >= 'a' && c <= 'z')
            {
                return static_cast<uint8_t>(HID_KEY_A + (c - 'a'));
            }
            if (c >= 'A' && c <= 'Z')
            {
                return static_cast<uint8_t>(HID_KEY_A + (c - 'A'));
            }
            if (c >= '1' && c <= '9')
            {
                return static_cast<uint8_t>(HID_KEY_1 + (c - '1'));
            }
            if (c == '0')
            {
                return HID_KEY_0;
            }
            if (c == ' ')
            {
                return HID_KEY_SPACE;
            }
            if (c == '\n')
            {
                return HID_KEY_ENTER;
            }
        }
        if (name == "Enter")
            return HID_KEY_ENTER;
        if (name == "Backspace")
            return HID_KEY_BACKSPACE;
        if (name == "Space")
            return HID_KEY_SPACE;
        return 0;
    }

    namespace
    {

        /*
         * 单段解析（不含 '+'）：单大写字母自动加 Shift，其余走 resolveKeyName。
         */
        ResolvedKey resolveSingleSegment(const String &name)
        {
            ResolvedKey r{0, 0};
            if (name.length() == 1)
            {
                char c = name[0];
                if (c >= 'A' && c <= 'Z')
                {
                    r.keycode = static_cast<uint8_t>(HID_KEY_A + (c - 'A'));
                    r.modifier = HID_MOD_LSHIFT;
                    return r;
                }
            }
            r.keycode = resolveKeyName(name);
            return r;
        }

    } // namespace

    ResolvedKey resolveKeyWithModifier(const String &name)
    {
        /* 无 '+'：单键（含修饰键名单独成键，如 "Ctrl" → 0xE0） */
        if (name.indexOf('+') < 0)
        {
            return resolveSingleSegment(name);
        }

        /*
         * "Mod+Mod+key" 单槽组合键（function_key 通道整串传入，如 "Ctrl+c"）。
         * 除最后一段外必须全为修饰键名，否则整串回退普通解析（大概率失败）。
         * 修饰键 usage code 转回 modifier 位掩码交给 IKeyboard::press 展开。
         */
        uint8_t mask = 0;
        int start = 0;
        for (;;)
        {
            const int sep = name.indexOf('+', start);
            if (sep < 0)
            {
                break;
            }
            const uint8_t usage = resolveModifierName(name.substring(start, sep));
            if (usage == 0)
            {
                return resolveSingleSegment(name);
            }
            mask = static_cast<uint8_t>(mask | (1u << (usage - HID_KEY_LCTRL)));
            start = sep + 1;
        }
        if (mask == 0)
        {
            return resolveSingleSegment(name);
        }

        const ResolvedKey last = resolveSingleSegment(name.substring(start));
        if (last.keycode == 0)
        {
            return ResolvedKey{0, 0};
        }
        ResolvedKey r{0, 0};
        r.keycode = last.keycode;
        /* 末段为大写字母时附带 Shift（与单键 "A" → Shift+A 行为一致） */
        r.modifier = static_cast<uint8_t>(mask | last.modifier);
        return r;
    }

    namespace
    {

        /*
         * usage code → 可读键名（keyDisplayName 专用，覆盖
         * resolveKeyName 内置的全部可读段；未知 usage 保留十六进制）。
         */
        String nameForUsage(uint8_t usage)
        {
            if (usage >= HID_KEY_A && usage <= HID_KEY_Z)
            {
                return String(static_cast<char>('a' + (usage - HID_KEY_A)));
            }
            if (usage >= HID_KEY_1 && usage < HID_KEY_0)
            {
                return String(static_cast<char>('1' + (usage - HID_KEY_1)));
            }
            if (usage == HID_KEY_0)
            {
                return "0";
            }
            if (usage == HID_KEY_ENTER)
            {
                return "Enter";
            }
            if (usage == HID_KEY_BACKSPACE)
            {
                return "Backspace";
            }
            if (usage == HID_KEY_SPACE)
            {
                return "Space";
            }
            if (usage == HID_KEY_LCTRL)
            {
                return "Ctrl";
            }
            if (usage == HID_KEY_LSHIFT)
            {
                return "Shift";
            }
            if (usage == HID_KEY_LALT)
            {
                return "Alt";
            }
            if (usage == HID_KEY_LGUI)
            {
                return "Win";
            }
            char buf[8];
            snprintf(buf, sizeof(buf), "0x%02X", static_cast<unsigned>(usage));
            return String(buf);
        }

    } // namespace

    String keyDisplayName(const String &name)
    {
        uint8_t usage = 0;
        if (parseLiteralNumber(name, usage))
        {
            return nameForUsage(usage);
        }
        /* 已是可读键名（"a" / "Ctrl" / "Enter" 等）原样返回 */
        return name;
    }

} // namespace ekeys
