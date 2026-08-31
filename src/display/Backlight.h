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
        void off();

        /*
         * 设置占空比，0~100（内部钳位；0=熄灭）。
         */
        void setDuty(uint8_t percent);

    private:
        Backlight() = default;

        bool begun_ = false;
    };

} // namespace ekeys

#endif // EKEYS_DISPLAY_BACKLIGHT_H
