/*
 * USBKeyboardImpl.cpp
 *
 * 基于 ESP32-S3 内置 USB CDC + HID Keyboard 的最小 HID 实现。
 */

#include "USBKeyboardImpl.h"

#include <USB.h>
#include <USBHIDKeyboard.h>
/* 注: arduino-esp32 2.x 的 USB 库无 hid_usage_codes.h（属 Adafruit TinyUSB），
 * 本文件不使用按键常量, 键值由上层以原始 usage code 传入。 */

#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {

        USBHIDKeyboard g_usb_keyboard;

        /*
         * F2 修复：每个修饰键位独立引用计数。
         * 之前 release(keycode) 直接 applyModifier(modifier, false) 释放整个 mask，
         * 导致先松 Ctrl+A、再松 Ctrl+B 时把 Ctrl 一并释放，B 的组合键失效。
         * 现在每个 modifier 位仅在计数归零时才真正下发 release。
         */
        constexpr uint8_t kModifierCount = 8; // LCtrl..RWin = 0xE0..0xE7
        uint8_t g_mod_count[kModifierCount] = {0, 0, 0, 0, 0, 0, 0, 0};
        /*
         * 记录每个 keycode 对应的修饰键位，release(keycode) 时按位 dec 计数。
         * IKeyboard::release 无 modifier 参数，不记录会导致组合键释放后修饰键滞留。
         */
        struct PressedEntry
        {
            uint8_t keycode;
            uint8_t modifier;
        };
        constexpr uint8_t kMaxPressed = 8;
        PressedEntry g_pressed[kMaxPressed];

        // modifier 位掩码(0x01=LCtrl..0x80=RWin) → 0xE0~0xE7 的 usage code 逐位处理
        // F2 修复：press 时 inc 计数、release 时 dec 计数，归零才真正下发 release。
        void applyModifier(uint8_t modifier, bool press)
        {
            for (uint8_t bit = 0; bit < kModifierCount; ++bit)
            {
                if ((modifier & (1u << bit)) == 0)
                {
                    continue;
                }
                const uint8_t usage = 0xE0 + bit;
                if (press)
                {
                    g_usb_keyboard.pressRaw(usage);
                    if (g_mod_count[bit] < 255)
                    {
                        g_mod_count[bit]++;
                    }
                }
                else
                {
                    if (g_mod_count[bit] > 0)
                    {
                        g_mod_count[bit]--;
                    }
                    if (g_mod_count[bit] == 0)
                    {
                        g_usb_keyboard.releaseRaw(usage);
                    }
                }
            }
        }

    } // namespace

    USBKeyboardImpl::USBKeyboardImpl()
        : connected_(false)
    {
    }

    bool USBKeyboardImpl::begin()
    {
        g_usb_keyboard.begin();
        connected_ = true;
        LOG_INFO("USB_HID", "USB HID keyboard ready");
        return true;
    }

    void USBKeyboardImpl::press(uint8_t keycode, uint8_t modifier)
    {
        if (!connected_)
        {
            return;
        }
        g_usb_keyboard.pressRaw(keycode);
        if (modifier)
        {
            applyModifier(modifier, true);
        }
        for (auto &entry : g_pressed)
        {
            if (entry.keycode == 0)
            {
                entry.keycode = keycode;
                entry.modifier = modifier;
                break;
            }
        }
    }

    void USBKeyboardImpl::release(uint8_t keycode)
    {
        if (!connected_)
        {
            return;
        }
        g_usb_keyboard.releaseRaw(keycode);
        for (auto &entry : g_pressed)
        {
            if (entry.keycode == keycode)
            {
                if (entry.modifier)
                {
                    applyModifier(entry.modifier, false);
                }
                entry.keycode = 0;
                entry.modifier = 0;
                break;
            }
        }
    }

    void USBKeyboardImpl::type(const String &text)
    {
        if (!connected_)
        {
            return;
        }
        g_usb_keyboard.print(text);
    }

    void USBKeyboardImpl::releaseAll()
    {
        if (!connected_)
        {
            return;
        }
        for (auto &entry : g_pressed)
        {
            entry.keycode = 0;
            entry.modifier = 0;
        }
        for (uint8_t i = 0; i < kModifierCount; ++i)
        {
            g_mod_count[i] = 0;
        }
        g_usb_keyboard.releaseAll();
    }

    bool USBKeyboardImpl::isConnected() const
    {
        return connected_;
    }

    void USBKeyboardImpl::send()
    {
        /*
         * USBHIDKeyboard 内部已自动 flush；保留接口以便后续对接
         * IKeyboard 接口时不强制变更调用方。
         */
    }

} // namespace ekeys
