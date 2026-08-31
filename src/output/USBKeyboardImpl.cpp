/*
 * USBKeyboardImpl.cpp
 *
 * 基于 ESP32-S3 内置 USB CDC + HID Keyboard 的最小 HID 实现。
 */

#include "USBKeyboardImpl.h"

#include <USB.h>
#include <USBHIDKeyboard.h>
#include <hid_usage_codes.h>

#include "logging/LogManager.h"

namespace ekeys {

namespace {

USBHIDKeyboard g_usb_keyboard;

}  // namespace

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
    if (!connected_) {
        return;
    }
    if (modifier) {
        g_usb_keyboard.press(keycode, modifier);
    } else {
        g_usb_keyboard.press(keycode);
    }
}

void USBKeyboardImpl::release(uint8_t keycode)
{
    if (!connected_) {
        return;
    }
    g_usb_keyboard.release(keycode);
}

void USBKeyboardImpl::type(const String &text)
{
    if (!connected_) {
        return;
    }
    g_usb_keyboard.print(text);
}

void USBKeyboardImpl::releaseAll()
{
    if (!connected_) {
        return;
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

}  // namespace ekeys
