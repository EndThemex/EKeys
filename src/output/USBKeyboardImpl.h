/*
 * USBKeyboardImpl.h
 *
 * 基于 TinyUSB HID Keyboard 的实现。
 *
 * 阶段 01 使用 ESP32-S3 内置 USB CDC + HID：
 *
 *     #include <USB.h>
 *     #include <USBHIDKeyboard.h>
 *
 * 需要 platformio.ini 已启用：
 *
 *     build_flags = -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1
 */

#ifndef EKEYS_OUTPUT_USB_KEYBOARD_IMPL_H
#define EKEYS_OUTPUT_USB_KEYBOARD_IMPL_H

#include "IKeyboard.h"

namespace ekeys
{

    class USBKeyboardImpl : public IKeyboard
    {
    public:
        USBKeyboardImpl();

        bool begin() override;
        void press(uint8_t keycode, uint8_t modifier = 0) override;
        void release(uint8_t keycode) override;
        void type(const String &text) override;
        void releaseAll() override;
        bool isConnected() const override;
        void send() override;

    private:
        bool connected_;
    };

} // namespace ekeys

#endif // EKEYS_OUTPUT_USB_KEYBOARD_IMPL_H
