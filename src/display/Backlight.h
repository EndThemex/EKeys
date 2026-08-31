/*
 * Backlight.h
 *
 * 阶段 05 之前使用默认 duty = 80%；后续接 CMD_CONFIG_SET 的
 * `tft_brightness` 字段（FEATURE_DOC §6）。
 */

#ifndef EKEYS_DISPLAY_BACKLIGHT_H
#define EKEYS_DISPLAY_BACKLIGHT_H

#include <stdint.h>

namespace ekeys
{

    class Backlight
    {
    public:
        Backlight() = default;
        ~Backlight() = default;

        void begin();
        void on();
        void off();

        /*
         * 设置占空比，0~100。
         */
        void setDuty(uint8_t percent);
    };

} // namespace ekeys

#endif // EKEYS_DISPLAY_BACKLIGHT_H
