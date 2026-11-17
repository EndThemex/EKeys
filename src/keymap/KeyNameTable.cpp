/*
 * KeyNameTable.cpp
 *
 * 解析 FEATURE_DOC §3.1 键名表：
 *
 *     a-z / A-Z / 0-9 / NUM_0~NUM_9
 *     Space / Enter / Esc / Backspace / Tab / CapsLock
 *     F1~F12 / PrintScreen / ScrollLock / Pause
 *     Insert / Delete / Home / End / PageUp / PageDown
 *     方向键（Up/Down/Left/Right 及箭头符号）
 *     符号 , . ; ' [ ] \ / - = `
 *     修饰键 Ctrl/Shift/Alt/Win（左右双侧）
 *     0xNN 十六进制 / 十进制字面量
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
        constexpr uint8_t HID_KEY_ESCAPE = 0x29;
        constexpr uint8_t HID_KEY_BACKSPACE = 0x2A;
        constexpr uint8_t HID_KEY_TAB = 0x2B;
        constexpr uint8_t HID_KEY_SPACE = 0x2C;
        constexpr uint8_t HID_KEY_MINUS = 0x2D;
        constexpr uint8_t HID_KEY_EQUAL = 0x2E;
        constexpr uint8_t HID_KEY_LEFTBRACKET = 0x2F;
        constexpr uint8_t HID_KEY_RIGHTBRACKET = 0x30;
        constexpr uint8_t HID_KEY_BACKSLASH = 0x31;
        constexpr uint8_t HID_KEY_SEMICOLON = 0x33;
        constexpr uint8_t HID_KEY_QUOTE = 0x34;
        constexpr uint8_t HID_KEY_GRAVE = 0x35;
        constexpr uint8_t HID_KEY_COMMA = 0x36;
        constexpr uint8_t HID_KEY_PERIOD = 0x37;
        constexpr uint8_t HID_KEY_SLASH = 0x38;
        constexpr uint8_t HID_KEY_CAPSLOCK = 0x39;
        constexpr uint8_t HID_KEY_F1 = 0x3A;
        constexpr uint8_t HID_KEY_PRINTSCREEN = 0x46;
        constexpr uint8_t HID_KEY_SCROLLLOCK = 0x47;
        constexpr uint8_t HID_KEY_PAUSE = 0x48;
        constexpr uint8_t HID_KEY_INSERT = 0x49;
        constexpr uint8_t HID_KEY_HOME = 0x4A;
        constexpr uint8_t HID_KEY_PAGEUP = 0x4B;
        constexpr uint8_t HID_KEY_DELETE = 0x4C;
        constexpr uint8_t HID_KEY_END = 0x4D;
        constexpr uint8_t HID_KEY_PAGEDOWN = 0x4E;
        constexpr uint8_t HID_KEY_RIGHT = 0x4F;
        constexpr uint8_t HID_KEY_LEFT = 0x50;
        constexpr uint8_t HID_KEY_DOWN = 0x51;
        constexpr uint8_t HID_KEY_UP = 0x52;
        constexpr uint8_t HID_KEY_NUMLOCK = 0x53;
        constexpr uint8_t HID_KEY_MENU = 0x65;
        constexpr uint8_t HID_MOD_LSHIFT = 0x02;

        /*
         * 修饰键 usage code（0xE0~0xE7）。
         * USB 输出层 pressRaw 直接使用 usage code。
         * BLE 输出层（t-vk 库）键值域不同（ASCII / 0x80+bit / usage+136），
         * 由 BLEKeyboardImpl::usageToLibKeycode 统一转换，勿在此处适配。
         * normal_key 通道拆槽后的 "Ctrl"+"c" 两槽依次 press 即为标准组合键。
         */
        constexpr uint8_t HID_KEY_LCTRL = 0xE0;
        constexpr uint8_t HID_KEY_LSHIFT = 0xE1;
        constexpr uint8_t HID_KEY_LALT = 0xE2;
        constexpr uint8_t HID_KEY_LGUI = 0xE3;
        constexpr uint8_t HID_KEY_RCTRL = 0xE4;
        constexpr uint8_t HID_KEY_RSHIFT = 0xE5;
        constexpr uint8_t HID_KEY_RALT = 0xE6;
        constexpr uint8_t HID_KEY_RGUI = 0xE7;

        /*
         * 修饰键名 → usage code（大小写不敏感）。
         * 支持左侧（默认 / *_L / L*）与右侧（*_R / R*）别名；
         * 未匹配返回 0x00。
         */
        uint8_t resolveModifierName(const String &name)
        {
            struct ModifierName
            {
                const char *name;
                uint8_t usage;
            };
            static constexpr ModifierName kModifiers[] = {
                {"Ctrl", HID_KEY_LCTRL},
                {"Control", HID_KEY_LCTRL},
                {"Ctrl_L", HID_KEY_LCTRL},
                {"LCtrl", HID_KEY_LCTRL},
                {"LeftCtrl", HID_KEY_LCTRL},
                {"Ctrl_R", HID_KEY_RCTRL},
                {"RCtrl", HID_KEY_RCTRL},
                {"RightCtrl", HID_KEY_RCTRL},
                {"Shift", HID_KEY_LSHIFT},
                {"Shift_L", HID_KEY_LSHIFT},
                {"LShift", HID_KEY_LSHIFT},
                {"LeftShift", HID_KEY_LSHIFT},
                {"Shift_R", HID_KEY_RSHIFT},
                {"RShift", HID_KEY_RSHIFT},
                {"RightShift", HID_KEY_RSHIFT},
                {"Alt", HID_KEY_LALT},
                {"Option", HID_KEY_LALT},
                {"Alt_L", HID_KEY_LALT},
                {"LAlt", HID_KEY_LALT},
                {"LeftAlt", HID_KEY_LALT},
                {"Alt_R", HID_KEY_RALT},
                {"RAlt", HID_KEY_RALT},
                {"RightAlt", HID_KEY_RALT},
                {"Win", HID_KEY_LGUI},
                {"GUI", HID_KEY_LGUI},
                {"Meta", HID_KEY_LGUI},
                {"Cmd", HID_KEY_LGUI},
                {"Super", HID_KEY_LGUI},
                {"Win_L", HID_KEY_LGUI},
                {"LGUI", HID_KEY_LGUI},
                {"LeftWin", HID_KEY_LGUI},
                {"Win_R", HID_KEY_RGUI},
                {"RGUI", HID_KEY_RGUI},
                {"RightWin", HID_KEY_RGUI},
            };
            for (const ModifierName &m : kModifiers)
            {
                if (name.equalsIgnoreCase(m.name))
                {
                    return m.usage;
                }
            }
            return 0;
        }

        bool parseLiteralNumber(const String &name, uint8_t &out)
        {
            /*
             * 单字符交由 resolveKeyName 的字符表处理（"1" 是数字键 0x1E，
             * 不是十进制字面量 1）；字面量至少 2 字符（"82" / "0x52"）。
             */
            if (name.length() < 2)
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

        /*
         * 具名键表（大小写不敏感）：覆盖 FEATURE_DOC §3.1 的编辑键 / 功能键 /
         * 锁定键 / 方向键，并兼容 App 编辑器展示名（CapsLock / PrintScreen /
         * PageUp / Delete 等）与箭头符号。
         */
        struct NamedKey
        {
            const char *name;
            uint8_t usage;
        };

        uint8_t resolveNamedKey(const String &name)
        {
            static constexpr NamedKey kNamedKeys[] = {
                {"Enter", HID_KEY_ENTER},
                {"Return", HID_KEY_ENTER},
                {"Escape", HID_KEY_ESCAPE},
                {"Esc", HID_KEY_ESCAPE},
                {"Backspace", HID_KEY_BACKSPACE},
                {"Tab", HID_KEY_TAB},
                {"Space", HID_KEY_SPACE},
                {"CapsLock", HID_KEY_CAPSLOCK},
                {"Caps", HID_KEY_CAPSLOCK},
                {"PrintScreen", HID_KEY_PRINTSCREEN},
                {"PrtSc", HID_KEY_PRINTSCREEN},
                {"ScrollLock", HID_KEY_SCROLLLOCK},
                {"ScrlLk", HID_KEY_SCROLLLOCK},
                {"Pause", HID_KEY_PAUSE},
                {"Insert", HID_KEY_INSERT},
                {"Ins", HID_KEY_INSERT},
                {"Home", HID_KEY_HOME},
                {"PageUp", HID_KEY_PAGEUP},
                {"PgUp", HID_KEY_PAGEUP},
                {"Delete", HID_KEY_DELETE},
                {"Del", HID_KEY_DELETE},
                {"End", HID_KEY_END},
                {"PageDown", HID_KEY_PAGEDOWN},
                {"PgDn", HID_KEY_PAGEDOWN},
                {"Right", HID_KEY_RIGHT},
                {"ArrowRight", HID_KEY_RIGHT},
                {"Left", HID_KEY_LEFT},
                {"ArrowLeft", HID_KEY_LEFT},
                {"Down", HID_KEY_DOWN},
                {"ArrowDown", HID_KEY_DOWN},
                {"Up", HID_KEY_UP},
                {"ArrowUp", HID_KEY_UP},
                {"→", HID_KEY_RIGHT},
                {"←", HID_KEY_LEFT},
                {"↓", HID_KEY_DOWN},
                {"↑", HID_KEY_UP},
                {"NumLock", HID_KEY_NUMLOCK},
                {"NumLk", HID_KEY_NUMLOCK},
                {"Menu", HID_KEY_MENU},
                {"Application", HID_KEY_MENU},
                /* 小键盘数字键名（FEATURE_DOC §3.1） */
                {"NUM_0", HID_KEY_0},
                {"NUM_1", HID_KEY_1},
                {"NUM_2", static_cast<uint8_t>(HID_KEY_1 + 1)},
                {"NUM_3", static_cast<uint8_t>(HID_KEY_1 + 2)},
                {"NUM_4", static_cast<uint8_t>(HID_KEY_1 + 3)},
                {"NUM_5", static_cast<uint8_t>(HID_KEY_1 + 4)},
                {"NUM_6", static_cast<uint8_t>(HID_KEY_1 + 5)},
                {"NUM_7", static_cast<uint8_t>(HID_KEY_1 + 6)},
                {"NUM_8", static_cast<uint8_t>(HID_KEY_1 + 7)},
                {"NUM_9", static_cast<uint8_t>(HID_KEY_1 + 8)},
            };
            for (const NamedKey &k : kNamedKeys)
            {
                if (name.equalsIgnoreCase(k.name))
                {
                    return k.usage;
                }
            }

            /* F1~F12 */
            if (name.length() >= 2 && name.length() <= 3 &&
                (name[0] == 'F' || name[0] == 'f'))
            {
                const int fn = name.substring(1).toInt();
                if (fn >= 1 && fn <= 12)
                {
                    return static_cast<uint8_t>(HID_KEY_F1 + (fn - 1));
                }
            }
            return 0;
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
        const uint8_t named = resolveNamedKey(name);
        if (named)
        {
            return named;
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
            switch (c)
            {
            case '0':
                return HID_KEY_0;
            case ' ':
                return HID_KEY_SPACE;
            case '\n':
                return HID_KEY_ENTER;
            /* FEATURE_DOC §3.1 符号键 */
            case '-':
                return HID_KEY_MINUS;
            case '=':
                return HID_KEY_EQUAL;
            case '[':
                return HID_KEY_LEFTBRACKET;
            case ']':
                return HID_KEY_RIGHTBRACKET;
            case '\\':
                return HID_KEY_BACKSLASH;
            case ';':
                return HID_KEY_SEMICOLON;
            case '\'':
                return HID_KEY_QUOTE;
            case '`':
                return HID_KEY_GRAVE;
            case ',':
                return HID_KEY_COMMA;
            case '.':
                return HID_KEY_PERIOD;
            case '/':
                return HID_KEY_SLASH;
            default:
                break;
            }
        }
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
         * resolveKeyName 内置的全部可读段 + 常用扩展键；
         * 未覆盖的 usage 保留十六进制）。
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

            /* 常用扩展键（App 端可能存 usage code 字面量，避免屏上显示 0xNN） */
            switch (usage)
            {
            case 0x29:
                return "Esc";
            case 0x2B:
                return "Tab";
            case 0x2D:
                return "-";
            case 0x2E:
                return "=";
            case 0x2F:
                return "[";
            case 0x30:
                return "]";
            case 0x31:
                return "\\";
            case 0x33:
                return ";";
            case 0x34:
                return "'";
            case 0x35:
                return "`";
            case 0x36:
                return ",";
            case 0x37:
                return ".";
            case 0x38:
                return "/";
            case 0x39:
                return "Caps";
            case 0x3A: /* F1~F12 = 0x3A~0x45 */
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
            case 0x40:
            case 0x41:
            case 0x42:
            case 0x43:
            case 0x44:
            case 0x45:
                return "F" + String(usage - 0x3A + 1);
            case 0x46:
                return "PrtSc";
            case 0x47:
                return "ScrlLk";
            case 0x48:
                return "Pause";
            case 0x49:
                return "Ins";
            case 0x4A:
                return "Home";
            case 0x4B:
                return "PgUp";
            case 0x4C:
                return "Del";
            case 0x4D:
                return "End";
            case 0x4E:
                return "PgDn";
            case 0x4F:
                return "Right";
            case 0x50:
                return "Left";
            case 0x51:
                return "Down";
            case 0x52:
                return "Up";
            case 0x53:
                return "NumLk";
            case 0x58:
                return "KPEnter";
            case 0x65:
                return "Menu";
            case 0xE4:
                return "RCtrl";
            case 0xE5:
                return "RShift";
            case 0xE6:
                return "RAlt";
            case 0xE7:
                return "RGUI";
            default:
                break;
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
