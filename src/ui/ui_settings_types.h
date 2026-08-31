#ifndef UI_SETTINGS_TYPES_H
#define UI_SETTINGS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_SETTINGS_PROFILE_COUNT 8

typedef struct {
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
} ui_settings_snapshot_t;

#ifdef __cplusplus
}
#endif

#endif