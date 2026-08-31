#ifndef UI_HASCREENSECONDARY_H
#define UI_HASCREENSECONDARY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void ui_HaScreenSecondary_screen_init(void);
extern void ui_HaScreenSecondary_screen_destroy(void);
extern void ui_event_HaScreenSecondary(lv_event_t * e);
extern void ui_event_ButtonExitHaSecondary(lv_event_t * e);
extern void ui_HaScreenSecondary_set_wifi_status(bool enabled, bool connected, int rssi, const char *ip_address);
extern void ui_HaScreenSecondary_set_tcp_status(bool connected, const char *server_endpoint);
extern void ui_HaScreenSecondary_set_mode_status(int work_mode);
extern void ui_HaScreenSecondary_set_voice_status(bool enabled, bool recording);
extern void ui_HaScreenSecondary_set_module_status(bool module_a_connected, bool module_b_connected);

extern lv_obj_t * ui_HaScreenSecondary;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif