#ifndef UI_PCSTATUSSCREENSECONDARY_H
#define UI_PCSTATUSSCREENSECONDARY_H

#ifdef __cplusplus
extern "C" {
#endif

extern void ui_PcStatusScreenSecondary_screen_init(void);
extern void ui_PcStatusScreenSecondary_screen_destroy(void);
extern void ui_event_PcStatusScreenSecondary(lv_event_t * e);
extern void ui_event_ButtonExitPcStatusSecondary(lv_event_t * e);

extern lv_obj_t * ui_PcStatusScreenSecondary;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif