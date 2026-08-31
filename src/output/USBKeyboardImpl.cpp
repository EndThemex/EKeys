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
         * 记录每个 keycode 对应的修饰键位，release(keycode) 时一并释放。
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
        void applyModifier(uint8_t modifier, bool press)
        {
            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                if (modifier & (1u << bit))
                {
                    const uint8_t usage = 0xE0 + bit;
                    if (press)
                    {
                        g_usb_keyboard.pressRaw(usage);
                    }
                    else
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
