#pragma once
#include <Arduino.h>

/* BLE 键位映射表（多 profile，可切换）
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

    /* ---- Profile 数据模型 ----
     *
     * 每个 profile 是一组按键配置：
     *   - keyMap[10]：keyId 1..9 → BLE 库 key 码（index 0 占位）
     *   - encoderMap[4]：单击/双击/长按 → BLE 库 key 码（index 0 占位）
     *   - rotateMap[3]：旋钮 CW/CCW → BLE 库 key 码（index 0 占位）
     *   - name：显示名（最多 12 字符，给 UI 留余地）
     *
     * 预设 4 套常用配置（详见 .cpp），用户可在 BLE → KeyMap 子页切换；
     * 当前选中的 profile 索引持久化到 NVS（重启仍生效）。
     */
    struct KeyMapProfile
    {
        char name[16];
        uint8_t keyMap[10];
        uint8_t encoderMap[4];
        uint8_t rotateMap[3];
    };

    /* 预置配置数量 */
    static constexpr uint8_t BLE_PROFILE_COUNT = 4;

    /* 当前生效的 profile 索引（运行时可变，main.cpp 负责 NVS 持久化） */
    uint8_t bleActiveProfile();

    /* 设置当前 profile 索引（带边界裁剪），并把 BLE_KEY_MAP / ENCODER_MAP /
     * ROTATE_MAP 同步刷新成新 profile 的内容。给 main.cpp 启动期一次性调用。 */
    void bleSetActiveProfile(uint8_t idx);

    /* profile 表本身（const，存放在 .cpp） */
    const KeyMapProfile &bleProfile(uint8_t idx);

    /* ---- 旧 API 兼容：常量数组视图 ----
     *
     * 老的 BLE 发送逻辑（BLE_KEY_MAP[1..9] 等）保持原有调用语义：
     *   - 索引 0 占位
     *   - 直接从"当前 profile"复制出来
     *
     * 这些数组**必须可写**（refreshMapsFromActiveProfile() 会覆盖），
     * 所以不能用 static constexpr（编译期常量不可写）。
     * C++17 的 inline 变量可以做到"头文件声明 + 单一定义"，但本项目
     * arduino 框架默认 C++11，升标准风险较大。
     *
     * 因此采用经典的 extern 声明 + BleKeyMap.cpp 单一定义的方案：
     *   - 任何 .h/.cpp 直接 #include "BleKeyMap.h" 即可使用这三个符号；
     *   - 真正的存储定义只在 BleKeyMap.cpp 出现一次。
     *
     * ⚠️ 不要改成 `inline uint8_t arr[N] = {...}` —— 那是 C++17 特性，
     * 当前工程会在编译期出 warning 并退化为每个 TU 一份拷贝（ODR 风险）。
     * 详见 docs/10-input-mapping-rule.md §10.1。
     */
    extern uint8_t BLE_KEY_MAP[10];
    extern uint8_t BLE_ENCODER_MAP[4];
    extern uint8_t BLE_ROTATE_MAP[3];

    /* 内部：用 bleActiveProfile() 把当前 profile 同步到上面的 3 个数组。
     * BleKeyboardSink::setActiveProfile() 内部调用；UI 不直接调用。 */
    void refreshMapsFromActiveProfile();

} // namespace ekeys