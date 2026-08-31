#include "ui.h"
#include "ui_KeyMapped.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

lv_obj_t * ui_KeyMapped = NULL;
static lv_obj_t * ui_KeyMappedButtonLeft = NULL;
static lv_obj_t * ui_KeyMappedButtonRight = NULL;
static lv_obj_t * ui_KeyMappedButtonEnter = NULL;
static lv_obj_t * ui_KeyMappedButtonExit = NULL;
static lv_obj_t * ui_KeyMappedIcon = NULL;
static lv_style_t s_keymapped_title_style;
static lv_style_t s_keymapped_icon_style;
static bool s_keymapped_title_style_ready = false;
static bool s_keymapped_icon_style_ready = false;
static ui_screen_tag_t g_active_screen_tag = UI_SCREEN_UNKNOWN;
static char s_keymapped_icon_src[48] = {0};
static char s_keymapped_icon_symbol[8] = {0};
static uint8_t * s_keymapped_icon_image_data = NULL;
static size_t s_keymapped_icon_image_size = 0;
static lv_img_dsc_t s_keymapped_icon_image_dsc;

static void keymapped_release_cached_image(void)
{
    if (s_keymapped_icon_image_data != NULL) {
        lv_mem_free(s_keymapped_icon_image_data);
        s_keymapped_icon_image_data = NULL;
    }
    s_keymapped_icon_image_size = 0;
    lv_memset_00(&s_keymapped_icon_image_dsc, sizeof(s_keymapped_icon_image_dsc));
}

static void keymapped_apply_cached_image(void)
{
    if (ui_KeyMappedIcon) {
        lv_img_cache_invalidate_src(&s_keymapped_icon_image_dsc);
        lv_img_set_src(ui_KeyMappedIcon, &s_keymapped_icon_image_dsc);
    }
}

void ui_KeyMapped_set_profile_icon_source(const char *file_path, const char *fallback_symbol)
{
    const bool has_file = file_path != NULL && file_path[0] != '\0';

    snprintf(s_keymapped_icon_symbol,
             sizeof(s_keymapped_icon_symbol),
             "%s",
             (fallback_symbol && fallback_symbol[0]) ? fallback_symbol : LV_SYMBOL_KEYBOARD);

    if (has_file) {
        snprintf(s_keymapped_icon_src, sizeof(s_keymapped_icon_src), "%s", file_path);
    } else {
        s_keymapped_icon_src[0] = '\0';
    }

    if (ui_KeyMappedIcon == NULL) {
        return;
    }

    if (!has_file) {
        lv_img_set_src(ui_KeyMappedIcon, s_keymapped_icon_symbol);
        return;
    }

    lv_img_set_src(ui_KeyMappedIcon, s_keymapped_icon_symbol);
}

void ui_KeyMapped_set_profile_icon_image_data(const uint8_t *image_data,
                                              size_t image_size,
                                              uint16_t width,
                                              uint16_t height,
                                              const char *fallback_symbol)
{
    (void)image_data;
    (void)image_size;
    (void)width;
    (void)height;

    ui_KeyMapped_set_profile_icon_source(NULL,
                                         (fallback_symbol && fallback_symbol[0]) ? LV_SYMBOL_KEYBOARD : LV_SYMBOL_KEYBOARD);
    keymapped_release_cached_image();
}

static void keymapped_forward_key(uint32_t key)
{
    lv_obj_t *active_screen = lv_scr_act();
    lv_group_t *g = lv_group_get_default();
    lv_obj_t *target = g ? lv_group_get_focused(g) : active_screen;
    if (target) {
        lv_event_send(target, LV_EVENT_KEY, (void *)key);
    }
}

ui_screen_tag_t ui_get_active_screen_tag(void)
{
    return g_active_screen_tag;
}

void ui_set_active_screen_tag(ui_screen_tag_t tag)
{
    g_active_screen_tag = tag;
}

void ui_event_KeyMappedScreen(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_KEY) {
        uintptr_t key = (uintptr_t)lv_event_get_param(e);
        if (key == (uintptr_t)LV_KEY_LEFT) {
            ui_set_active_screen_tag(UI_SCREEN_MAIN);
            _ui_screen_change(&ui_MainScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_MainScreen_screen_init);
            lv_refr_now(NULL);
        } else if (key == (uintptr_t)LV_KEY_RIGHT) {
            _ui_screen_change(&ui_MusicScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_MusicScreen_screen_init);
            lv_refr_now(NULL);
        } else if (key == (uintptr_t)LV_KEY_ENTER) {
            ui_set_active_screen_tag(UI_SCREEN_KEYMAPPED_SECONDARY);
            _ui_screen_change(&ui_KeyMappedSecondary, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_KeyMappedSecondary_screen_init);
            lv_refr_now(NULL);
        }
    }
}

void ui_event_ButtonLeftKeyMapped(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        keymapped_forward_key(LV_KEY_LEFT);
    }
}

void ui_event_ButtonRightKeyMapped(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        keymapped_forward_key(LV_KEY_RIGHT);
    }
}

void ui_KeyMapped_screen_init(void)
{
    ui_KeyMapped = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_KeyMapped, LV_OBJ_FLAG_SCROLLABLE);
    ui_set_active_screen_tag(UI_SCREEN_KEYMAPPED);

    if (!s_keymapped_title_style_ready) {
        lv_style_init(&s_keymapped_title_style);
        lv_style_set_text_font(&s_keymapped_title_style, &ui_font_FontCKJGT28);
        lv_style_set_text_letter_space(&s_keymapped_title_style, 1);
        lv_style_set_text_color(&s_keymapped_title_style, lv_color_hex(0xF5F8FF));
        lv_style_set_text_opa(&s_keymapped_title_style, LV_OPA_COVER);
        s_keymapped_title_style_ready = true;
    }

    if (!s_keymapped_icon_style_ready) {
        lv_style_init(&s_keymapped_icon_style);
        lv_style_set_text_font(&s_keymapped_icon_style, &lv_font_montserrat_48);
        s_keymapped_icon_style_ready = true;
    }

    ui_KeyMappedButtonLeft = lv_btn_create(ui_KeyMapped);
    lv_obj_set_size(ui_KeyMappedButtonLeft, 41, 28);
    lv_obj_set_align(ui_KeyMappedButtonLeft, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_KeyMappedButtonLeft, -178, -9);
    lv_obj_set_style_bg_opa(ui_KeyMappedButtonLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_KeyMappedButtonLeft, LV_OBJ_FLAG_SCROLLABLE);

    ui_KeyMappedButtonRight = lv_btn_create(ui_KeyMapped);
    lv_obj_set_size(ui_KeyMappedButtonRight, 41, 28);
    lv_obj_set_align(ui_KeyMappedButtonRight, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_KeyMappedButtonRight, 181, -10);
    lv_obj_set_style_bg_opa(ui_KeyMappedButtonRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_KeyMappedButtonRight, LV_OBJ_FLAG_SCROLLABLE);

    ui_KeyMappedButtonEnter = lv_btn_create(ui_KeyMapped);
    lv_obj_set_size(ui_KeyMappedButtonEnter, 41, 28);
    lv_obj_set_align(ui_KeyMappedButtonEnter, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_KeyMappedButtonEnter, 2, 38);
    lv_obj_set_style_bg_opa(ui_KeyMappedButtonEnter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_KeyMappedButtonEnter, LV_OBJ_FLAG_SCROLLABLE);

    ui_KeyMappedButtonExit = lv_btn_create(ui_KeyMapped);
    lv_obj_set_size(ui_KeyMappedButtonExit, 40, 20);
    lv_obj_set_align(ui_KeyMappedButtonExit, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_KeyMappedButtonExit, -181, -54);
    lv_obj_set_style_bg_opa(ui_KeyMappedButtonExit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_KeyMappedButtonExit, LV_OBJ_FLAG_SCROLLABLE);

    ui_KeyMappedIcon = lv_img_create(ui_KeyMapped);
    lv_img_set_src(ui_KeyMappedIcon, LV_SYMBOL_KEYBOARD);
    lv_obj_add_style(ui_KeyMappedIcon, &s_keymapped_icon_style, 0);
    lv_obj_set_size(ui_KeyMappedIcon, 80, 80);
    lv_obj_set_x(ui_KeyMappedIcon, 20);
    lv_obj_set_y(ui_KeyMappedIcon, -2);
    lv_obj_set_align(ui_KeyMappedIcon, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_KeyMappedIcon, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_KeyMappedIcon, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *keymapped_title = lv_label_create(ui_KeyMapped);
    lv_label_set_recolor(keymapped_title, true);
    lv_label_set_text(keymapped_title, "KEYBOARD SETTING");
    lv_obj_set_width(keymapped_title, 300);
    lv_label_set_long_mode(keymapped_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(keymapped_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(keymapped_title, &s_keymapped_title_style, 0);
    lv_obj_align_to(keymapped_title, ui_KeyMappedIcon, LV_ALIGN_OUT_BOTTOM_MID, -16, -22);

    if (s_keymapped_icon_image_data != NULL && s_keymapped_icon_image_size > 0) {
        keymapped_apply_cached_image();
    } else {
        ui_KeyMapped_set_profile_icon_source(s_keymapped_icon_src, s_keymapped_icon_symbol);
    }

    lv_obj_add_event_cb(ui_KeyMappedButtonLeft, ui_event_ButtonLeftKeyMapped, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_KeyMappedButtonRight, ui_event_ButtonRightKeyMapped, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_KeyMapped, ui_event_KeyMappedScreen, LV_EVENT_ALL, NULL);
}

void ui_KeyMapped_screen_destroy(void)
{
    if (ui_KeyMapped) {
        lv_obj_del(ui_KeyMapped);
    }
    ui_KeyMapped = NULL;
    ui_KeyMappedButtonLeft = NULL;
    ui_KeyMappedButtonRight = NULL;
    ui_KeyMappedButtonEnter = NULL;
    ui_KeyMappedButtonExit = NULL;
    ui_KeyMappedIcon = NULL;
}
