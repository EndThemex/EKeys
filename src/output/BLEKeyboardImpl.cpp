/*
 * BLEKeyboardImpl.cpp
 *
 * t-vk/ESP32-BLE-Keyboard 包装：
 *   - press/release 直接透传原始 keycode（库内部处理 0xE0~0xE7 修饰键）；
 *   - modifier 位掩码在 press 时展开为 0xE0+n，并记录到 tracked_，
 *     release 时一并释放（与 USBKeyboardImpl 行为一致）。
 */

#include "BLEKeyboardImpl.h"

#include <BleKeyboard.h>

#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {

        BleKeyboard *s_ble = nullptr;
        bool s_connected = false;

        constexpr uint8_t kModifierBase = 0xE0; // LCtrl .. RWin = 0xE0~0xE7

    } // namespace

    bool BLEKeyboardImpl::begin()
    {
        if (impl_inited_)
        {
            return true;
        }

        /*
         * 释放经典蓝牙内存（FEATURE_DOC §4.2）。
         * 必须在 BleKeyboard::begin()（内部 esp_bt_controller_init）之前调用。
         */
        esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        {
            LOG_WARNING("BLEKBD", "mem_release classic bt: 0x%x", static_cast<int>(err));
        }

        for (auto &t : tracked_)
        {
            t.keycode = 0;
            t.modifier = 0;
        }

        static BleKeyboard ble("EKeys", "EKeys", 100);
        ble.begin();
        s_ble = &ble;
        impl_inited_ = true;
        LOG_INFO("BLEKBD", "BLE keyboard started (device name: EKeys)");
        return true;
    }

    void BLEKeyboardImpl::press(uint8_t keycode, uint8_t modifier)
    {
        if (s_ble == nullptr || !s_ble->isConnected())
        {
            return;
        }

        if (modifier != 0)
        {
            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                if (modifier & (1u << bit))
                {
                    s_ble->press(kModifierBase + bit);
                }
            }
            for (auto &t : tracked_)
            {
                if (t.keycode == 0)
                {
                    t.keycode = keycode;
                    t.modifier = modifier;
                    break;
                }
            }
        }
        s_ble->press(keycode);
    }

    void BLEKeyboardImpl::release(uint8_t keycode)
    {
        if (s_ble == nullptr)
        {
            return;
        }

        for (auto &t : tracked_)
        {
            if (t.keycode == keycode)
            {
                for (uint8_t bit = 0; bit < 8; ++bit)
                {
                    if (t.modifier & (1u << bit))
                    {
                        s_ble->release(kModifierBase + bit);
                    }
                }
                t.keycode = 0;
                t.modifier = 0;
                break;
            }
        }
        s_ble->release(keycode);
    }

    void BLEKeyboardImpl::type(const String &text)
    {
        if (s_ble == nullptr || !s_ble->isConnected())
        {
            return;
        }
        s_ble->print(text);
    }

    void BLEKeyboardImpl::releaseAll()
    {
        if (s_ble == nullptr)
        {
            return;
        }
        for (auto &t : tracked_)
        {
            t.keycode = 0;
            t.modifier = 0;
        }
        s_ble->releaseAll();
    }

    bool BLEKeyboardImpl::isConnected() const
    {
        if (s_ble == nullptr)
        {
            return false;
        }
        const bool now = s_ble->isConnected();
        if (now != s_connected)
        {
            s_connected = now;
            LOG_INFO("BLEKBD", "host %s", now ? "connected" : "disconnected");
        }
        return now;
    }

    void BLEKeyboardImpl::send()
    {
        /* t-vk 库每次 press/release 立即发送报告，无需 flush */
    }

} // namespace ekeys
