/*
 * ui_GameScreen.c
 *
 * 程序化创建的前台玩法屏（docs/13 §4.1）。
 * Simon（docs/13 Step 4）：中央状态文本 + 底部 11 格按键指示条。
 * 不走 SquareLine 生成，避免重导出覆盖；主题系统后续再接入。
 */

#include "ui/game/ui_GameScreen.h"

/* 底部指示条：11 格，固定几何（屏宽 428，见 LvglPort.cpp）。
 * 格数与矩阵键 1~11 对齐（kInputKeyCount，static_assert 在 MainTask.cpp） */
#define GAME_SCREEN_CELL_COUNT 11
#define GAME_SCREEN_CELL_W 28
#define GAME_SCREEN_CELL_H 26
#define GAME_SCREEN_CELL_GAP 6
#define GAME_SCREEN_CELL_RADIUS 4

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_label = NULL;
static lv_obj_t *s_cells[GAME_SCREEN_CELL_COUNT] = {NULL};

/* 熄灭/点亮配色：暗灰 / 琥珀 */
#define GAME_CELL_COLOR_OFF LV_COLOR_MAKE(0x2A, 0x2A, 0x2A)
#define GAME_CELL_COLOR_ON LV_COLOR_MAKE(0xFF, 0xC1, 0x07)

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

    /* 中央状态文本：上移给底部指示条留空间 */
    s_label = lv_label_create(s_screen);
    lv_label_set_text(s_label, "");
    lv_obj_set_style_text_color(s_label, lv_color_white(), 0);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_label, LV_PCT(90));
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, 0);
#if LV_FONT_MONTSERRAT_24
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_24, 0);
#endif
    lv_obj_align(s_label, LV_ALIGN_CENTER, 0, -14);

    /* 底部 11 格指示条：整体居中，格间等距 */
    const lv_coord_t step = GAME_SCREEN_CELL_W + GAME_SCREEN_CELL_GAP;
    const lv_coord_t total_w =
        GAME_SCREEN_CELL_COUNT * GAME_SCREEN_CELL_W +
        (GAME_SCREEN_CELL_COUNT - 1) * GAME_SCREEN_CELL_GAP;
    for (int i = 0; i < GAME_SCREEN_CELL_COUNT; ++i)
    {
        lv_obj_t *cell = lv_obj_create(s_screen);
        lv_obj_remove_style_all(cell);
        lv_obj_set_size(cell, GAME_SCREEN_CELL_W, GAME_SCREEN_CELL_H);
        lv_obj_set_style_radius(cell, GAME_SCREEN_CELL_RADIUS, 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(cell, GAME_CELL_COLOR_OFF, 0);
        /* 第一格左缘对齐整体起点；BOTTOM_MID x 相对屏中心 */
        lv_obj_align(cell, LV_ALIGN_BOTTOM_MID,
                     -total_w / 2 + i * step, -6);
        s_cells[i] = cell;
    }

    return s_screen;
}

void ui_GameScreen_set_text(const char *text)
{
    if (s_label != NULL && text != NULL)
    {
        lv_label_set_text(s_label, text);
    }
}

void ui_GameScreen_set_cell(uint8_t key_id, bool on)
{
    /* 矩阵键 1~11（kInputKeyCount 与 MatrixScanner 对齐，static_assert
     * 在 MainTask.cpp）；指示条下标 = key_id - 1 */
    if (key_id < 1 || key_id > GAME_SCREEN_CELL_COUNT)
    {
        return;
    }
    lv_obj_t *cell = s_cells[key_id - 1];
    if (cell != NULL)
    {
        lv_obj_set_style_bg_color(cell, on ? GAME_CELL_COLOR_ON : GAME_CELL_COLOR_OFF,
                                  0);
    }
}

void ui_GameScreen_set_all_cells(bool on)
{
    for (uint8_t k = 1; k <= GAME_SCREEN_CELL_COUNT; ++k)
    {
        ui_GameScreen_set_cell(k, on);
    }
}

void ui_GameScreen_destroy(void)
{
    if (s_screen != NULL)
    {
        lv_obj_del(s_screen);
        s_screen = NULL;
        s_label = NULL;
        for (int i = 0; i < GAME_SCREEN_CELL_COUNT; ++i)
        {
            s_cells[i] = NULL;
        }
    }
}
