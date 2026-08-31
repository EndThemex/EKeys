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
        case WorkMode::Wireless24G:
            LOG_WARNING("KBD", "2.4G mode not implemented yet, fallback to USB");
            return create(WorkMode::Wired);
        }
        return nullptr;
    }

    void KeyboardFactory::release()
    {
        /*
         * USB 栈由 Arduino / TinyUSB 自动管理；
         * BLE 资源随实例销毁（BleKeyboard 为静态存储，仅断开广播）。
         */
    }

} // namespace ekeys
