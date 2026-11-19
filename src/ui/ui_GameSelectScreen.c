/*
 * ui_GameSelectScreen.c
 *
 * 游戏选择二级页（docs/14 §4.2）：旋转选游戏，当前仅俄罗斯方块可进入。
 * 两个条目：俄罗斯方块（可选）/ 敬请期待（置灰，ENTER 弹 toast）。
 * 键值：LEFT/RIGHT→切换选中，ENTER→启动选中游戏，ESC→回游戏菜单。
 */
#include "ui_GameSelectScreen.h"

#include <stdbool.h>
#include <stdio.h>

#include "ui_GameScreen.h"
#include "ui_helpers.h"

/* TetrisGame 门面提供的 C 链接入口（进入时负责切竖屏 + 建屏 + 开局） */
extern void tetrisGame_enter(void);

/* 菜单条目定义 */
#define GAME_SELECT_COUNT 2
#define GAME_SELECT_TETRIS 0
#define GAME_SELECT_COMING 1

lv_obj_t *ui_GameSelectScreen = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_item_label[GAME_SELECT_COUNT];
static lv_obj_t *s_item_indicator[GAME_SELECT_COUNT];
static lv_obj_t *s_toast = NULL;
static lv_timer_t *s_toast_timer = NULL;
static lv_style_t s_item_style;
static bool s_item_style_ready = false;
static uint8_t s_selected = GAME_SELECT_TETRIS;
static uint8_t s_lang = 0;

