/*
 * ui_AiScreen.c
 *
 * AI Mode 一级入口页。与 KeyMapped / Music / Audio / PcStatus 等一级页一致：
 * 大图标 + 标题，旋钮单击（ENTER）才进入二级功能页 ui_AiScreenSecondary，
 * 进入后才启动串口 / WiFi 收发（避免仅翻页就开启握手）。
 *
 * 键值约定：LEFT→音效页，RIGHT→PC 状态页，ENTER→AI 功能页，ESC→主页。
 */

#include "ui_AiScreen.h"

#include <stdbool.h>

#include "ui_AiScreenSecondary.h"
#include "ui_AudioScreen.h"
#include "ui_helpers.h"
#include "ui_MainScreen.h"
#include "ui_PcStatusScreen.h"

lv_obj_t *ui_AiScreen = NULL;
static lv_obj_t *s_AiIcon = NULL;
static lv_obj_t *s_AiIconLabel = NULL;
static lv_obj_t *s_AiTitle = NULL;
static lv_style_t s_ai_icon_style;
static bool s_ai_icon_style_ready = false;
static lv_style_t s_ai_title_style;
static bool s_ai_title_style_ready = false;

void ui_event_AiScreen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code != LV_EVENT_KEY)
    {
        return;
    }

    uintptr_t key = (uintptr_t)lv_event_get_param(e);
    if (key == (uintptr_t)LV_KEY_RIGHT)
    {
        ui_set_active_screen_tag(UI_SCREEN_PC_STATUS);
        _ui_screen_change(&ui_PcStatusScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_PcStatusScreen_screen_init);
        lv_refr_now(NULL);
    }
    else if (key == (uintptr_t)LV_KEY_LEFT)
    {
        ui_set_active_screen_tag(UI_SCREEN_AUDIO);
        _ui_screen_change(&ui_AudioScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_AudioScreen_screen_init);
        lv_refr_now(NULL);
    }
    else if (key == (uintptr_t)LV_KEY_ENTER)
    {
        ui_set_active_screen_tag(UI_SCREEN_AI_SECONDARY);
        _ui_screen_change(&ui_AiScreenSecondary, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_AiScreenSecondary_screen_init);
        lv_refr_now(NULL);
    }
    else if (key == (uintptr_t)LV_KEY_ESC)
    {
        ui_set_active_screen_tag(UI_SCREEN_MAIN);
        _ui_screen_change(&ui_MainScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_MainScreen_screen_init);
        lv_refr_now(NULL);
    }
}

void ui_AiScreen_screen_init(void)
{
    ui_AiScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_AiScreen, LV_OBJ_FLAG_SCROLLABLE);
    ui_set_active_screen_tag(UI_SCREEN_AI);

    if (!s_ai_icon_style_ready)
    {
        lv_style_init(&s_ai_icon_style);
        lv_style_set_bg_opa(&s_ai_icon_style, LV_OPA_TRANSP);
        lv_style_set_border_width(&s_ai_icon_style, 3);
        lv_style_set_border_color(&s_ai_icon_style, lv_color_hex(0xF5F8FF));
        lv_style_set_border_opa(&s_ai_icon_style, LV_OPA_COVER);
        lv_style_set_radius(&s_ai_icon_style, 18);
        s_ai_icon_style_ready = true;
    }

    if (!s_ai_title_style_ready)
    {
        lv_style_init(&s_ai_title_style);
        lv_style_set_text_font(&s_ai_title_style, &ui_font_FontCKJGT28);
        lv_style_set_text_letter_space(&s_ai_title_style, 1);
        lv_style_set_text_color(&s_ai_title_style, lv_color_hex(0xF5F8FF));
        lv_style_set_text_opa(&s_ai_title_style, LV_OPA_COVER);
        s_ai_title_style_ready = true;
    }

    /* 图标：圆角描边方块 + "AI"（ui_font_Symbol48 未收录图片符号，
     * 沿用 docs/13 §8.1 的"文字 + 描边矩形"占位方案） */
    s_AiIcon = lv_obj_create(ui_AiScreen);
    lv_obj_set_size(s_AiIcon, 80, 80);
    lv_obj_set_x(s_AiIcon, 20);
    lv_obj_set_y(s_AiIcon, -2);
    lv_obj_set_align(s_AiIcon, LV_ALIGN_CENTER);
    lv_obj_add_style(s_AiIcon, &s_ai_icon_style, 0);
    lv_obj_clear_flag(s_AiIcon, LV_OBJ_FLAG_SCROLLABLE);

    s_AiIconLabel = lv_label_create(s_AiIcon);
    lv_label_set_text(s_AiIconLabel, "AI");
    lv_obj_set_style_text_font(s_AiIconLabel, &ui_font_BebasNeueFont48, 0);
    lv_obj_set_style_text_color(s_AiIconLabel, lv_color_hex(0xF5F8FF), 0);
    lv_obj_center(s_AiIconLabel);

    s_AiTitle = lv_label_create(ui_AiScreen);
    lv_label_set_text(s_AiTitle, "AI MODE");
    lv_obj_set_width(s_AiTitle, 300);
    lv_label_set_long_mode(s_AiTitle, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_AiTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_AiTitle, &s_ai_title_style, 0);
    lv_obj_align_to(s_AiTitle, s_AiIcon, LV_ALIGN_OUT_BOTTOM_MID, -16, -22);

    lv_obj_add_event_cb(ui_AiScreen, ui_event_AiScreen, LV_EVENT_ALL, NULL);
}

void ui_AiScreen_apply_language(uint8_t lang)
{
    if (!s_AiTitle)
    {
        return;
    }
    /* 标题样式字体 FontCKJGT28 仅 ASCII，中文需逐对象覆盖为 FontCKJGT24 */
    lv_label_set_text(s_AiTitle, lang == 0 ? "AI 模式" : "AI MODE");
    lv_obj_set_style_text_font(s_AiTitle,
                               lang == 0 ? &ui_font_FontCKJGT24 : &ui_font_FontCKJGT28, 0);
}

void ui_AiScreen_screen_destroy(void)
{
    if (ui_AiScreen)
    {
        lv_obj_del(ui_AiScreen);
    }
    ui_AiScreen = NULL;
    s_AiIcon = NULL;
    s_AiIconLabel = NULL;
    s_AiTitle = NULL;
}
