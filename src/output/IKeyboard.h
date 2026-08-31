/*
 * IKeyboard.h
 *
 * 键盘输出抽象接口（FEATURE_DOC §4 / ARCHITECTURE §3.5）。
 *
 * 三种后端（阶段 01 / 06）共用同一接口：
 *
 *   - USBKeyboardImpl           （TinyUSB HID Keyboard）
 *   - BLEKeyboardImpl           （ESP32 BLE Keyboard）
 *   - Wireless24GKeyboardImpl   （占位）
 *
 * KeyResolver 通过 IKeyboard& 调用，所有权归 KeyboardFactory。
 */

#ifndef EKEYS_OUTPUT_I_KEYBOARD_H
#define EKEYS_OUTPUT_I_KEYBOARD_H

#include <Arduino.h>
#include <stdint.h>

namespace ekeys {

class IKeyboard {
public:
    virtual ~IKeyboard() = default;

    /*
     * 初始化后端。失败返回 false。
     */
    virtual bool begin() = 0;

    /*
     * 普通 HID 按键按压 / 释放。
     * modifier 为 Boot Keyboard 修饰键位（0x01=LCtrl .. 0x80=RWin）。
     */
    virtual void press(uint8_t keycode, uint8_t modifier = 0) = 0;
    virtual void release(uint8_t keycode) = 0;

    /*
     * 字符串快捷接口。KeyResolver 暂不使用，保留供后续宏 / 文本注入。
     */
    virtual void type(const String &text) = 0;

    /*
     * 清空所有已按下键。
     */
    virtual void releaseAll() = 0;

    /*
     * 是否已被主机识别（HID 报告 ready）。
     */
    virtual bool isConnected() const = 0;

    /*
     * 立刻 flush 缓冲区（部分实现可能不需要）。
     */
    virtual void send() = 0;
};

}  // namespace ekeys

#endif  // EKEYS_OUTPUT_I_KEYBOARD_H
