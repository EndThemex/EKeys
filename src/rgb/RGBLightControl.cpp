#include "rgb/RGBLightControl.h"

namespace ekeys
{

    RGBLightControl::RGBLightControl() {}

    void RGBLightControl::begin()
    {
        // VCC 控制：参考 FunModularKeyboard 写 LOW（高电平开/低电平开由板子决定）
        pinMode(LED_VCC_CTRL, OUTPUT);
        digitalWrite(LED_VCC_CTRL, LOW);

        FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds_, NUM_LEDS);
        FastLED.setBrightness(brightness_); // 0~255，80 较温和
        FastLED.clear(true);                // 上电全灭
        enabled_ = false;
        currentEffect_ = LightEffect::Off;
    }

    void RGBLightControl::setBrightnessLevel(uint8_t b)
    {
        if (brightness_ == b)
            return;
        brightness_ = b;
        FastLED.setBrightness(brightness_);
        // FastLED.setBrightness 仅影响下一次 show()，因此静态灯效需要立即重画一帧。
        // Off 状态本身不显示，无需重画；动态灯效会由 tick() 自然刷新。
        if (enabled_ && currentEffect_ != LightEffect::Off)
        {
            switch (currentEffect_)
            {
            case LightEffect::Rainbow:
            case LightEffect::RainbowWave:
            case LightEffect::Pulse:
                // 动态灯效：等待下一次 tick() 即可
                break;
            default:
                applyEffectStatic();
                break;
            }
        }
    }

    void RGBLightControl::show()
    {
        FastLED.show();
    }

    void RGBLightControl::turnOffAll()
    {
        FastLED.clear(true);
    }

    void RGBLightControl::setEnabled(bool enabled)
    {
        enabled_ = enabled;
        if (!enabled_)
        {
            turnOffAll();
        }
        else
        {
            applyEffectStatic();
        }
    }

    void RGBLightControl::setEffect(LightEffect e)
    {
        if (e >= LightEffect::Count)
            e = LightEffect::Off;
        currentEffect_ = e;
        if (enabled_)
            applyEffectStatic();
    }

    void RGBLightControl::cycleEffect(int8_t dir)
    {
        int8_t cur = (int8_t)currentEffect_;
        int8_t n = (int8_t)LightEffect::Count;
        cur = (cur + dir + n) % n;
        setEffect((LightEffect)cur);
    }

    const char *RGBLightControl::effectName(LightEffect e)
    {
        switch (e)
        {
        case LightEffect::Off:
            return "Off";
        case LightEffect::StaticWhite:
            return "Static White";
        case LightEffect::StaticRed:
            return "Static Red";
        case LightEffect::StaticGreen:
            return "Static Green";
        case LightEffect::StaticBlue:
            return "Static Blue";
        case LightEffect::StaticOlive:
            return "Static Olive"; // 橄榄（bgv1 主色）
        case LightEffect::StaticMoss:
            return "Static Moss"; // 苔绿（bgv2 主色）
        case LightEffect::Rainbow:
            return "Rainbow";
        case LightEffect::RainbowWave:
            return "Rainbow Wave";
        case LightEffect::Pulse:
            return "Pulse";
        case LightEffect::AuroraMoss:
            return "Aurora Moss"; // 极光苔绿
        default:
            return "?";
        }
    }

    void RGBLightControl::applyEffectStatic()
    {
        switch (currentEffect_)
        {
        case LightEffect::Off:
            FastLED.clear(true);
            return;
        case LightEffect::StaticWhite:
            fill_solid(leds_, NUM_LEDS, CRGB::White);
            break;
        case LightEffect::StaticRed:
            fill_solid(leds_, NUM_LEDS, CRGB::Red);
            break;
        case LightEffect::StaticGreen:
            fill_solid(leds_, NUM_LEDS, CRGB::Green);
            break;
        case LightEffect::StaticBlue:
            fill_solid(leds_, NUM_LEDS, CRGB::Blue);
            break;
        case LightEffect::StaticOlive:
            // #416100 ≈ R=65 G=97 B=0（bgv1 主色）
            fill_solid(leds_, NUM_LEDS, CRGB(65, 97, 0));
            break;
        case LightEffect::StaticMoss:
            // #2E5613 ≈ R=46 G=86 B=19（bgv2 主色，更暗更深绿）
            fill_solid(leds_, NUM_LEDS, CRGB(46, 86, 19));
            break;
        case LightEffect::Rainbow:
        case LightEffect::RainbowWave:
        case LightEffect::Pulse:
        case LightEffect::AuroraMoss:
            // 动态灯效：先填一帧底色，后续 tick() 推进
            fill_solid(leds_, NUM_LEDS, CRGB::Black);
            break;
        default:
            return;
        }
        FastLED.show();
    }

    void RGBLightControl::tick()
    {
        if (!enabled_)
            return;

        static uint8_t hue = 0;
        switch (currentEffect_)
        {
        case LightEffect::Rainbow:
            fill_rainbow(leds_, NUM_LEDS, hue, 256 / max((int)NUM_LEDS, 1));
            hue++;
            FastLED.show();
            break;
        case LightEffect::RainbowWave:
            for (int i = 0; i < NUM_LEDS; ++i)
            {
                leds_[i] = CHSV((uint8_t)(hue + i * 20), 255, 255);
            }
            hue++;
            FastLED.show();
            break;
        case LightEffect::Pulse:
        {
            // 整体呼吸：brightness 用 0..255 三角波
            static uint8_t br = 0;
            static int8_t step = 4;
            for (int i = 0; i < NUM_LEDS; ++i)
            {
                leds_[i] = CRGB(255, 255, 255).nscale8(br);
            }
            br += step;
            if (br <= 0 || br >= 255)
                step = -step;
            FastLED.show();
            break;
        }
        case LightEffect::AuroraMoss:
        {
            // 极光苔绿：每颗 LED 在 #416100(橄榄) → #2E5613(苔绿) → #83A100(亮黄绿)
            // 三色之间做相位偏移的呼吸；整排 9 颗形成缓慢的极光波。
            // 周期 ≈ 4s（30ms/tick * 133 ≈ 4s）。
            static uint8_t phase = 0; // 0..255
            // 关键色（与 bgv2 配色调色板保持一致）
            const CRGB cOlive(65, 97, 0);    // #416100
            const CRGB cMoss(46, 86, 19);    // #2E5613
            const CRGB cBright(131, 161, 0); // #83A100（bgv1/bgv2 中的高光色）
            for (int i = 0; i < NUM_LEDS; ++i)
            {
                // 每颗 LED 相位错开 28 (≈ 256/9)，形成行波
                uint8_t p = phase + i * 28;
                // 用 0..84 / 85..169 / 170..255 三个区间做三色插值
                if (p < 85)
                {
                    leds_[i] = blend(cMoss, cOlive, (uint8_t)(p * 3));
                }
                else if (p < 170)
                {
                    leds_[i] = blend(cOlive, cBright, (uint8_t)((p - 85) * 3));
                }
                else
                {
                    leds_[i] = blend(cBright, cMoss, (uint8_t)((p - 170) * 3));
                }
            }
            phase += 2; // 30ms * 2/256 ≈ 4s 一个完整循环
            FastLED.show();
            break;
        }
        default:
            // 静态灯效：applyEffectStatic 已写好，不需要 tick
            break;
        }
    }

} // namespace ekeys
