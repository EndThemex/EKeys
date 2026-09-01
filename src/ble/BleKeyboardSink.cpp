#include "BleKeyboardSink.h"
#include "BleKeyMap.h"

#if EKEYS_ENABLE_BLE
#include <BleKeyboard.h>
#include <BLEDevice.h> // for getAdvertising()->stop()/start()

static BleKeyboard g_ble(EKEYS_DEVICE_NAME, "EKeys Inc", 100 /*battery*/);
#endif

namespace ekeys
{
#if EKEYS_ENABLE_BLE
    /* 记录"旋钮单击/旋转最近一次 press 的 key 码"，releaseAll 改用 release 单键。
     * 关键：避免 releaseAll() 把矩阵键正在按的 key 也清掉。 */
    static uint8_t g_lastEncKey = 0;
    /* 用户级 BLE 开关：true=正常发 HID，false=停广播 + 吞掉所有按键 */
    static bool g_enabled = true;
#endif

    void BleKeyboardSink::begin()
    {
#if EKEYS_ENABLE_BLE
        g_ble.begin();
        g_enabled = true;
#endif
    }

    void BleKeyboardSink::tick()
    {
        // 当前库无需周期性 tick，保留接口便于后续插日志
    }

    void BleKeyboardSink::pressKey(uint8_t keyId)
    {
#if EKEYS_ENABLE_BLE
        if (!g_enabled || !g_ble.isConnected())
            return;
        if (keyId < 1 || keyId > 9)
            return;
        uint8_t hid = BLE_KEY_MAP[keyId];
        if (hid == 0)
            return;
        g_ble.press(hid);
#endif
    }

    void BleKeyboardSink::releaseKey(uint8_t keyId)
    {
#if EKEYS_ENABLE_BLE
        if (!g_enabled || !g_ble.isConnected())
            return;
        if (keyId < 1 || keyId > 9)
            return;
        uint8_t hid = BLE_KEY_MAP[keyId];
        if (hid == 0)
            return;
        g_ble.release(hid);
#endif
    }

    void BleKeyboardSink::encoderClick(int8_t kind)
    {
#if EKEYS_ENABLE_BLE
        if (!g_enabled || !g_ble.isConnected())
            return;
        if (kind < 1 || kind > 3)
            return;
        uint8_t hid = BLE_ENCODER_MAP[kind];
        if (hid == 0)
            return;
        g_ble.press(hid);
        g_lastEncKey = hid;
#endif
    }

    void BleKeyboardSink::encoderRelease()
    {
#if EKEYS_ENABLE_BLE
        if (!g_enabled)
            return;
        /* 只 release 旋钮那次按的键，不再 releaseAll（避免清掉正在按的矩阵键） */
        if (g_lastEncKey != 0)
        {
            g_ble.release(g_lastEncKey);
            g_lastEncKey = 0;
        }
#endif
    }

    void BleKeyboardSink::encoderRotate(int8_t delta)
    {
#if EKEYS_ENABLE_BLE
        if (!g_enabled || !g_ble.isConnected())
            return;
        /* delta > 0 → +1 索引（顺时针 → Right Arrow），delta < 0 → +2 索引（逆时针） */
        uint8_t idx = (delta > 0) ? 1 : 2;
        uint8_t hid = BLE_ROTATE_MAP[idx];
        if (hid == 0)
            return;
        g_ble.press(hid);
        g_lastEncKey = hid;
#endif
    }

    void BleKeyboardSink::encoderRotateRelease()
    {
        /* 与 encoderRelease() 等价语义：只 release 旋钮上次按的键 */
        encoderRelease();
    }

    bool BleKeyboardSink::isConnected() const
    {
#if EKEYS_ENABLE_BLE
        return g_enabled && g_ble.isConnected();
#else
        return false;
#endif
    }

    bool BleKeyboardSink::isEnabled() const
    {
#if EKEYS_ENABLE_BLE
        return g_enabled;
#else
        return false;
#endif
    }

    void BleKeyboardSink::setEnabled(bool en)
    {
#if EKEYS_ENABLE_BLE
        if (g_enabled == en)
            return;
        g_enabled = en;

        if (en)
        {
            /* 重新开始广播（Arduino-ESP32 BLE API：BLEDevice::startAdvertising） */
            BLEDevice::startAdvertising();
        }
        else
        {
            /* 关：1) 释放所有残留的 HID key report（不然 Host 端的"未释放键"
             *    会一直按着），2) 停广播让设备不可发现 */
            g_ble.releaseAll();
            g_lastEncKey = 0;
            BLEDevice::stopAdvertising();
        }
#else
        (void)en;
#endif
    }

} // namespace ekeys
