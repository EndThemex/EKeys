/*
 * KeyboardFactory.h
 *
 * 阶段 01 仅返回 USBKeyboardImpl；阶段 06 按 Configuration::WORK_MODE
 * 创建对应实例。
 */

#ifndef EKEYS_OUTPUT_KEYBOARD_FACTORY_H
#define EKEYS_OUTPUT_KEYBOARD_FACTORY_H

#include <memory>

#include "IKeyboard.h"

namespace ekeys {

enum class WorkMode : uint8_t {
    Wired = 0,
    Bluetooth = 1,
    Wireless24G = 2,
};

class KeyboardFactory {
public:
    /*
     * 阶段 01：wired 模式固定返回 USBKeyboardImpl；
     * 其它 work_mode 暂时回退到 USB 并打印 WARN。
     */
    static std::unique_ptr<IKeyboard> create(WorkMode mode = WorkMode::Wired);

    /*
     * 释放当前实例，回收内存资源（BLE 模式需要释放经典蓝牙）。
     * 阶段 01 留空实现。
     */
    static void release();
};

}  // namespace ekeys

#endif  // EKEYS_OUTPUT_KEYBOARD_FACTORY_H
