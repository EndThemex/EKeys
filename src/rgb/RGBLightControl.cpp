/*
 * RGBLightControl.cpp
 *
 * 见 RGBLightControl.h。
 */

#include "RGBLightControl.h"

#include <Arduino.h>

#include "rgb/RGBDriver.h"

namespace ekeys {

namespace {

constexpr uint32_t kFrameIntervalMs = 30;

/* 0~255 色相 → RGB（简化色环，256 步一循环） */
void hueToRgb(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b)
{
    const uint8_t sector = hue / 43;  // 0~5
    const uint8_t frac = (hue % 43) * 6;
    switch (sector)
    {
    case 0: r = 255; g = frac; b = 0; break;
    case 1: r = 255 - frac; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = frac; break;
    case 3: r = 0; g = 255 - frac; b = 255; break;
    case 4: r = frac; g = 0; b = 255; break;
    default: r = 255; g = 0; b = 255 - frac; break;
    }
}

}  // namespace

RGBLightControl &RGBLightControl::instance()
{
    static RGBLightControl inst;
    return inst;
}

void RGBLightControl::applySettings(const DeviceSettings &snap)
{
    const RGBMode new_mode = static_cast<RGBMode>(snap.rgb_mode);
    const bool mode_changed = (new_mode != mode_);
    mode_ = new_mode;
    single_index_ = snap.rgb_single_colar % 24;
    brightness_ = (snap.rgb_brightness > 100) ? 100 : snap.rgb_brightness;

    RGBDriver::instance().begin();
    RGBDriver::instance().SetBrightness(brightness_);
    if (mode_changed && mode_ == RGB_NONE_MODE)
    {
        RGBDriver::instance().clearAll();
        RGBDriver::instance().show();
    }
}

RgbColor RGBLightControl::currentSingleColor() const
{
    return kPalette24[single_index_];
}

void RGBLightControl::setHighlight(uint8_t led, bool active)
{
    if (led < 11)
    {
        highlight_[led] = active;
    }
}

uint8_t RGBLightControl::hueWheel(uint16_t hue)
{
    return static_cast<uint8_t>(hue & 0xFF);
}

void RGBLightControl::renderFrame()
{
    RGBDriver &led = RGBDriver::instance();

    switch (mode_)
    {
    case RGB_NONE_MODE:
        return;  // 关灯（applySettings 已 clear）

    case RGB_SINGLE_MODE:
    {
        const RgbColor c = currentSingleColor();
        led.setAll(c.r, c.g, c.b);
        break;
    }

    case RGB_RAINBOW_MODE:
    case RGB_RAINBOWWARE_MODE:
    {
        for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
        {
            uint8_t r, g, b;
            hueToRgb(hueWheel(elapsed_ms_ * 2 + i * 23), r, g, b);
            if (mode_ == RGB_RAINBOWWARE_MODE)
            {
                /* 波形亮度：沿灯带传播的正弦 */
                const float wave = 0.55f + 0.45f *
                    sinf((elapsed_ms_ * 0.02f) + i * 0.6f);
                r = static_cast<uint8_t>(r * wave);
                g = static_cast<uint8_t>(g * wave);
                b = static_cast<uint8_t>(b * wave);
            }
            led.setPixel(i, r, g, b);
        }
        break;
    }

    case RGB_COLORCYCLE_MODE:
    {
        uint8_t r, g, b;
        hueToRgb(hueWheel(elapsed_ms_ * 3), r, g, b);
        led.setAll(r, g, b);
        break;
    }

    case RGB_METER_MODE:
    {
        /* 亮度条：绿→红渐变点亮前 N 颗（保留模式） */
        const uint8_t lit = static_cast<uint8_t>(
            (static_cast<uint32_t>(brightness_) * RGBDriver::kLedCount) / 100);
        for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
        {
            if (i < lit)
            {
                const uint8_t level = static_cast<uint8_t>(255 * (i + 1) / RGBDriver::kLedCount);
                led.setPixel(i, level, 255 - level, 0);
            }
            else
            {
                led.setPixel(i, 0, 0, 0);
            }
        }
        break;
    }

    case RGB_FIRE_MODE:
    {
        for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
        {
            /* 随机闪烁的暖色 */
            if ((uint8_t)(elapsed_ms_ + i * 37 + fire_seed_[i]) % 7 == 0)
            {
                fire_seed_[i] = static_cast<uint8_t>(random(256));
            }
            const uint8_t flicker = 120 + fire_seed_[i] / 3;
            led.setPixel(i, flicker, static_cast<uint8_t>(flicker * 0.35f), 0);
        }
        break;
    }

    case RGB_PULSE_MODE:
    {
        const RgbColor c = currentSingleColor();
        const float breath = 0.5f - 0.5f * cosf(elapsed_ms_ * 0.006f);  // ~1s 呼吸
        led.setAll(static_cast<uint8_t>(c.r * breath),
                   static_cast<uint8_t>(c.g * breath),
                   static_cast<uint8_t>(c.b * breath));
        break;
    }
    }

    /* 点击高亮 override（FEATURE_DOC §9 RGB_CLICK_MODE） */
    const RgbColor c = currentSingleColor();
    for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
    {
        if (highlight_[i])
        {
            led.setPixel(i, c.r, c.g, c.b);
        }
    }

    led.show();
}

void RGBLightControl::tick(uint32_t elapsed_ms)
{
    if (mode_ == RGB_NONE_MODE)
    {
        return;
    }
    elapsed_ms_ += elapsed_ms;
    if ((elapsed_ms_ - last_render_ms_) < kFrameIntervalMs)
    {
        return;
    }
    last_render_ms_ = elapsed_ms_;
    renderFrame();
}

}  // namespace ekeys
