/*
 * Backlight.h
 *
 * 背光控制（单例，阶段 04 起 LEDC PWM）。
 * duty 由 CMD_CONFIG_SET 的 `tft_brightness` 字段驱动（FEATURE_DOC §6），
 * 经 SETTING_UPDATE 消息由 DisplayTask 调用 setDuty()。
 */

#ifndef EKEYS_DISPLAY_BACKLIGHT_H
#define EKEYS_DISPLAY_BACKLIGHT_H

#include <stdint.h>

namespace ekeys
{

    class Backlight
    {
    public:
        static Backlight &instance();

        Backlight(const Backlight &) = delete;
        Backlight &operator=(const Backlight &) = delete;

        void begin();

        void on();
        /* 真正熄灭（绕过 setDuty 的 5% 下限，电源模式 2 关屏用） */
        void off();

        /*
         * 设置屏幕亮度，0~100（内部钳位到 [5,100]；需熄灭请调用 off()）。
         */
        void setDuty(uint8_t percent);

    private:
        Backlight() = default;

        bool begun_ = false;
    };

} // namespace ekeys

#endif // EKEYS_DISPLAY_BACKLIGHT_H
