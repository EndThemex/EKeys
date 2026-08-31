/*
 * Backlight.cpp
 *
 * 阶段 01：仅 GPIO 高 / 低，不使用 LEDC PWM。
 * 阶段 05 再切到 LEDC PWM（CONFIG 中 tft_brightness 0~100）。
 */

#include "Backlight.h"

#include <Arduino.h>

#include "hardware/PinMap.h"

namespace ekeys {

void Backlight::begin()
{
    pinMode(kPinLcdBacklight, OUTPUT);
    digitalWrite(kPinLcdBacklight, HIGH);
}

void Backlight::on()
{
    digitalWrite(kPinLcdBacklight, HIGH);
}

void Backlight::off()
{
    digitalWrite(kPinLcdBacklight, LOW);
}

void Backlight::setDuty(uint8_t percent)
{
    if (percent > 0) {
        digitalWrite(kPinLcdBacklight, HIGH);
    } else {
        digitalWrite(kPinLcdBacklight, LOW);
    }
}

}  // namespace ekeys
