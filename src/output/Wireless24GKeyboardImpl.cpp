/*
 * Wireless24GKeyboardImpl.cpp
 *
 * 见 Wireless24GKeyboardImpl.h。
 */

#include "Wireless24GKeyboardImpl.h"

#include "logging/LogManager.h"

namespace ekeys {

namespace {

/* keycode 0xE0~0xE7 → modifier 位（与 USBKeyboardImpl 同表） */
constexpr uint8_t kModifierTable[8] = {
    0x01, 0x02, 0x04, 0x08, 0x80, 0x40, 0x20, 0x10
};

}  // namespace

Wireless24GKeyboardImpl::Wireless24GKeyboardImpl(IRadio24G *radio)
    : radio_(radio)
{
}

Wireless24GKeyboardImpl::~Wireless24GKeyboardImpl()
{
    if (inited_ && radio_ != nullptr)
    {
        radio_->end();
    }
}

bool Wireless24GKeyboardImpl::begin()
{
#ifdef BOARD_HAS_24G
    if (radio_ == nullptr || !radio_->begin())
    {
        LOG_ERROR("KBD", "24G radio begin() failed");
        return false;
    }
    inited_ = true;
    LOG_INFO("KBD", "24G keyboard ready");
    return true;
#else
    /* 硬件未到位：日志告警，调用方回退 USB（docs/07 7.1） */
    LOG_WARNING("KBD", "2.4G not implemented");
    return false;
#endif
}

void Wireless24GKeyboardImpl::pushReport()
{
    if (inited_ && radio_ != nullptr)
    {
        radio_->sendKeyboardReport(modifier_, keys_);
    }
}

void Wireless24GKeyboardImpl::press(uint8_t keycode, uint8_t modifier)
{
    if (keycode >= 0xE0 && keycode <= 0xE7)
    {
        modifier_ |= kModifierTable[keycode - 0xE0];
    }
    else
    {
        for (uint8_t &slot : keys_)
        {
            if (slot == 0)
            {
                slot = keycode;
                break;
            }
        }
    }
    pushReport();
}

void Wireless24GKeyboardImpl::release(uint8_t keycode)
{
    if (keycode >= 0xE0 && keycode <= 0xE7)
    {
        modifier_ &= ~kModifierTable[keycode - 0xE0];
    }
    else
    {
        for (uint8_t &slot : keys_)
        {
            if (slot == keycode)
            {
                slot = 0;
            }
        }
    }
    pushReport();
}

void Wireless24GKeyboardImpl::type(const String &text)
{
    /* 阶段 07 占位：2.4G 文本注入暂不支持（仅宏/单键场景使用 press/release） */
    LOG_WARNING("KBD", "24G type() not supported (%u chars)",
                static_cast<unsigned>(text.length()));
}

void Wireless24GKeyboardImpl::releaseAll()
{
    modifier_ = 0;
    for (uint8_t &slot : keys_)
    {
        slot = 0;
    }
    pushReport();
}

bool Wireless24GKeyboardImpl::isConnected() const
{
    return inited_ && radio_ != nullptr && radio_->isConnected();
}

void Wireless24GKeyboardImpl::send()
{
    pushReport();
}

}  // namespace ekeys
