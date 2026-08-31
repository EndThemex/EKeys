/*
 * Backlight.cpp
 *
 * 阶段 04：LEDC PWM 背光（验收标准：CMD_CONFIG_SET tft_brightness 立即生效）。
 * LEDC 通道 0 / 5kHz / 8-bit 分辨率；begin 前调用 setDuty 走 digitalWrite 兜底。
 */

#include "Backlight.h"

#include <Arduino.h>

#include "hardware/PinMap.h"
#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {

        constexpr int kBacklightLedcChannel = 0;
        constexpr int kBacklightLedcFreqHz = 5000;
        constexpr int kBacklightLedcResolution = 8; // duty 0~255

        constexpr uint8_t kDefaultDutyPercent = 80;

    } // namespace

    Backlight &Backlight::instance()
    {
        static Backlight inst;
        return inst;
    }

    void Backlight::begin()
    {
        if (begun_)
        {
            return;
        }
        ledcSetup(kBacklightLedcChannel, kBacklightLedcFreqHz,
                  kBacklightLedcResolution);
        ledcAttachPin(kPinLcdBacklight, kBacklightLedcChannel);
        begun_ = true;
        setDuty(kDefaultDutyPercent);
        LOG_INFO("BL", "backlight PWM ready (%d Hz, duty %u%%)",
                 kBacklightLedcFreqHz, kDefaultDutyPercent);
    }

    void Backlight::on()
    {
        setDuty(100);
    }

    void Backlight::off()
    {
        setDuty(0);
    }

    void Backlight::setDuty(uint8_t percent)
    {
        if (percent > 100)
        {
            percent = 100;
        }
        const uint32_t duty =
            (static_cast<uint32_t>(percent) * ((1u << kBacklightLedcResolution) - 1u)) / 100u;

        if (!begun_)
        {
            /* begin 前的兜底：GPIO 高 / 低 */
            pinMode(kPinLcdBacklight, OUTPUT);
            digitalWrite(kPinLcdBacklight, percent > 0 ? HIGH : LOW);
            return;
        }
        ledcWrite(kBacklightLedcChannel, duty);
    }

} // namespace ekeys
