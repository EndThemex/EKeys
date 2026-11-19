/*
 * ui_TetrisScreen.c
 *
 * 俄罗斯方块竖屏游戏页（docs/14 §4.3）：142×428。
 *   - 屏幕左侧 3 列 × 4 行键位图，与竖屏实体键一一对应（末行两键 + 旋钮）
 *   - 右侧 HUD：分数 / 等级 / 消行 / NEXT×3 / HOLD
 *   - 10×20 索引 4-bit 画布（120×240，cell=12px）
 *   - 底部提示与状态覆盖层（READY / PAUSED / GAME OVER）
 *
 * 棋盘 buffer 从 PSRAM 分配（14.5KB），不占本已紧张的 128KB LVGL 池；
 * 直接写 4bit 像素后一次性 invalidate，避免 lv_canvas_set_px 逐像素刷新。
 * 本页不注册 LV_EVENT_KEY（旋钮由 DisplayTask 直接送 TetrisGame 门面）。
 */
#include "ui_TetrisScreen.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "game/TetrisPalette.h"
#include "ui_helpers.h"

/* ---------- 布局常量（竖屏 142×428，与 game/TetrisConfig.h 的 kCols/
 * kVisibleRows/kCellPx 保持一致；该文件为 C++ 无法直接 include，故本地定义） ---------- */
#define BOARD_COLS 10
#define BOARD_ROWS 20
#define CELL_PX 12
#define BOARD_W (BOARD_COLS * CELL_PX) /* 120 */
#define BOARD_H (BOARD_ROWS * CELL_PX) /* 240 */
#define BOARD_X 11                     /* (142-120)/2 */
#define BOARD_Y 120

#define ACTION_COUNT 11 /* K1~K11 全部有功能 */
#define GRID_COLS TETRIS_KEY_GRID_COLS
#define GRID_ROWS TETRIS_KEY_GRID_ROWS
#define GRID_SLOTS (GRID_COLS * GRID_ROWS)

lv_obj_t *ui_TetrisScreen = NULL;
static lv_obj_t *s_board = NULL;
static lv_obj_t *s_score_label = NULL;
static lv_obj_t *s_level_label = NULL;
static lv_obj_t *s_lines_label = NULL;
static lv_obj_t *s_combo_label = NULL;
static lv_obj_t *s_next_label[3];
static lv_obj_t *s_hold_label = NULL;
static lv_obj_t *s_status = NULL;
static lv_obj_t *s_hint = NULL;
static lv_obj_t *s_action_chip[ACTION_COUNT];
static lv_obj_t *s_knob_chip = NULL;

static uint8_t *s_board_buf = NULL; /* PSRAM，含 64B 调色板头 */
static uint8_t s_lang = 0;

/* 动作索引 = 功能槽位（与物理键颜色/LED 下标一一对应，改动功能只需改文字）：
 *   0=重开(K1) 1=右移(K2) 2=无 3=暂停(K4) 4=旋转(K5) 5=硬降(K6)
 *   6=无 7=退出(K8) 8=左移(K9) 9=无 10=无
 * 无功能槽位文字显示占位符 "-"，底色保留对应物理键的 LED 色。 */
static const char *s_action_text[ACTION_COUNT][2] = {
    {"重", "RS"},  {"右", "R"},   {"-", "-"},    {"停", "P"},
    {"转", "ROT"}, {"硬", "D"},   {"-", "-"},    {"出", "X"},
    {"左", "L"},   {"-", "-"},   {"-", "-"}};

static const uint8_t s_action_rgb[ACTION_COUNT][3] = {
    {TETRIS_ACTION_RGB_0},  {TETRIS_ACTION_RGB_1},
    {TETRIS_ACTION_RGB_2},  {TETRIS_ACTION_RGB_3},
    {TETRIS_ACTION_RGB_4},  {TETRIS_ACTION_RGB_5},
    {TETRIS_ACTION_RGB_6},  {TETRIS_ACTION_RGB_7},
    {TETRIS_ACTION_RGB_8},  {TETRIS_ACTION_RGB_9},
    {TETRIS_ACTION_RGB_10}};

/*
 * 键位图槽位 → 动作索引，顺序必须与 TetrisPalette.h 的
 * TETRIS_KEY_GRID_IDS {K8,K4,K1, K9,K5,K2, K10,K6,K3, K11,K7,旋钮} 一致。
 * -1 表示该槽位是 EC11 旋钮（无矩阵键、无 LED）。
 */
static const int8_t s_grid_keys[GRID_SLOTS] = TETRIS_KEY_GRID_IDS;
/* 槽位 → 功能索引（= key_id - 1，颜色/文字/LED 共用此下标）：
 *   K8=退出 / K4=暂停 / K1=重开 / K9=左移 / K5=旋转 / K2=右移
 *   K10/K3/K11/K7=暂无功能 / 旋钮 */
