/*
 * BLEKeyboardImpl.cpp
 *
 * t-vk/ESP32-BLE-Keyboard 包装。
 *
 * F11 修复（键值域转换）：上层传入原始 HID usage code，而 t-vk 库的
 * press/release 键值域是：
 *   - 0x00~0x7F : ASCII（库内部查 _asciimap 转 usage）
 *   - 0x80~0x87 : 直接置位/清除 modifier byte 的 bit (k-0x80)
 *   - >= 0x88   : 原始 usage（库内部做 k-136）
 * 直接透传 usage 会出错：0x04('a') 被当 ASCII 查表得 0 丢弃，
 * 0x28(Enter) 被打成 '('，0xE0~0xE7 修饰键被映射为字母+Shift。
 * 因此所有跨库调用必须先经 usageToLibKeycode() 转换。
 *
 * modifier 位掩码在 press 时展开为库域 0x80+n 并记录到 tracked_，
 * release 时一并释放（与 USBKeyboardImpl 行为一致）。
 */

#include "BLEKeyboardImpl.h"

#include <esp_bt.h>
#include <BleKeyboard.h>

#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {

        BleKeyboard *s_ble = nullptr;
        bool s_connected = false;
        /* 每次上电只允许一次 BLEDevice::init；重复 begin() 复用已运行的栈。 */
        bool s_ble_started = false;

        constexpr uint8_t kModifierBase = 0xE0; // LCtrl .. RWin = 0xE0~0xE7
        /* F2 修复：每个修饰键位独立引用计数（0xE0..0xE7）。 */
        constexpr uint8_t kModifierCount = 8;
        uint8_t s_mod_count[kModifierCount] = {0, 0, 0, 0, 0, 0, 0, 0};
        /* t-vk 库域的修饰键基址（0x80+bit 直接操作 modifier byte）。 */
        constexpr uint8_t kLibModifierBase = 0x80;

        /*
         * usage code → t-vk 库域（F11 修复，见文件头）。
         * 修饰键 0xE0~0xE7 → 0x80+bit；普通 usage（0x01~0x77）→ usage+136。
         * 不在键盘 usage 范围的值返回 false。
         */
        bool usageToLibKeycode(uint8_t usage, uint8_t &out)
        {
            if (usage >= kModifierBase &&
                usage < static_cast<uint8_t>(kModifierBase + kModifierCount))
            {
                out = static_cast<uint8_t>(kLibModifierBase +
                                           (usage - kModifierBase));
                return true;
            }
            if (usage >= 0x01 && usage <= 0x77)
            {
                out = static_cast<uint8_t>(usage + 136);
                return true;
            }
            return false;
        }

    } // namespace

    BLEKeyboardImpl::~BLEKeyboardImpl()
    {
        /*
         * F11 修复：模式切换 / 关机销毁实例时，把已按下的键全部释放，
         * 避免旧实例持有的 modifier/按键状态残留在主机侧（卡键）。
         * BLE 栈本身每次上电只 init 一次，随静态对象存活，不在此析构。
         */
        releaseAll();
    }

    bool BLEKeyboardImpl::begin()
    {
        if (impl_inited_)
        {
            return true;
        }

        static BleKeyboard ble("EKeys", "EKeys", 100);

        /*
         * F11 修复（幂等）：BLEDevice::init 每次上电只允许一次。
         * 模式切走再切回时，新实例直接复用已运行的 BLE 栈，
         * 禁止对同一静态对象重复调用 ble.begin()（行为未定义）。
         */
        if (s_ble_started)
        {
            s_ble = &ble;
            impl_inited_ = true;
            LOG_INFO("BLEKBD", "BLE keyboard reused (already started)");
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

        ble.begin();
        s_ble = &ble;

        /*
         * F11 修复（失败检测）：BleKeyboard::begin() 返回 void，
         * 库初始化失败原本不可检测（工厂的 USB 回退分支不可达）。
         * BLEDevice::init 同步完成，此处校验控制器已使能。
         */
        if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED)
        {
            LOG_ERROR("BLEKBD", "BLE controller not enabled after begin()");
            s_ble = nullptr;
            return false;
        }

        s_ble_started = true;
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
            for (uint8_t bit = 0; bit < kModifierCount; ++bit)
            {
                if (modifier & (1u << bit))
                {
                    s_ble->press(kLibModifierBase + bit);
                    if (s_mod_count[bit] < 255)
                    {
                        s_mod_count[bit]++;
                    }
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
        uint8_t lib = 0;
        if (usageToLibKeycode(keycode, lib))
        {
            s_ble->press(lib);
        }
        else
        {
            LOG_WARNING("BLEKBD", "press: unsupported usage 0x%x",
                        static_cast<unsigned>(keycode));
        }
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
                for (uint8_t bit = 0; bit < kModifierCount; ++bit)
                {
                    if (t.modifier & (1u << bit))
                    {
                        if (s_mod_count[bit] > 0)
                        {
                            s_mod_count[bit]--;
                        }
                        if (s_mod_count[bit] == 0)
                        {
                            s_ble->release(kLibModifierBase + bit);
                        }
                    }
                }
                t.keycode = 0;
                t.modifier = 0;
                break;
            }
        }
        uint8_t lib = 0;
        if (usageToLibKeycode(keycode, lib))
        {
            s_ble->release(lib);
        }
        else
        {
            LOG_WARNING("BLEKBD", "release: unsupported usage 0x%x",
                        static_cast<unsigned>(keycode));
        }
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
        for (uint8_t i = 0; i < kModifierCount; ++i)
        {
            s_mod_count[i] = 0;
        }
        s_ble->releaseAll();
    }

    /*
     * isConnected() 为 const 但会更新 s_connected 边沿缓存（仅日志用）。
     * s_connected 只在本方法写；库内 connected 由 BLE 回调任务写、
     * 此处读（bool 读天然原子）。当前唯一调用方是 MainTask，单读单写。
     */
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
