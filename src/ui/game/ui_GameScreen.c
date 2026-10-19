/*
 * ui_GameScreen.c
 *
 * 程序化创建的前台玩法屏（docs/13 §4.1）。
 * 第一版只显示 GAME + 最近按键，用于验证 InputRouter / ForegroundKeyQueue；
 * Simon 等游戏在 GameHost 内替换这里的渲染。
 */

#include "ui/game/ui_GameScreen.h"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_label = NULL;

lv_obj_t *ui_GameScreen_create(void)
{
    if (s_screen != NULL)
    {
        return s_screen;
    }

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    s_label = lv_label_create(s_screen);
    lv_label_set_text(s_label, "GAME");
    lv_obj_set_style_text_color(s_label, lv_color_white(), 0);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label, LV_PCT(90));
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_label, LV_ALIGN_CENTER, 0, 0);

    return s_screen;
}

void ui_GameScreen_set_text(const char *text)
{
    if (s_label != NULL && text != NULL)
    {
        lv_label_set_text(s_label, text);
    }
}

void ui_GameScreen_destroy(void)
{
    if (s_screen != NULL)
    {
        lv_obj_del(s_screen);
        s_screen = NULL;
        s_label = NULL;
    }
}
