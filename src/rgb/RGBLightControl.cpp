/*
 * RGBLightControl.cpp
 *
 * 见 RGBLightControl.h。
 */

#include "RGBLightControl.h"

#include <Arduino.h>

#include "rgb/RGBDriver.h"

namespace ekeys
{

    namespace
    {

        constexpr uint32_t kFrameIntervalMs = 30;

        /* 0~255 色相 → RGB（简化色环，256 步一循环） */
        void hueToRgb(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b)
        {
            const uint8_t sector = hue / 43; // 0~5
            const uint8_t frac = (hue % 43) * 6;
            switch (sector)
            {
            case 0:
                r = 255;
                g = frac;
                b = 0;
                break;
            case 1:
                r = 255 - frac;
                g = 255;
                b = 0;
                break;
            case 2:
                r = 0;
                g = 255;
                b = frac;
                break;
            case 3:
                r = 0;
                g = 255 - frac;
                b = 255;
                break;
            case 4:
                r = frac;
                g = 0;
                b = 255;
                break;
            default:
                r = 255;
                g = 0;
                b = 255 - frac;
                break;
            }
        }

    } // namespace

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
        single_index_ = snap.rgb_single_color % 24;
        brightness_ = (snap.rgb_brightness > 100) ? 100 : snap.rgb_brightness;

        RGBDriver::instance().begin();
        RGBDriver::instance().SetBrightness(brightness_);
        if (mode_changed && mode_ == RGB_NONE_MODE)
        {
            /* 强制 NONE 分支首帧重绘，避免切回后高亮掩码恰好相同被跳过 */
            last_none_mask_ = 0xFFFF;
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
        const RgbColor c = currentSingleColor();

        /*
         * 关灯模式下仍渲染点击高亮（FEATURE_DOC §9：RGB_CLICK_MODE 独立于
         * rgb_mode）：高亮键点亮调色板色、其余键熄灭；掩码无变化时跳过重发。
         */
        if (mode_ == RGB_NONE_MODE)
        {
            /* 11 颗 LED 需 11 位掩码，必须用 uint16_t（uint8_t 会把
             * LED 8~10 的位截断为 0，导致键 9~11 高亮失效） */
            uint16_t mask = 0;
            for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
            {
                if (highlight_[i])
                {
                    mask |= static_cast<uint16_t>(1u << i);
                }
            }
            if (mask == last_none_mask_)
            {
                return;
            }
            last_none_mask_ = mask;
            for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
            {
                if (mask & (1u << i))
                {
                    led.setPixel(i, c.r, c.g, c.b);
                }
                else
                {
                    led.setPixel(i, 0, 0, 0);
                }
            }
            led.show();
            return;
        }

        switch (mode_)
        {
        case RGB_SINGLE_MODE:
        {
            led.setAll(c.r, c.g, c.b);
            break;
        }

        case RGB_RAINBOW_MODE:
        case RGB_RAINBOWWARE_MODE:
        {
            for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
            {
                uint8_t r, g, b;
                hueToRgb(hueWheel(elapsed_ms_ / 8 + i * 23), r, g, b);
                if (mode_ == RGB_RAINBOWWARE_MODE)
                {
                    /* 波形亮度：沿灯带传播的正弦（约 5s 一周） */
                    const float wave = 0.55f + 0.45f *
                                                   sinf((elapsed_ms_ * 0.00125f) + i * 0.6f);
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
            hueToRgb(hueWheel(elapsed_ms_ / 8), r, g, b);
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
            /* 每 ~120ms 才允许每颗灯重洗一次 flicker seed，
             * 比之前每帧 30ms 抖动更接近真实火焰"舔动"的节奏 */
            const uint8_t fire_tick = static_cast<uint8_t>(elapsed_ms_ >> 7);
            for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
            {
                if (static_cast<uint8_t>(fire_tick + i * 17 + fire_seed_[i] / 16) % 4 == 0)
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
            const float breath = 0.5f - 0.5f * cosf(elapsed_ms_ * 0.006f); // ~1s 呼吸
            led.setAll(static_cast<uint8_t>(c.r * breath),
                       static_cast<uint8_t>(c.g * breath),
                       static_cast<uint8_t>(c.b * breath));
            break;
        }
        }

        /* 点击高亮 override（FEATURE_DOC §9 RGB_CLICK_MODE） */
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
        /* NONE 模式不早退：点击高亮（RGB_CLICK_MODE）仍需按帧渲染 */
        elapsed_ms_ += elapsed_ms;
        if ((elapsed_ms_ - last_render_ms_) < kFrameIntervalMs)
        {
            return;
        }
        last_render_ms_ = elapsed_ms_;
        renderFrame();
    }

} // namespace ekeys
