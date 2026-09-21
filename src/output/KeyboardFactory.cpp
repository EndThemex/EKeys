/*
 * KeyboardFactory.cpp
 */

#include "KeyboardFactory.h"

#include "BLEKeyboardImpl.h"
#include "USBKeyboardImpl.h"
#include "logging/LogManager.h"

namespace ekeys
{

    std::unique_ptr<IKeyboard> KeyboardFactory::create(WorkMode mode)
    {
        switch (mode)
        {
        case WorkMode::Wired:
        {
            auto kb = std::unique_ptr<USBKeyboardImpl>(new USBKeyboardImpl());
            if (kb->begin())
            {
                return std::unique_ptr<IKeyboard>(std::move(kb));
            }
            LOG_ERROR("KBD", "USBKeyboardImpl begin() failed");
            return nullptr;
        }
        case WorkMode::Bluetooth:
        {
            auto kb = std::unique_ptr<BLEKeyboardImpl>(new BLEKeyboardImpl());
            if (kb->begin())
            {
                return std::unique_ptr<IKeyboard>(std::move(kb));
            }
            LOG_WARNING("KBD", "BLEKeyboardImpl begin() failed, fallback to USB");
            return create(WorkMode::Wired);
        }
        }
        LOG_WARNING("KBD", "unknown WorkMode=%u, fallback to USB",
                     static_cast<unsigned>(mode));
        return create(WorkMode::Wired);
    }

    void KeyboardFactory::release()
    {
        /*
         * USB 栈由 Arduino / TinyUSB 自动管理。
         * BLE 控制器每次上电只初始化一次（BLEKeyboardImpl::begin 幂等）：
         * 切到 USB 时 BLE 栈随静态对象保留，切回 BLE 直接复用，
         * 实例析构时仅 releaseAll 清空主机侧按键状态。
         */
    }

} // namespace ekeys
