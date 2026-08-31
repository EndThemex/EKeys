/*
 * KeyboardFactory.cpp
 */

#include "KeyboardFactory.h"

#include "USBKeyboardImpl.h"
#include "logging/LogManager.h"

namespace ekeys {

std::unique_ptr<IKeyboard> KeyboardFactory::create(WorkMode mode)
{
    switch (mode) {
        case WorkMode::Wired: {
            auto kb = std::unique_ptr<USBKeyboardImpl>(new USBKeyboardImpl());
            if (kb->begin()) {
                return std::unique_ptr<IKeyboard>(std::move(kb));
            }
            LOG_ERROR("KBD", "USBKeyboardImpl begin() failed");
            return nullptr;
        }
        case WorkMode::Bluetooth:
            LOG_WARNING("KBD", "Bluetooth mode not implemented yet, fallback to USB");
            return create(WorkMode::Wired);
        case WorkMode::Wireless24G:
            LOG_WARNING("KBD", "2.4G mode not implemented yet, fallback to USB");
            return create(WorkMode::Wired);
    }
    return nullptr;
}

void KeyboardFactory::release()
{
    /*
     * 阶段 01：USB 栈由 Arduino / TinyUSB 自动管理，无需手动释放。
     */
}

}  // namespace ekeys
