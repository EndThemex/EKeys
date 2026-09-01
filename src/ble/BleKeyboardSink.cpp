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

    /* ---- 旋钮旋转的去抖/合并状态机 ----
     *
     * 历史问题（已修）：
     *   1) 旋转过慢 → PCNT 在抖动/边沿临界点来回跳 ±1，发出反向 BLE 方向键
     *      → 用户感觉"来回"。
     *   2) 旋转过快 → 一次 drainQueue 内可能收到多个连续同向 delta，全部
     *      转 BLE press+release → Host 端被切了 N 次。
     *   3) 反向 → 同向快速切换时，旧方向键的 release 还没发，新方向的 press
     *      就覆盖了 g_lastEncKey，导致旧方向键"粘住"（Host 端一直按着）。
     *
     * 修复：
     *   - 在 BLE 层做合并 + 阈值：accum_ 累积净步数，超过 1 才视为有效；
     *   - 方向切换时必须先 release 上一次发的键，再 press 新键；
     *   - 同向连续累加只发 1 次 press（在帧尾统一发），然后下帧 release，
     *     BLE 层不再被 PCNT 抖率拖死。
     */
    static int8_t g_rotAccum = 0;            // 净步数（>0 顺时针，<0 逆时针）
    static uint8_t g_lastRotateHid = 0;      // 最近一次旋转 press 的 HID key（与单击隔离）
    static uint8_t g_lastRotateSigned = 0;   // 0=无, 1=顺时针(+), 2=逆时针(-)
    static bool g_rotPressPending = false;   // 本帧是否需要 press 旋转键
    static uint32_t g_lastRotatePressMs = 0; // 上次真正发 press 的 millis()

    /* BLE 旋转方向键最小间隔。低于此间隔的连续 press 会被合并/丢弃，
     * 防止 macOS / Windows 的"按住方向键自动加速"机制误触发。
     *   - 50ms 是 BLE 一次连接间隔（connection interval）量级，正常 HID
     *     report 也至少 7.5ms；50ms 对单步旋转响应几乎无感。
     *   - 与 RotaryEncoder 层的 ENC_MIN_STEP_MS=8 不同：源头已经合并过，
     *     这里 50ms 是给"两次有效旋转事件"之间再加一道缓冲。 */
    static constexpr uint32_t BLE_ROTATE_MIN_INTERVAL_MS = 50;
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
        if (delta == 0)
            return;

        /* 累积净步数。范围裁剪到 ±3 防止异常情况下 accum 爆炸
         * （正常一次 drainQueue 里最多也就 ±3 步）。 */
        g_rotAccum += delta;
        if (g_rotAccum > 3)
            g_rotAccum = 3;
        if (g_rotAccum < -3)
            g_rotAccum = -3;

        /* 只有净步数越过 ±1 才视为"一次有效旋转"，触发 press。
         * 这样可以滤掉：
         *   - 慢速旋转时 PCNT 在 ±1 之间反复跳：两次 +1 与 -1 累加 = 0，不发；
         *   - 一次快速旋转内连续多步 +N：累加到 +N 后仍只发一次 press，
         *     不会再切 N 次方向键（"快速旋转切换多次"问题）。
         * 阈值的方向由 accum 的符号决定：+1 → Right，-1 → Left。 */
        uint8_t signedDir = (g_rotAccum > 0) ? 1 : 2;
        if (g_rotAccum == 0)
        {
            /* ±1 之间反复跳，全部抵消，不发任何键。 */
            return;
        }

        /* 方向切换 / 首次 press：在发新 press 前必须先把上次方向键 release，
         * 否则 Host 端会同时收到 Right 和 Left 两个方向键（粘键）。 */
        if (g_lastRotateSigned != 0 && g_lastRotateSigned != signedDir)
        {
            if (g_lastRotateHid != 0)
            {
                g_ble.release(g_lastRotateHid);
            }
            g_lastRotateHid = 0;
            g_lastRotateSigned = 0;
            /* 单击键不受影响：单击走 g_lastEncKey，与旋转键隔离。 */
            g_lastEncKey = 0;
        }

        /* 若上一次方向键的 release 还没发出去（g_lastRotateHid!=0），
         * 说明上一帧的 press 还在等 release。这里把 accum 当作"还想继续
         * 旋转"，不要重复 press，只更新 accum；下一帧 release 由
         * encoderRotateRelease() 在帧开头发。 */
        if (g_lastRotateHid != 0)
        {
            /* 同向累加，不重复 press（保持 accum 让主循环可以看净步数） */
            return;
        }

        uint8_t hid = BLE_ROTATE_MAP[signedDir];
        if (hid == 0)
            return;

        /* L3 输出层限流：距上次 press < 50ms 的同/反向 press 直接吞掉。
         * 注意这里只针对"新 press"，上一帧的 release 由 encoderRotateRelease()
         * 在帧开头正常发送，不受此限流影响。 */
        if (g_lastRotatePressMs != 0 &&
            (millis() - g_lastRotatePressMs) < BLE_ROTATE_MIN_INTERVAL_MS)
        {
            /* 累积值保留，下一次 encoderRotate() 继续观察。
             * 若用户持续同向转，下次仍会因 accum 越界或 timeout 重新发。 */
            return;
        }

        g_ble.press(hid);
        g_lastRotateHid = hid;
        g_lastRotateSigned = signedDir;
        g_rotPressPending = true;
        g_lastRotatePressMs = millis();
