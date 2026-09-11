#ifndef UI_SETTINGS_TYPES_H
#define UI_SETTINGS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define UI_SETTINGS_PROFILE_COUNT 8

/*
 * 矩阵键进 UI 的动作编码基址（与 message_types.h 的 kMatrixKeyActionBase
 * 保持一致）：DisplayTask 以 BASE + key_id（101~111）透传 LV_EVENT_KEY，
 * 与 LV_KEY_*（10/17~20/27）数值不重叠——裸传 key_id 时矩阵键 10 会
 * 被当成旋钮单击（LV_KEY_ENTER=10）。
 */
#define UI_MATRIX_KEY_ACTION_BASE 100

    typedef struct
    {
        int32_t work_mode;
        int32_t rgb_mode;
        int32_t rgb_click_mode;
        int32_t rgb_brightness;
        int32_t tft_theme;
        int32_t tft_brightness;
        int32_t device_volume;
        int32_t power_mode;
        bool audio_enable;
        bool connect_host;
        bool voice_enable;
        uint8_t active_keymap_profile;
        char rgb_single_color[16];
        /* 主页日期/星期语言：0=中文（默认），1=英文 */
        uint8_t ui_lang;
    } ui_settings_snapshot_t;

#ifdef __cplusplus
}
#endif

#endif