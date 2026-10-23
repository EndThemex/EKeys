#ifndef UI_KEYMAPPED_H
#define UI_KEYMAPPED_H

#include <stddef.h>
#include <stdint.h>

#include "ui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void ui_KeyMapped_screen_init(void);
    void ui_KeyMapped_screen_destroy(void);
    /* 按系统语言刷新标题（0=中文 1=English），由 ui_lang 模块统一调用 */
    void ui_KeyMapped_apply_language(uint8_t lang);
    void ui_event_KeyMappedScreen(lv_event_t *e);
    void ui_event_ButtonLeftKeyMapped(lv_event_t *e);
    void ui_event_ButtonRightKeyMapped(lv_event_t *e);
    void ui_KeyMapped_set_profile_icon_source(const char *fallback_symbol);
    void ui_KeyMapped_set_profile_icon_image_data(const uint8_t *image_data,
                                                  size_t image_size,
                                                  uint16_t width,
                                                  uint16_t height,
                                                  const char *fallback_symbol);

    extern lv_obj_t *ui_KeyMapped;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
