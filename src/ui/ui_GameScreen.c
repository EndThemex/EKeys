/*
 * ui_GameScreen.c
 *
 * 游戏菜单一级页（docs/14 §4.1）：大图标 + 标题，与其它一级页一致。
 * 轮播插入点：AI --RIGHT--> GAMES --RIGHT--> PC_STATUS（反向 LEFT 对应）。
 * 键值：LEFT→AI，RIGHT→PC_STATUS，ENTER→游戏选择二级页，ESC→主页。
 */
#include "ui_GameScreen.h"

#include <stdbool.h>

#include "ui_AiScreen.h"
#include "ui_GameSelectScreen.h"
#include "ui_helpers.h"
#include "ui_MainScreen.h"
#include "ui_PcStatusScreen.h"

lv_obj_t *ui_GameScreen = NULL;
static lv_obj_t *s_game_icon = NULL;
static lv_obj_t *s_game_icon_label = NULL;
static lv_obj_t *s_game_title = NULL;
static lv_style_t s_game_icon_style;
static bool s_game_icon_style_ready = false;
static lv_style_t s_game_title_style;
static bool s_game_title_style_ready = false;
static uint8_t s_game_lang = 0;

static void game_nav(lv_obj_t **screen, ui_screen_tag_t tag,
                     void (*init)(void))
{
    ui_set_active_screen_tag(tag);
    _ui_screen_change(screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, init);
    lv_refr_now(NULL);
}

void ui_event_GameScreen(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_KEY)
    {
        return;
    }
    const uintptr_t key = (uintptr_t)lv_event_get_param(e);
    if (key == (uintptr_t)LV_KEY_RIGHT)
    {
        game_nav(&ui_PcStatusScreen, UI_SCREEN_PC_STATUS,
                 &ui_PcStatusScreen_screen_init);
    }
    else if (key == (uintptr_t)LV_KEY_LEFT)
    {
        game_nav(&ui_AiScreen, UI_SCREEN_AI, &ui_AiScreen_screen_init);
    }
    else if (key == (uintptr_t)LV_KEY_ENTER)
    {
        game_nav(&ui_GameSelectScreen, UI_SCREEN_GAME_SELECT,
                 &ui_GameSelectScreen_screen_init);
    }
    else if (key == (uintptr_t)LV_KEY_ESC)
    {
        game_nav(&ui_MainScreen, UI_SCREEN_MAIN, &ui_MainScreen_screen_init);
    }
}

void ui_GameScreen_screen_init(void)
{
    ui_GameScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_GameScreen, LV_OBJ_FLAG_SCROLLABLE);
    ui_set_active_screen_tag(UI_SCREEN_GAMES);

    if (!s_game_icon_style_ready)
    {
        lv_style_init(&s_game_icon_style);
        lv_style_set_bg_opa(&s_game_icon_style, LV_OPA_TRANSP);
        lv_style_set_border_width(&s_game_icon_style, 3);
        lv_style_set_border_color(&s_game_icon_style, lv_color_hex(0xF5F8FF));
        lv_style_set_border_opa(&s_game_icon_style, LV_OPA_COVER);
        lv_style_set_radius(&s_game_icon_style, 16);
        s_game_icon_style_ready = true;
    }
    if (!s_game_title_style_ready)
    {
        lv_style_init(&s_game_title_style);
        lv_style_set_text_font(&s_game_title_style, &ui_font_FontCKJGT28);
        lv_style_set_text_letter_space(&s_game_title_style, 1);
        lv_style_set_text_color(&s_game_title_style, lv_color_hex(0xF5F8FF));
        lv_style_set_text_opa(&s_game_title_style, LV_OPA_COVER);
        s_game_title_style_ready = true;
    }

    /* 图标：描边圆角方块 + "G"，沿用 AI 一级页的尺寸规范。
     * 64×64 + 圆角 16 与 48px 字形可视面积对齐，
     * 配合下方标题正间距，避免描边框与标题文字重叠。 */
    s_game_icon = lv_obj_create(ui_GameScreen);
    lv_obj_set_size(s_game_icon, 64, 64);
    lv_obj_set_align(s_game_icon, LV_ALIGN_CENTER);
    lv_obj_set_x(s_game_icon, 4);
    lv_obj_set_y(s_game_icon, -18);
    lv_obj_add_style(s_game_icon, &s_game_icon_style, 0);
    lv_obj_clear_flag(s_game_icon, LV_OBJ_FLAG_SCROLLABLE);

    s_game_icon_label = lv_label_create(s_game_icon);
    lv_label_set_text(s_game_icon_label, "G");
    lv_obj_set_style_text_font(s_game_icon_label, &ui_font_BebasNeueFont48, 0);
    lv_obj_set_style_text_color(s_game_icon_label, lv_color_hex(0xF5F8FF), 0);
    lv_obj_center(s_game_icon_label);

    s_game_title = lv_label_create(ui_GameScreen);
    lv_obj_set_width(s_game_title, 300);
    lv_label_set_long_mode(s_game_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_game_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_game_title, &s_game_title_style, 0);
    lv_obj_align_to(s_game_title, s_game_icon, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    ui_GameScreen_apply_language(s_game_lang);
    lv_obj_add_event_cb(ui_GameScreen, ui_event_GameScreen, LV_EVENT_ALL, NULL);
}

void ui_GameScreen_apply_language(uint8_t lang)
{
    s_game_lang = lang > 1 ? 1 : lang;
    if (!s_game_title)
    {
        return;
    }
    if (s_game_lang == 0)
    {
        lv_label_set_text(s_game_title, "游戏中心");
        lv_obj_set_style_text_font(s_game_title, &ui_font_FontCKJGT24, 0);
    }
    else
    {
        lv_label_set_text(s_game_title, "GAMES");
        lv_obj_set_style_text_font(s_game_title, &ui_font_FontCKJGT28, 0);
    }
}

void ui_GameScreen_screen_destroy(void)
{
    if (ui_GameScreen)
    {
        lv_obj_del(ui_GameScreen);
    }
    ui_GameScreen = NULL;
    s_game_icon = NULL;
    s_game_icon_label = NULL;
    s_game_title = NULL;
}