static const int8_t s_grid_action[GRID_SLOTS] = {
    7, 3, 0,  /* K8 退出 / K4 暂停 / K1 重开 */
    8, 4, 1,  /* K9 左移 / K5 旋转 / K2 右移 */
    9, 5, 2,  /* K10 无 / K6 硬降 / K3 无 */
    10, 6, -1 /* K11 无 / K7 无 / 旋钮 */
};

/* 亮底色需要用深色文字 */
static bool action_needs_dark_text(uint8_t action)
{
    return action == 2 || action == 4 || action == 6 ||
           action == 8 || action == 9 || action == 10;
}

/* 直接写 4bit 索引像素：跳过 16×lv_color32_t 调色板头，x 偶数高半字节 */
static inline void board_set_px(uint8_t x, uint8_t y, uint8_t idx)
{
    uint8_t *px = s_board_buf + sizeof(lv_color32_t) * 16;
    const uint32_t stride = (BOARD_W + 1) >> 1;
    uint8_t *cell = &px[stride * y + (x >> 1)];
    const uint8_t shift = (x & 1) ? 0 : 4;
    *cell = (uint8_t)((*cell & ~(0x0F << shift)) | ((idx & 0x0F) << shift));
}

void ui_TetrisScreen_draw_board(const uint8_t cells[20][10])
{
    if (!s_board || !s_board_buf)
    {
        return;
    }
    for (uint8_t row = 0; row < BOARD_ROWS; ++row)
    {
        for (uint8_t col = 0; col < BOARD_COLS; ++col)
        {
            const uint8_t idx = cells[row][col] & 0x0F;
            const uint8_t px0 = col * CELL_PX;
            const uint8_t py0 = row * CELL_PX;
            for (uint8_t dy = 0; dy < CELL_PX; ++dy)
            {
                for (uint8_t dx = 0; dx < CELL_PX; ++dx)
                {
                    board_set_px((uint8_t)(px0 + dx), (uint8_t)(py0 + dy), idx);
                }
            }
        }
    }
    lv_obj_invalidate(s_board);
}

