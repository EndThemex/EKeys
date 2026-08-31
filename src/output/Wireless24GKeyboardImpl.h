/*
 * Wireless24GKeyboardImpl.h
 *
 * 2.4G 无线键盘后端（FEATURE_DOC §4，阶段 07 任务 7.1）。
 *
 * - 实现 IKeyboard 同一接口，内部委托 IRadio24G 发送 HID 报告；
 * - BOARD_HAS_24G 未定义（硬件未到位）时 begin() 返回 false，
 *   KeyboardFactory 回退 USB，不破坏 USB/BLE 切换流程（docs/07 7.1）。
 */

#ifndef EKEYS_OUTPUT_WIRELESS_24G_KEYBOARD_IMPL_H
#define EKEYS_OUTPUT_WIRELESS_24G_KEYBOARD_IMPL_H

#include <Arduino.h>

#include "IKeyboard.h"
#include "IRadio24G.h"

namespace ekeys {

class Wireless24GKeyboardImpl : public IKeyboard {
public:
    explicit Wireless24GKeyboardImpl(IRadio24G *radio = nullptr);
    ~Wireless24GKeyboardImpl() override;

    bool begin() override;
    void press(uint8_t keycode, uint8_t modifier = 0) override;
    void release(uint8_t keycode) override;
    void type(const String &text) override;
    void releaseAll() override;
    bool isConnected() const override;
    void send() override;

private:
    /* 把当前 modifier_ + keys_ 报告推给射频模块 */
    void pushReport();

    IRadio24G *radio_;       // 不持有所有权（硬件模式下外部注入）
    bool inited_ = false;
    uint8_t modifier_ = 0;
    uint8_t keys_[6] = {0};
};

}  // namespace ekeys

#endif  // EKEYS_OUTPUT_WIRELESS_24G_KEYBOARD_IMPL_H
