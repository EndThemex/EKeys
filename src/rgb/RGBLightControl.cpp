#include "rgb/RGBLightControl.h"

namespace ekeys {

RGBLightControl::RGBLightControl() {}

void RGBLightControl::begin() {
    // VCC 控制：参考 FunModularKeyboard 写 LOW（高电平开/低电平开由板子决定）
    pinMode(LED_VCC_CTRL, OUTPUT);
    digitalWrite(LED_VCC_CTRL, LOW);

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds_, NUM_LEDS);
    FastLED.setBrightness(80);  // 0~255，80 较温和
    FastLED.clear(true);        // 上电全灭
    enabled_ = false;
    currentEffect_ = LightEffect::Off;
}

void RGBLightControl::turnOffAll() {
    FastLED.clear(true);
}

void RGBLightControl::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
        turnOffAll();
    } else {
        applyEffectStatic();
    }
}

void RGBLightControl::setEffect(LightEffect e) {
    if (e >= LightEffect::Count) e = LightEffect::Off;
    currentEffect_ = e;
    if (enabled_) applyEffectStatic();
}

void RGBLightControl::cycleEffect(int8_t dir) {
    int8_t cur = (int8_t)currentEffect_;
    int8_t n   = (int8_t)LightEffect::Count;
    cur = (cur + dir + n) % n;
    setEffect((LightEffect)cur);
}

const char* RGBLightControl::effectName(LightEffect e) {
    switch (e) {
        case LightEffect::Off:          return "Off";
        case LightEffect::StaticWhite:  return "Static White";
        case LightEffect::StaticRed:    return "Static Red";
        case LightEffect::StaticGreen:  return "Static Green";
        case LightEffect::StaticBlue:   return "Static Blue";
        case LightEffect::Rainbow:      return "Rainbow";
        case LightEffect::RainbowWave:  return "Rainbow Wave";
        case LightEffect::Pulse:        return "Pulse";
        default:                        return "?";
    }
}

void RGBLightControl::applyEffectStatic() {
    switch (currentEffect_) {
        case LightEffect::Off:
            FastLED.clear(true);
            return;
        case LightEffect::StaticWhite:
            fill_solid(leds_, NUM_LEDS, CRGB::White); break;
        case LightEffect::StaticRed:
            fill_solid(leds_, NUM_LEDS, CRGB::Red); break;
        case LightEffect::StaticGreen:
            fill_solid(leds_, NUM_LEDS, CRGB::Green); break;
        case LightEffect::StaticBlue:
            fill_solid(leds_, NUM_LEDS, CRGB::Blue); break;
        case LightEffect::Rainbow:
        case LightEffect::RainbowWave:
        case LightEffect::Pulse:
            // 动态灯效：先填一帧底色，后续 tick() 推进
            fill_solid(leds_, NUM_LEDS, CRGB::Black);
            break;
        default:
            return;
    }
    FastLED.show();
}

void RGBLightControl::tick() {
    if (!enabled_) return;

    static uint8_t hue = 0;
    switch (currentEffect_) {
        case LightEffect::Rainbow:
            fill_rainbow(leds_, NUM_LEDS, hue, 256 / max((int)NUM_LEDS, 1));
            hue++;
            FastLED.show();
            break;
        case LightEffect::RainbowWave:
            for (int i = 0; i < NUM_LEDS; ++i) {
                leds_[i] = CHSV((uint8_t)(hue + i * 20), 255, 255);
            }
            hue++;
            FastLED.show();
            break;
        case LightEffect::Pulse: {
            // 整体呼吸：brightness 用 0..255 三角波
            static uint8_t br = 0;
            static int8_t step = 4;
            for (int i = 0; i < NUM_LEDS; ++i) {
                leds_[i] = CRGB(255, 255, 255).nscale8(br);
            }
            br += step;
            if (br <= 0 || br >= 255) step = -step;
            FastLED.show();
            break;
        }
        default:
            // 静态灯效：applyEffectStatic 已写好，不需要 tick
            break;
    }
}

}  // namespace ekeys
