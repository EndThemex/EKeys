/*
 * RGBDriver.cpp
 *
 * 见 RGBDriver.h。
 */

#include "RGBDriver.h"

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "hardware/PinMap.h"
#include "logging/LogManager.h"

namespace ekeys
{

  RGBDriver &RGBDriver::instance()
  {
    static RGBDriver inst;
    return inst;
  }

  void RGBDriver::begin()
  {
    if (inited_)
    {
      return;
    }

    /* LED 供电先上电（PINOUT §2.4：先拉高 LED_PWR_CTRL 再驱动 DIN） */
    pinMode(kPinRgbPowerCtrl, OUTPUT);
    digitalWrite(kPinRgbPowerCtrl, HIGH);
    delay(1);

    auto *strip = new Adafruit_NeoPixel(kLedCount, kPinRgbDin,
                                        NEO_GRB + NEO_KHZ800);
    strip->begin();
    strip->setBrightness(64); // 默认约 25%
    strip->clear();
    strip->show();
    impl_ = strip;
    inited_ = true;
    LOG_INFO("RGB", "WS2812B ready (din=%u pwr=%u, %u leds)",
             kPinRgbDin, kPinRgbPowerCtrl, kLedCount);
  }

  void RGBDriver::setPixel(uint8_t led, uint8_t r, uint8_t g, uint8_t b)
  {
    if (!inited_ || led >= kLedCount)
    {
      return;
    }
    static_cast<Adafruit_NeoPixel *>(impl_)->setPixelColor(led, r, g, b);
  }

  void RGBDriver::setAll(uint8_t r, uint8_t g, uint8_t b)
  {
    if (!inited_)
    {
      return;
    }
    static_cast<Adafruit_NeoPixel *>(impl_)->fill(
        static_cast<Adafruit_NeoPixel *>(impl_)->Color(r, g, b));
  }

  void RGBDriver::clearAll()
  {
    if (!inited_)
    {
      return;
    }
    static_cast<Adafruit_NeoPixel *>(impl_)->clear();
  }

  void RGBDriver::show()
  {
    if (!inited_)
    {
      return;
    }
    static_cast<Adafruit_NeoPixel *>(impl_)->show();
  }

  void RGBDriver::SetBrightness(uint8_t brightness)
  {
    if (!inited_)
    {
      return;
    }
    if (brightness > 100)
    {
      brightness = 100;
    }
    static_cast<Adafruit_NeoPixel *>(impl_)->setBrightness(
        static_cast<uint8_t>(brightness * 255 / 100));
  }

} // namespace ekeys
