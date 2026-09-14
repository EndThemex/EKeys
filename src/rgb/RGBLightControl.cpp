/*
 * RGBLightControl.cpp
 *
 * 见 RGBLightControl.h。
 */

#include "RGBLightControl.h"

#include <Arduino.h>

#include "rgb/RGBDriver.h"

#include "audio/AudioAnalyzer.h"

namespace ekeys
{

    namespace
    {

        constexpr uint32_t kFrameIntervalMs = 30;

        /* 拾音模式：每帧回落步长（30ms/帧 → 满量程回落约 0.6s） */
        constexpr float kSoundDecayPerFrame = 0.045f;
        /* 拾音模式：静默时的最低亮度比例（保持模式可见） */
        constexpr float kSoundIdleFloor = 0.12f;

        /* 矩阵律动滤波：
         * AudioAnalyzer 每帧按帧内峰值自适应归一化，底噪/帧间波动会被放大成
         * 高频微闪。门限以下视为 0；起跳/回落改用指数平滑（非对称：
         * 起跳快而不过冲、回落缓），消掉帧间抖动。 */
        constexpr float kMatrixNoiseGate = 0.07f; /* 静噪门限（归一化 0~1） */
        constexpr float kMatrixAttack = 0.50f;    /* 起跳平滑系数（越大越跟手） */
        constexpr float kMatrixDecay = 0.10f;     /* 回落平滑系数（越小越缓） */
        constexpr float kMatrixEpsilon = 0.004f;  /* 残留截断，避免半亮像素长亮 */

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

    void RGBLightControl::setAudioBands(const float *bands, uint8_t count)
    {
        if (bands == nullptr || count == 0)
        {
            return;
        }
        if (count > 16)
        {
            count = 16;
        }
        for (uint8_t i = 0; i < count; ++i)
        {
            audio_bands_[i] = bands[i];
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

        case RGB_SOUND_MODE:
        {
            /*
             * 拾音：16 频段能量 → 11 灯（按段覆盖范围取峰值）。
             * 快速起跳、慢速回落形成"峰值下落"效果；色相沿灯带铺开。
             */
            for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
            {
                const uint8_t start = static_cast<uint8_t>(
                    i * AudioAnalyzer::kBandCount / RGBDriver::kLedCount);
                const uint8_t stop = static_cast<uint8_t>(
                    (i + 1) * AudioAnalyzer::kBandCount / RGBDriver::kLedCount);
                float target = 0.0f;
                for (uint8_t b = start; b < stop; ++b)
                {
                    if (audio_bands_[b] > target)
                    {
                        target = audio_bands_[b];
                    }
                }

                float level = audio_level_[i];
                level = (target > level) ? target : level - kSoundDecayPerFrame;
                if (level < 0.0f)
                {
                    level = 0.0f;
                }
                else if (level > 1.0f)
                {
                    level = 1.0f;
                }
                audio_level_[i] = level;

                uint8_t r, g, b;
                hueToRgb(hueWheel(i * 256 / RGBDriver::kLedCount), r, g, b);
                const float scale = (level > kSoundIdleFloor) ? level : kSoundIdleFloor;
                led.setPixel(i,
                             static_cast<uint8_t>(r * scale),
                             static_cast<uint8_t>(g * scale),
                             static_cast<uint8_t>(b * scale));
            }
            break;
        }

        case RGB_MATRIX_MODE:
        {
            /*
             * 矩阵律动：3 行 × 4 列（ROW0 的 COL3 空置，共 11 灯）。
             * 4 列对应 4 组频段（左低右高），每列按音量自下而上点亮 0~3 行，
             * 行亮度支持小数过渡；VU 配色：底行绿 / 中行黄 / 顶行红。
             */
            for (uint8_t col = 0; col < 4; ++col)
            {
                const uint8_t start = static_cast<uint8_t>(col * 4);
                float target = 0.0f;
                for (uint8_t b = start; b < start + 4; ++b)
                {
                    if (audio_bands_[b] > target)
                    {
                        target = audio_bands_[b];
                    }
                }
                /* 静噪门限：底噪不点亮 */
                if (target < kMatrixNoiseGate)
                {
                    target = 0.0f;
                }

                /* 非对称指数平滑：起跳快跟随、回落缓，消除帧间微闪 */
                float level = audio_col_[col];
                const float k = (target > level) ? kMatrixAttack : kMatrixDecay;
                level += (target - level) * k;
                if (level < kMatrixEpsilon)
                {
                    level = 0.0f;
                }
                else if (level > 1.0f)
                {
                    level = 1.0f;
                }
                audio_col_[col] = level;

                /* 行下标 → LED 下标（与 MatrixScanner::keyIdToRowCol 同布局，
                 * ROW0 只有 COL0~2，故行基址 0/3/7） */
                static const uint8_t kRowLedBase[3] = {0, 3, 7};
                /* VU 配色：底行绿、中行黄、顶行红（row=0 为底行） */
                static const uint8_t kRowColor[3][3] = {
                    {0, 255, 0}, {255, 200, 0}, {255, 40, 0}};
                for (uint8_t row = 0; row < 3; ++row)
                {
                    const float lit = level * 3.0f - row;
                    const uint8_t led_idx = kRowLedBase[2 - row] + col;
                    if (lit <= 0.0f)
                    {
                        led.setPixel(led_idx, 0, 0, 0);
                        continue;
                    }
                    const float inten = (lit > 1.0f) ? 1.0f : lit;
                    led.setPixel(led_idx,
                                 static_cast<uint8_t>(kRowColor[row][0] * inten),
                                 static_cast<uint8_t>(kRowColor[row][1] * inten),
                                 static_cast<uint8_t>(kRowColor[row][2] * inten));
                }
            }
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
