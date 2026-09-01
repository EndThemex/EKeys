#pragma once
#include <Arduino.h>

/* BLE 键位映射表
 *
 * 物理键 keyId (1..9) 和旋钮动作（单击/双击/长按）映射到 ESP32 BLE Keyboard
 * 库内部使用的 key 编码。
 *
 * ⚠️ 注意：ESP32 BLE Keyboard 库的 BleKeyboard::press(uint8_t k) 接受的 k
 *    不是标准 USB HID Usage ID，而是它自定义的"库 key 码"：
 *
 *      k <  128  → 当 ASCII 处理（查内部 _asciimap 表）
 *      128..135  → modifier (0x80=LCTRL, 0x81=LSHIFT, ...)
 *      k >= 136  → 非打印键：库内部用 k - 136 得到 HID Usage ID 再写 report
 *
 *    因此，对标准 USB HID Keyboard Page (0x07) 的键 X：
 *      - 普通键（X < 0x80）：库 key 码 = X + 136 = X + 0x88
 *      - modifier（X >= 0xE0）：直接传 X（库会走 modifier 分支）
 *
 *    直接传 HID Usage ID（比如 0x1E = '1'）会被库当作 ASCII 0x1E（控制字符
 *    RS = Record Separator），查到 _asciimap[0x1E]=0，press() 直接返回 0
 *    不发送报告 —— 这就是之前"按了键只出空格和符号"的根因。
 *
 *    参考 ESP32 BLE Keyboard 库源码 BleKeyboard.cpp::press()。
 *
 * 常用 HID Usage ID（Keyboard/Keypad Page = 0x07，USB HID 1.5）：
 *   0x04 = 'a'/'A'       0x1E = '1'/'!'       0x28 = Enter
 *   0x05 = 'b'/'B'       0x1F = '2'/'@'       0x29 = Esc
 *   ...                  ...                  0x2B = Tab
 *   0x1D = 'z'/'Z'       0x26 = '9'/'('       0x2C = Space
 *   0xE0 = Left Ctrl     0xE1 = Left Shift
 */

namespace ekeys
{

    /* 把 USB HID Usage ID 转换为库内部 key 码。
     * HID 普通键（< 0x80）：加 0x88 进入"非打印键区"。
     * modifier（>= 0xE0）：库按 >=128 走 modifier 分支，原值即可。 */
    static constexpr inline uint8_t hidToLibKey(uint8_t hidUsageId)
    {
        return (hidUsageId >= 0xE0) ? hidUsageId : (uint8_t)(hidUsageId + 0x88U);
    }

    // 默认：1..9 → 顶部数字键 1..9
    inline constexpr uint8_t BLE_KEY_MAP[10] = {
        0,                 // index 0 占位（keyId 从 1 开始）
        hidToLibKey(0x1E), // 1 → '1'
        hidToLibKey(0x1F), // 2 → '2'
        hidToLibKey(0x20), // 3 → '3'
        hidToLibKey(0x21), // 4 → '4'
        hidToLibKey(0x22), // 5 → '5'
        hidToLibKey(0x23), // 6 → '6'
        hidToLibKey(0x24), // 7 → '7'
        hidToLibKey(0x25), // 8 → '8'
        hidToLibKey(0x26), // 9 → '9'
    };

    // 旋钮动作 → 库 key 码
    // kind 1=单击, 2=双击, 3=长按
    inline constexpr uint8_t BLE_ENCODER_MAP[4] = {
        0,                 // index 0 占位
        hidToLibKey(0x28), // 1 单击 → Enter
        hidToLibKey(0x29), // 2 双击 → Esc
        hidToLibKey(0x2B), // 3 长按 → Tab
    };

    // 旋钮旋转 → 库 key 码
    // index 1 = delta = +1（顺时针），2 = delta = -1（逆时针）
    // HID 0x4F = Right Arrow（"前进"），0x50 = Left Arrow（"后退"）
    inline constexpr uint8_t BLE_ROTATE_MAP[3] = {
        0,                 // index 0 占位
        hidToLibKey(0x4F), // 1 顺时针 → Right Arrow
        hidToLibKey(0x50), // 2 逆时针 → Left Arrow
    };

} // namespace ekeys