/* 选中态：高亮底色 + 左侧指示条 */
static void game_select_apply(void)
{
    for (uint8_t i = 0; i < GAME_SELECT_COUNT; ++i)
    {
        if (!s_item_label[i] || !s_item_indicator[i])
        {
            continue;
        }
        const bool active = (i == s_selected);
        lv_obj_set_style_bg_opa(s_item_label[i], active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(s_item_label[i], active ? lv_color_hex(0x1D6E56)
                                                          : lv_color_hex(0x0E1420), 0);
        lv_obj_set_style_border_color(s_item_label[i], active ? lv_color_hex(0x5DCAA5)
                                                               : lv_color_hex(0x333B4A), 0);
        lv_obj_set_style_text_color(s_item_label[i], active ? lv_color_hex(0xE8FFF6)
                                                             : lv_color_hex(0x7C8798), 0);
        if (active)
        {
            lv_obj_clear_flag(s_item_indicator[i], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(s_item_indicator[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void toast_hide_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_toast)
    {
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_toast_timer)
    {
        lv_timer_del(s_toast_timer);
        s_toast_timer = NULL;
    }
}

static void game_select_show_toast(const char *text)
{
    if (!s_toast)
    {
        return;
    }
    lv_label_set_text(s_toast, text);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    if (s_toast_timer)
    {
        lv_timer_reset(s_toast_timer);
    }
    else
    {
        s_toast_timer = lv_timer_create(toast_hide_cb, 1200, NULL);
        lv_timer_set_repeat_count(s_toast_timer, 1);
    }
}

static void game_select_nav_back(void)
{
    ui_set_active_screen_tag(UI_SCREEN_GAMES);
    _ui_screen_change(&ui_GameScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0,
                      &ui_GameScreen_screen_init);
    lv_refr_now(NULL);
}

void ui_event_GameSelectScreen(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_KEY)
    {
        return;
    }
    const uintptr_t key = (uintptr_t)lv_event_get_param(e);
    if (key == (uintptr_t)LV_KEY_LEFT || key == (uintptr_t)LV_KEY_RIGHT)
    {
        s_selected = (uint8_t)((s_selected + 1) % GAME_SELECT_COUNT);
        game_select_apply();
        lv_refr_now(NULL);
    }
    else if (key == (uintptr_t)LV_KEY_ENTER)
    {
        if (s_selected == GAME_SELECT_TETRIS)
        {
            tetrisGame_enter();
        }
        else
        {
            game_select_show_toast(s_lang == 0 ? "即将上线" : "COMING SOON");
        }
    }
    else if (key == (uintptr_t)LV_KEY_ESC)
    {
        game_select_nav_back();
    }
}

void ui_GameSelectScreen_screen_init(void)
{
    ui_GameSelectScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_GameSelectScreen, LV_OBJ_FLAG_SCROLLABLE);
    ui_set_active_screen_tag(UI_SCREEN_GAME_SELECT);
    s_selected = GAME_SELECT_TETRIS;

    if (!s_item_style_ready)
    {
        lv_style_init(&s_item_style);
        lv_style_set_radius(&s_item_style, 10);
        lv_style_set_border_width(&s_item_style, 1);
        s_item_style_ready = true;
    }

    s_title = lv_label_create(ui_GameSelectScreen);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0xF5F8FF), 0);

    for (uint8_t i = 0; i < GAME_SELECT_COUNT; ++i)
    {
        s_item_label[i] = lv_label_create(ui_GameSelectScreen);
        lv_obj_set_width(s_item_label[i], 300);
        lv_obj_align(s_item_label[i], LV_ALIGN_CENTER, 20,
                     i == 0 ? -22 : 26);
        lv_obj_set_height(s_item_label[i], 42);
        lv_obj_add_style(s_item_label[i], &s_item_style, 0);
        lv_obj_set_style_pad_left(s_item_label[i], 16, 0);
        lv_obj_set_style_pad_top(s_item_label[i], 12, 0);
        lv_obj_set_style_text_font(s_item_label[i], &ui_font_FontCKJGT16, 0);
        lv_obj_clear_flag(s_item_label[i], LV_OBJ_FLAG_SCROLLABLE);

        s_item_indicator[i] = lv_obj_create(ui_GameSelectScreen);
        lv_obj_set_size(s_item_indicator[i], 4, 30);
        lv_obj_set_style_bg_color(s_item_indicator[i], lv_color_hex(0x5DCAA5), 0);
        lv_obj_set_style_border_width(s_item_indicator[i], 0, 0);
        lv_obj_set_style_pad_all(s_item_indicator[i], 0, 0);
        lv_obj_align_to(s_item_indicator[i], s_item_label[i],
                        LV_ALIGN_OUT_LEFT_MID, -6, 0);
        lv_obj_clear_flag(s_item_indicator[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    s_toast = lv_label_create(ui_GameSelectScreen);
    lv_obj_set_style_text_font(s_toast, &ui_font_FontCKJGT16, 0);
    lv_obj_set_style_text_color(s_toast, lv_color_hex(0xF5C4B3), 0);
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(0x202632), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_toast, 8, 0);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);

    ui_GameSelectScreen_apply_language(s_lang);
    game_select_apply();
    lv_obj_add_event_cb(ui_GameSelectScreen, ui_event_GameSelectScreen,
                        LV_EVENT_ALL, NULL);
}

void ui_GameSelectScreen_apply_language(uint8_t lang)
{
    s_lang = lang > 1 ? 1 : lang;
    if (!s_title)
    {
        return;
    }
    if (s_lang == 0)
    {
        lv_label_set_text(s_title, "选择游戏");
        lv_obj_set_style_text_font(s_title, &ui_font_FontCKJGT24, 0);
        lv_label_set_text(s_item_label[0], "俄罗斯方块  TETRIS");
        lv_label_set_text(s_item_label[1], "敬请期待  COMING SOON");
    }
    else
    {
        lv_label_set_text(s_title, "SELECT GAME");
        lv_obj_set_style_text_font(s_title, &ui_font_FontCKJGT28, 0);
        lv_label_set_text(s_item_label[0], "TETRIS");
        lv_label_set_text(s_item_label[1], "COMING SOON");
    }
}

void ui_GameSelectScreen_screen_destroy(void)
{
    if (s_toast_timer)
    {
        lv_timer_del(s_toast_timer);
        s_toast_timer = NULL;
    }
    if (ui_GameSelectScreen)
    {
        lv_obj_del(ui_GameSelectScreen);
    }
    ui_GameSelectScreen = NULL;
    s_title = NULL;
    s_toast = NULL;
    for (uint8_t i = 0; i < GAME_SELECT_COUNT; ++i)
    {
        s_item_label[i] = NULL;
        s_item_indicator[i] = NULL;
    }
}