void ui_TetrisScreen_set_hud(uint32_t score, uint8_t level,
                             uint32_t lines, int16_t combo)
{
    char buf[24];
    if (s_score_label)
    {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)score);
        lv_label_set_text(s_score_label, buf);
    }
    if (s_level_label)
    {
        snprintf(buf, sizeof(buf), "LV %u", (unsigned)level);
        lv_label_set_text(s_level_label, buf);
    }
    if (s_lines_label)
    {
        snprintf(buf, sizeof(buf), "LN %lu", (unsigned long)lines);
        lv_label_set_text(s_lines_label, buf);
    }
    if (s_combo_label)
    {
        if (combo > 0)
        {
            snprintf(buf, sizeof(buf), "COMBO %d", combo);
            lv_label_set_text(s_combo_label, buf);
            lv_obj_clear_flag(s_combo_label, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(s_combo_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static const char *piece_name(int8_t p)
{
    static const char *names[7] = {"I", "O", "T", "S", "Z", "J", "L"};
    if (p < 0 || p > 6)
    {
        return "-";
    }
    return names[p];
}

void ui_TetrisScreen_set_preview(int8_t next0, int8_t next1, int8_t next2,
                                 int8_t hold, bool hold_used)
{
    if (s_next_label[0])
    {
        lv_label_set_text(s_next_label[0], piece_name(next0));
    }
    if (s_next_label[1])
    {
        lv_label_set_text(s_next_label[1], piece_name(next1));
    }
    if (s_next_label[2])
    {
        lv_label_set_text(s_next_label[2], piece_name(next2));
    }
    if (s_hold_label)
    {
        lv_label_set_text(s_hold_label, hold < 0 ? "-" : piece_name(hold));
        lv_obj_set_style_text_opa(s_hold_label,
                                  hold_used ? LV_OPA_40 : LV_OPA_COVER, 0);
    }
}

void ui_TetrisScreen_set_status(uint8_t state, const char *text)
{
    if (!s_status)
    {
        return;
    }
    if (state == 0)
    {
        lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_status, LV_OBJ_FLAG_HIDDEN);
    if (text != NULL)
    {
        lv_label_set_text(s_status, text);
    }
    else if (state == 1)
    {
        lv_label_set_text(s_status, s_lang == 0 ? "已暂停\n单击继续 · 双击退出" : "PAUSED\nCLICK RESUME");
    }
    else
    {
        lv_label_set_text(s_status, s_lang == 0 ? "游戏结束\n单击重开 · 双击退出" : "GAME OVER\nCLICK RESTART");
    }
}

void ui_TetrisScreen_apply_language(uint8_t lang)
{
    s_lang = lang > 1 ? 1 : lang;
    if (!s_status)
    {
        return;
    }
    for (uint8_t i = 0; i < ACTION_COUNT; ++i)
    {
        if (s_action_chip[i])
        {
            lv_label_set_text(s_action_chip[i], s_action_text[i][s_lang]);
            lv_obj_set_style_text_font(s_action_chip[i],
                                       s_lang == 0 ? &ui_font_FontCKJGT16
                                                   : &ui_font_BebasNeueFont14,
                                       0);
        }
    }
    if (s_knob_chip)
    {
        lv_label_set_text(s_knob_chip, s_lang == 0 ? "旋钮" : "KNOB");
        lv_obj_set_style_text_font(s_knob_chip, s_lang == 0
                                                    ? &ui_font_FontCKJGT16
                                                    : &ui_font_BebasNeueFont14,
                                   0);
    }
    if (s_hint)
    {
        lv_label_set_text(s_hint, s_lang == 0
                                      ? "旋钮:移动/单击转\n双击暂停"
                                      : "KNOB: MOVE/ROT\nDBL-CLICK PAUSE");
        lv_obj_set_style_text_font(s_hint, s_lang == 0
                                               ? &ui_font_FontCKJGT16
                                               : &ui_font_BebasNeueFont14,
                                   0);
    }
}

void ui_TetrisScreen_screen_init(void)
{
    ui_TetrisScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_TetrisScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_TetrisScreen, lv_color_hex(0x0A0E18), 0);

    /* ---- 左侧 3 列 × 4 行键位图：与竖屏实体键位置一一对应 ---- */
    const uint8_t chip_w = 24;
    const uint8_t chip_h = 17;
    const uint8_t gap_x = 2;
    const uint8_t gap_y = 2;
    const uint8_t grid_x = 2;
    const uint8_t grid_y = 2;
    for (uint8_t slot = 0; slot < GRID_SLOTS; ++slot)
    {
        const uint8_t col = slot % GRID_COLS;
        const uint8_t row = slot / GRID_COLS;
        const int8_t key_id = s_grid_keys[slot];
        const int8_t action = s_grid_action[slot];
        lv_obj_t *chip = lv_label_create(ui_TetrisScreen);
        lv_obj_set_size(chip, chip_w, chip_h);
        lv_obj_set_pos(chip, grid_x + col * (chip_w + gap_x),
                       grid_y + row * (chip_h + gap_y));
        lv_obj_set_style_pad_all(chip, 0, 0);
        lv_obj_set_style_text_align(chip, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);

        if (key_id < 0)
        {
            /* 末行第三格是旋钮：灰色圆角块，无 LED */
            s_knob_chip = chip;
            lv_obj_set_style_radius(chip, 9, 0);
            lv_obj_set_style_bg_color(chip, lv_color_hex(0x2A3340), 0);
            lv_obj_set_style_text_color(chip, lv_color_hex(0xB8C2D0), 0);
            lv_obj_set_style_text_font(chip, &ui_font_BebasNeueFont14, 0);
        }
        else
        {
            s_action_chip[action] = chip;
            lv_obj_set_style_radius(chip, 4, 0);
            lv_obj_set_style_bg_color(chip,
                                      lv_color_make(s_action_rgb[action][0],
                                                    s_action_rgb[action][1],
                                                    s_action_rgb[action][2]),
                                      0);
            lv_obj_set_style_text_color(chip,
                                        action_needs_dark_text(action)
                                            ? lv_color_hex(0x0A0E18)
                                            : lv_color_hex(0xFFFFFF),
                                        0);
        }
    }

    /* ---- 右侧 HUD：分数 / 等级 / 消行 / HOLD / NEXT×3 ---- */
    s_score_label = lv_label_create(ui_TetrisScreen);
    lv_obj_set_pos(s_score_label, 82, 2);
    lv_obj_set_width(s_score_label, 56);
    lv_obj_set_style_text_font(s_score_label, &ui_font_BebasNeueFont24, 0);
    lv_obj_set_style_text_color(s_score_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(s_score_label, "0");

    s_level_label = lv_label_create(ui_TetrisScreen);
    lv_obj_set_pos(s_level_label, 82, 28);
    lv_obj_set_style_text_font(s_level_label, &ui_font_BebasNeueFont14, 0);
    lv_obj_set_style_text_color(s_level_label, lv_color_hex(0x9FE1CB), 0);
    lv_label_set_text(s_level_label, "LV 1");

    s_lines_label = lv_label_create(ui_TetrisScreen);
    lv_obj_set_pos(s_lines_label, 82, 44);
    lv_obj_set_style_text_font(s_lines_label, &ui_font_BebasNeueFont14, 0);
    lv_obj_set_style_text_color(s_lines_label, lv_color_hex(0x85B7EB), 0);
    lv_label_set_text(s_lines_label, "LN 0");

    s_hold_label = lv_label_create(ui_TetrisScreen);
    lv_obj_set_pos(s_hold_label, 82, 62);
    lv_obj_set_size(s_hold_label, 20, 20);
    lv_obj_set_style_bg_color(s_hold_label, lv_color_hex(0x121826), 0);
    lv_obj_set_style_bg_opa(s_hold_label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_hold_label, lv_color_hex(0xB5D4F4), 0);
    lv_obj_set_style_text_align(s_hold_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_hold_label, &ui_font_BebasNeueFont16, 0);

    for (uint8_t i = 0; i < 3; ++i)
    {
        s_next_label[i] = lv_label_create(ui_TetrisScreen);
        lv_obj_set_pos(s_next_label[i], 82 + i * 20, 84);
        lv_obj_set_size(s_next_label[i], 18, 18);
        lv_obj_set_style_bg_color(s_next_label[i], lv_color_hex(0x121826), 0);
        lv_obj_set_style_bg_opa(s_next_label[i], LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(s_next_label[i], lv_color_hex(0xE8FFF6), 0);
        lv_obj_set_style_text_align(s_next_label[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(s_next_label[i], &ui_font_BebasNeueFont16, 0);
    }

    /* ---- 10×20 索引 4bit 棋盘画布 ---- */
    if (!s_board_buf)
    {
        const size_t buf_bytes = LV_CANVAS_BUF_SIZE_INDEXED_4BIT(BOARD_W, BOARD_H);
        s_board_buf = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_board_buf)
        {
            s_board_buf = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_8BIT);
        }
    }
    s_board = lv_canvas_create(ui_TetrisScreen);
    lv_obj_set_pos(s_board, BOARD_X, BOARD_Y);
    lv_obj_set_size(s_board, BOARD_W, BOARD_H);
    if (s_board_buf)
    {
        memset(s_board_buf, 0, LV_CANVAS_BUF_SIZE_INDEXED_4BIT(BOARD_W, BOARD_H));
        lv_canvas_set_buffer(s_board, s_board_buf, BOARD_W, BOARD_H,
                             LV_IMG_CF_INDEXED_4BIT);
        const uint8_t pal[9][3] = {
            {TETRIS_PIECE_RGB_0}, {TETRIS_PIECE_RGB_1},
            {TETRIS_PIECE_RGB_2}, {TETRIS_PIECE_RGB_3},
            {TETRIS_PIECE_RGB_4}, {TETRIS_PIECE_RGB_5},
            {TETRIS_PIECE_RGB_6}, {TETRIS_PIECE_RGB_7},
            {TETRIS_PIECE_RGB_8}};
        for (uint8_t i = 0; i < 9; ++i)
        {
            lv_canvas_set_palette(s_board, i,
                                  lv_color_make(pal[i][0], pal[i][1], pal[i][2]));
        }
    }

    s_combo_label = lv_label_create(ui_TetrisScreen);
    lv_obj_set_pos(s_combo_label, 8, 362);
    lv_obj_set_style_text_font(s_combo_label, &ui_font_BebasNeueFont16, 0);
    lv_obj_set_style_text_color(s_combo_label, lv_color_hex(0xFAC775), 0);
    lv_obj_add_flag(s_combo_label, LV_OBJ_FLAG_HIDDEN);

    s_hint = lv_label_create(ui_TetrisScreen);
    lv_obj_set_pos(s_hint, 2, 384);
    lv_obj_set_width(s_hint, 138);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0x7C8798), 0);

    s_status = lv_label_create(ui_TetrisScreen);
    lv_obj_set_size(s_status, 128, 60);
    lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_text_font(s_status, &ui_font_FontCKJGT16, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(s_status, lv_color_hex(0x0A0E18), 0);
    lv_obj_set_style_bg_opa(s_status, LV_OPA_80, 0);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);

    ui_TetrisScreen_apply_language(s_lang);
}

void ui_TetrisScreen_screen_destroy(void)
{
    if (ui_TetrisScreen)
    {
        lv_obj_del(ui_TetrisScreen);
    }
    ui_TetrisScreen = NULL;
    s_board = NULL;
    s_score_label = s_level_label = s_lines_label = s_combo_label = NULL;
    s_hold_label = s_status = s_hint = s_knob_chip = NULL;
    for (uint8_t i = 0; i < 3; ++i)
    {
        s_next_label[i] = NULL;
    }
    for (uint8_t i = 0; i < ACTION_COUNT; ++i)
    {
        s_action_chip[i] = NULL;
    }
    if (s_board_buf)
    {
        heap_caps_free(s_board_buf);
        s_board_buf = NULL;
    }
}
