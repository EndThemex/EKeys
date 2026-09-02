#pragma once
#include <Arduino.h>
#include <FastLED.h>

// 引脚与灯珠数参考 FunModularKeyboard/src/RGBLightControl.h
// EKeys 只有 9 颗灯（与 9 键 1:1），故 NUM_LEDS 改为 9。
namespace ekeys
{

#define LED_PIN 6
#define LED_VCC_CTRL 36
#define NUM_LEDS 9
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

    // 灯效枚举：旋转旋钮循环切换
    enum class LightEffect : uint8_t
    {
        Off = 0,
        StaticWhite, // 静态白
        StaticRed,   // 静态红
        StaticGreen, // 静态绿
        StaticBlue,  // 静态蓝
        Rainbow,     // 彩虹
        RainbowWave, // 彩虹波浪
        Pulse,       // 整体呼吸
        Count,       // 哨兵：灯效总数
    };

    class RGBLightControl
    {
    public:
        RGBLightControl();

        // 初始化：拉高 VCC 控、注册 FastLED。setup() 中调用。
        void begin();

        // 关闭全部输出（不切换 currentEffect_，方便恢复）
        void turnOffAll();

        // 切换总开关：true=亮，false=灭（仅切 LED 输出，不改 currentEffect_）
        void setEnabled(bool enabled);
        bool isEnabled() const { return enabled_; }

        // 选择/循环灯效
        void setEffect(LightEffect e);
        void cycleEffect(int8_t dir); // +1 / -1
        LightEffect currentEffect() const { return currentEffect_; }
        static const char *effectName(LightEffect e);

        // 亮度（0~255）。番茄钟等外部模块可读 / 写。
        uint8_t brightness() const { return brightness_; }
        void setBrightnessLevel(uint8_t b); // 立即生效：静态灯效会立即重画，动态灯效由 tick() 自然刷新

        // 直接访问 LED 缓冲区（番茄钟等需要临时覆盖颜色时使用）。
        CRGB *leds() { return leds_; }
        void show(); // 立即 flush 到 LED

        // 每帧推进：动态灯效（Rainbow/Wave/Pulse）每 30ms 调一次。
        // 静态灯效不需要推进。
        void tick();

    private:
        void applyEffectStatic(); // 静态色一次写完即可
        CRGB leds_[NUM_LEDS];
        bool enabled_ = false;
        LightEffect currentEffect_ = LightEffect::Off;
        uint8_t brightness_ = 80;
    };

} // namespace ekeys