#endif
    }

    void BleKeyboardSink::encoderRotateRelease()
    {
#if EKEYS_ENABLE_BLE
        /* 只 release 旋钮方向键，不动单击/矩阵键的 lastEncKey 路径。
         * 1) 若本帧刚 press 过方向键 → release 它。
         * 2) 否则若有残留 → release。 */
        if (g_lastRotateHid != 0)
        {
            g_ble.release(g_lastRotateHid);
            g_lastRotateHid = 0;
            g_lastRotateSigned = 0;
        }
        /* 消费完一次方向键事件后清零 accum，避免下个 poll 把旧方向
         * 又"补发"一次。如果用户继续转同方向，会重新累加到 ≥1 再发。
         * 注：g_lastRotatePressMs 不清零 —— 下一次 press 仍受 50ms 限流，
         * 这是正确行为（避免上一次旋转的 release 还没走完链路就又来一次 press）。 */
        g_rotAccum = 0;
        g_rotPressPending = false;
#endif
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
            g_lastRotateHid = 0;
            g_lastRotateSigned = 0;
            g_rotAccum = 0;
            g_rotPressPending = false;
            g_lastRotatePressMs = 0; // 重新 enable 后立即可发 press
            BLEDevice::stopAdvertising();
        }
#else
        (void)en;
#endif
    }

    void BleKeyboardSink::setActiveProfile(uint8_t idx)
    {
        /* 委托给 BleKeyMap：它会做边界裁剪 + 同步 3 个数组。 */
        bleSetActiveProfile(idx);
        /* 切换 profile 时清掉旋转状态机：旧方向的 HID key 还卡在 g_lastRotateHid
         * 上没 release，新 profile 的同一键码可能不同，必须强制清零避免粘键。 */
#if EKEYS_ENABLE_BLE
        if (g_lastRotateHid != 0 && g_enabled && g_ble.isConnected())
        {
            g_ble.release(g_lastRotateHid);
        }
        g_lastRotateHid = 0;
        g_lastRotateSigned = 0;
        g_rotAccum = 0;
        g_rotPressPending = false;
        g_lastEncKey = 0;
        g_lastRotatePressMs = 0; // 切换后立即可发 press
#endif
    }

    uint8_t BleKeyboardSink::activeProfile() const
    {
        return bleActiveProfile();
    }

} // namespace ekeys
