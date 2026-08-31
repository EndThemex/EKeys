#ifndef UI_KEYMAPPEDSECONDARY_H
#define UI_KEYMAPPEDSECONDARY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void ui_KeyMappedSecondary_screen_init(void);
extern void ui_KeyMappedSecondary_screen_destroy(void);
extern void ui_event_KeyMappedSecondaryScreen(lv_event_t * e);
extern void ui_event_ButtonLeftKeyMappedSecondary(lv_event_t * e);
extern void ui_event_ButtonRightKeyMappedSecondary(lv_event_t * e);
extern lv_obj_t * ui_KeyMappedSecondary;

void ui_KeyMappedSecondary_bind_main_screen_summary(lv_obj_t *icon_label,
													lv_obj_t *icon_image,
													lv_obj_t *profile_name);
void ui_KeyMappedSecondary_set_profile(const char *icon, const char *name, const char *file_name);
void ui_KeyMappedSecondary_set_profile_icon_source(const char *file_path, const char *fallback_symbol);
void ui_KeyMappedSecondary_set_profile_icon_image_data(const uint8_t *image_data,
													   size_t image_size,
													   uint16_t width,
													   uint16_t height,
													   const char *fallback_symbol);
void ui_KeyMappedSecondary_set_key_label(unsigned int key_index, const char *text);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif