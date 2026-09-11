/*
 * ui_AudioScreen.c
 *
 * 音效页一级屏（Sound Pad entry）。与 KeyMapped / MusicScreen 一致风格：
 * 大图标 + 标题，旋钮 ENTER 进入二级屏（AudioScreenSecondary）。
 */

#include "ui_AudioScreen.h"

#include "ui.h"

lv_obj_t *ui_AudioScreen = NULL;
lv_obj_t *ui_AudioScreenButtonLeft = NULL;
lv_obj_t *ui_AudioScreenButtonRight = NULL;
lv_obj_t *ui_AudioScreenButtonEnter = NULL;
lv_obj_t *ui_AudioScreenButtonExit = NULL;
static lv_obj_t *s_AudioScreenIcon = NULL;
static lv_style_t s_audio_screen_icon_style;
static bool s_audio_screen_icon_style_ready = false;
static lv_style_t s_audio_screen_title_style;
static bool s_audio_screen_title_style_ready = false;

static void audio_screen_forward_key(uint32_t key)
{
    lv_obj_t *active_screen = lv_scr_act();
    lv_group_t *g = lv_group_get_default();
    lv_obj_t *target = g ? lv_group_get_focused(g) : active_screen;
    if (target) {
        lv_event_send(target, LV_EVENT_KEY, (void *)key);
    }
}

void ui_event_AudioScreen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code != LV_EVENT_KEY) {
        return;
    }

    uintptr_t key = (uintptr_t)lv_event_get_param(e);
    if (key == (uintptr_t)LV_KEY_RIGHT) {
        ui_set_active_screen_tag(UI_SCREEN_PC_STATUS);
        _ui_screen_change(&ui_PcStatusScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_PcStatusScreen_screen_init);
        lv_refr_now(NULL);
    } else if (key == (uintptr_t)LV_KEY_LEFT) {
        ui_set_active_screen_tag(UI_SCREEN_MUSIC);
        _ui_screen_change(&ui_MusicScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_MusicScreen_screen_init);
        lv_refr_now(NULL);
    } else if (key == (uintptr_t)LV_KEY_ENTER) {
        ui_set_active_screen_tag(UI_SCREEN_AUDIO_SECONDARY);
        _ui_screen_change(&ui_AudioScreenSecondary, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_AudioScreenSecondary_screen_init);
        lv_refr_now(NULL);
    } else if (key == (uintptr_t)LV_KEY_ESC) {
        ui_set_active_screen_tag(UI_SCREEN_MAIN);
        _ui_screen_change(&ui_MainScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_MainScreen_screen_init);
        lv_refr_now(NULL);
    }
}

void ui_event_AudioScreenButtonLeft(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        audio_screen_forward_key(LV_KEY_LEFT);
    }
}

void ui_event_AudioScreenButtonRight(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        audio_screen_forward_key(LV_KEY_RIGHT);
    }
}

void ui_event_AudioScreenButtonEnter(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        audio_screen_forward_key(LV_KEY_ENTER);
    }
}

void ui_AudioScreen_screen_init(void)
{
    ui_AudioScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_AudioScreen, LV_OBJ_FLAG_SCROLLABLE);
    ui_set_active_screen_tag(UI_SCREEN_AUDIO);

    if (!s_audio_screen_icon_style_ready) {
        lv_style_init(&s_audio_screen_icon_style);
        lv_style_set_text_font(&s_audio_screen_icon_style, &lv_font_montserrat_48);
        s_audio_screen_icon_style_ready = true;
    }

    if (!s_audio_screen_title_style_ready) {
        lv_style_init(&s_audio_screen_title_style);
        lv_style_set_text_font(&s_audio_screen_title_style, &ui_font_FontCKJGT28);
        lv_style_set_text_letter_space(&s_audio_screen_title_style, 1);
        lv_style_set_text_color(&s_audio_screen_title_style, lv_color_hex(0xF5F8FF));
        lv_style_set_text_opa(&s_audio_screen_title_style, LV_OPA_COVER);
        s_audio_screen_title_style_ready = true;
    }

    ui_AudioScreenButtonLeft = lv_btn_create(ui_AudioScreen);
    lv_obj_set_size(ui_AudioScreenButtonLeft, 41, 28);
    lv_obj_set_align(ui_AudioScreenButtonLeft, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_AudioScreenButtonLeft, -178, -9);
    lv_obj_set_style_bg_opa(ui_AudioScreenButtonLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_AudioScreenButtonLeft, LV_OBJ_FLAG_SCROLLABLE);

    ui_AudioScreenButtonRight = lv_btn_create(ui_AudioScreen);
    lv_obj_set_size(ui_AudioScreenButtonRight, 41, 28);
    lv_obj_set_align(ui_AudioScreenButtonRight, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_AudioScreenButtonRight, 181, -10);
    lv_obj_set_style_bg_opa(ui_AudioScreenButtonRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_AudioScreenButtonRight, LV_OBJ_FLAG_SCROLLABLE);

    ui_AudioScreenButtonEnter = lv_btn_create(ui_AudioScreen);
    lv_obj_set_size(ui_AudioScreenButtonEnter, 41, 28);
    lv_obj_set_align(ui_AudioScreenButtonEnter, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_AudioScreenButtonEnter, 2, 38);
    lv_obj_set_style_bg_opa(ui_AudioScreenButtonEnter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_AudioScreenButtonEnter, LV_OBJ_FLAG_SCROLLABLE);

    ui_AudioScreenButtonExit = lv_btn_create(ui_AudioScreen);
    lv_obj_set_size(ui_AudioScreenButtonExit, 40, 20);
    lv_obj_set_align(ui_AudioScreenButtonExit, LV_ALIGN_CENTER);
    lv_obj_set_pos(ui_AudioScreenButtonExit, -181, -54);
    lv_obj_set_style_bg_opa(ui_AudioScreenButtonExit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_AudioScreenButtonExit, LV_OBJ_FLAG_SCROLLABLE);

    s_AudioScreenIcon = lv_img_create(ui_AudioScreen);
    lv_img_set_src(s_AudioScreenIcon, LV_SYMBOL_AUDIO);
    lv_obj_add_style(s_AudioScreenIcon, &s_audio_screen_icon_style, 0);
    lv_obj_set_size(s_AudioScreenIcon, 80, 80);
    lv_obj_set_x(s_AudioScreenIcon, 20);
    lv_obj_set_y(s_AudioScreenIcon, -2);
    lv_obj_set_align(s_AudioScreenIcon, LV_ALIGN_CENTER);
    lv_obj_add_flag(s_AudioScreenIcon, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(s_AudioScreenIcon, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *audio_screen_title = lv_label_create(ui_AudioScreen);
    lv_label_set_recolor(audio_screen_title, true);
    lv_label_set_text(audio_screen_title, "SOUND PAD");
    lv_obj_set_width(audio_screen_title, 300);
    lv_label_set_long_mode(audio_screen_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(audio_screen_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(audio_screen_title, &s_audio_screen_title_style, 0);
    lv_obj_align_to(audio_screen_title, s_AudioScreenIcon, LV_ALIGN_OUT_BOTTOM_MID, -16, -22);

    lv_obj_add_event_cb(ui_AudioScreenButtonLeft, ui_event_AudioScreenButtonLeft, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_AudioScreenButtonRight, ui_event_AudioScreenButtonRight, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_AudioScreenButtonEnter, ui_event_AudioScreenButtonEnter, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_AudioScreen, ui_event_AudioScreen, LV_EVENT_ALL, NULL);
}

void ui_AudioScreen_screen_destroy(void)
{
    if (ui_AudioScreen) {
        lv_obj_del(ui_AudioScreen);
    }
    ui_AudioScreen = NULL;
    ui_AudioScreenButtonLeft = NULL;
    ui_AudioScreenButtonRight = NULL;
    ui_AudioScreenButtonEnter = NULL;
    ui_AudioScreenButtonExit = NULL;
    s_AudioScreenIcon = NULL;
}
