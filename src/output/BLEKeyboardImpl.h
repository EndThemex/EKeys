/*
 * BLEKeyboardImpl.h
 *
 * 基于 t-vk/ESP32-BLE-Keyboard 的 BLE HID 实现（FEATURE_DOC §4.1）。
 *
 * begin() 前必须释放经典蓝牙内存（esp_bt_controller_mem_release
 * ESP_BT_MODE_CLASSIC_BT），否则 BLE + WiFi 共存时堆不足。
 */

#ifndef EKEYS_OUTPUT_BLE_KEYBOARD_IMPL_H
#define EKEYS_OUTPUT_BLE_KEYBOARD_IMPL_H

#include "IKeyboard.h"

namespace ekeys
{

    class BLEKeyboardImpl : public IKeyboard
    {
    public:
        BLEKeyboardImpl() = default;
        ~BLEKeyboardImpl() override = default;

        bool begin() override;
        void press(uint8_t keycode, uint8_t modifier = 0) override;
        void release(uint8_t keycode) override;
        void type(const String &text) override;
        void releaseAll() override;
        bool isConnected() const override;
        void send() override;

    private:
        bool impl_inited_ = false;

        /*
         * keycode → 按下时带上的 modifier 位。
         * IKeyboard::release(keycode) 无 modifier 参数，释放时补齐修饰键。
         */
        struct ModifierRecord
        {
            uint8_t keycode{0};
            uint8_t modifier{0};
        };
        static constexpr uint8_t kMaxTrackedKeys = 12;
        ModifierRecord tracked_[kMaxTrackedKeys]{};
    };

} // namespace ekeys

#endif // EKEYS_OUTPUT_BLE_KEYBOARD_IMPL_H
