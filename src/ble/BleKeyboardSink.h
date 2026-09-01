#pragma once
#include <Arduino.h>

namespace ekeys
{

    /* BLE HID 键盘封装（条件编译）
     *
     * 当 EKEYS_ENABLE_BLE=0 时：
     *   - 类方法全部变成空实现 / 返回 false
     *   - 不链接 NimBLE / BLE-Keyboard 库
     *   - main.cpp 仍可正常调用接口，无需 #ifdef
     *
     * 当 EKEYS_ENABLE_BLE=1 时：
     *   - 正常启动 BLE 广播（设备名 = EKEYS_DEVICE_NAME 宏）
     *   - 按 BleKeyMap.h 表发送 HID 报文
     */
    class BleKeyboardSink
    {
    public:
        BleKeyboardSink() = default;

        void begin();
        void tick();

        // keyId (1..9) → 查表发 HID press
        void pressKey(uint8_t keyId);
        // keyId (1..9) → 查表发 HID release
        void releaseKey(uint8_t keyId);

        // kind: 1=单击 2=双击 3=长按
        void encoderClick(int8_t kind);
        void encoderRelease();

        // 旋钮旋转：delta = +1 顺时针, -1 逆时针
        // press 一帧后再调 encoderRotateRelease() 释放（保持矩阵键按下状态）
        // 内部会自动合并连续同向 delta（短时间内只发 1 次 HID），
        // 自动处理反向 → 同向切换（先 release 上一个键再 press 新的），
        // 也兜底过滤慢速抖动的反向尖峰。
        void encoderRotate(int8_t delta);
        void encoderRotateRelease();

        bool isConnected() const;

        /* 用户级别的 BLE 开关（关 = 停广播 + 不发 HID，按键直接被吞）
         * 不修改库的 begin/end 内部状态，只切换 NimBLE 广播 + 自维护 enabled_ 标志。 */
        bool isEnabled() const;
        void setEnabled(bool en);

        /* Profile 切换：UI 在 KeyMap 子页改 profile 时调用。
         * 内部触发 refreshMapsFromActiveProfile()，下次按键立刻按新表发。
         * index 越界会自动裁剪到 0。 */
        void setActiveProfile(uint8_t idx);
        uint8_t activeProfile() const;
    };

} // namespace ekeys
