// LVGL version: 8.3.11

#ifndef UI_STATUSBAR_H
#define UI_STATUSBAR_H

#ifdef __cplusplus
extern "C"
{
#endif

    enum WORKMODE
    {
        WIRED_KEYBOARD_MODE = 0,
        BLUETOOTH_KEYBOARD_MODE,
    };

    extern void ui_StatusBar_init(void);
    extern void ui_StatusBar_show(bool show);

    extern void status_bar_set_working_mode(int mode);
    extern void status_bar_set_recording_state(bool is_recording);
    extern void status_bar_set_volume(uint8_t volume);
    extern void status_bar_set_battery_level(uint8_t level);
    extern void status_bar_set_wifi_status(bool enabled, bool connected, int rssi);
    // CUSTOM VARIABLES

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
