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
        /*
         * 屏幕亮度下限 5%（与 UI 步进 / 协议命令 / applyUiSettingsSnapshot
         * 钳位保持一致）。0 也被抬到 5，避免首启无 ini 时黑屏。
         */
        constexpr uint8_t kBacklightMinPercent = 5;

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
        /*
         * 下限 5%（包括 0）：未烧录 ini 时 Configuration 默认 0，
         * 抬到 5% 可避免首启黑屏。`off()` 想黑屏请改调 setDuty(0) 之后
         * 再 digitalWrite LOW，或调用方自行下拉 LCD_BL。
         */
        if (percent < kBacklightMinPercent)
        {
            percent = kBacklightMinPercent;
        }
        /*
         * LCD_BL 实际为低电平点亮（实测：占空比越大屏幕越暗），
         * 这里把输入百分比反相后再喂 LEDC，使 percent 与视觉亮度同向。
         */
        const uint32_t duty =
            ((100u - static_cast<uint32_t>(percent)) *
             ((1u << kBacklightLedcResolution) - 1u)) /
            100u;

        if (!begun_)
        {
            /* begin 前的兜底：percent≥5 → 低电平点亮（与反相后的 PWM 行为一致） */
            pinMode(kPinLcdBacklight, OUTPUT);
            digitalWrite(kPinLcdBacklight, LOW);
            return;
        }
        ledcWrite(kBacklightLedcChannel, duty);
    }

} // namespace ekeys
