#ifndef UI_KEYMAPPED_H
#define UI_KEYMAPPED_H

#include <stddef.h>
#include <stdint.h>

#if defined __has_include
#if __has_include("lvgl.h")
#include "lvgl.h"
#elif __has_include("lvgl/lvgl.h")
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif
#else
#include "lvgl.h"
#endif

#include "ui_screen_tag.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_KeyMapped_screen_init(void);
void ui_KeyMapped_screen_destroy(void);
/* 按系统语言刷新标题（0=中文 1=English），由 ui_lang 模块统一调用 */
void ui_KeyMapped_apply_language(uint8_t lang);
void ui_event_KeyMappedScreen(lv_event_t * e);
void ui_event_ButtonLeftKeyMapped(lv_event_t * e);
void ui_event_ButtonRightKeyMapped(lv_event_t * e);
void ui_KeyMapped_set_profile_icon_source(const char *fallback_symbol);
void ui_KeyMapped_set_profile_icon_image_data(const uint8_t *image_data,
                                              size_t image_size,
                                              uint16_t width,
                                              uint16_t height,
                                              const char *fallback_symbol);

ui_screen_tag_t ui_get_active_screen_tag(void);
void ui_set_active_screen_tag(ui_screen_tag_t tag);

extern lv_obj_t * ui_KeyMapped;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
