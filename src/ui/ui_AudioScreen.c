/*
 * ui_AudioScreen.c
 *
 * 音效页（Sound Pad）。手写 SquareLine 风格（参照 ui_MusicScreen.c）：
 * 4 列 × 3 行共 12 格 = 11 个音效格 + 1 状态格。屏 428×142。
 *
 * 仅 label/边框样式更新，无大缓冲（LVGL 内存约束，见 sound-pad 计划）。
 */

#include "ui_AudioScreen.h"

#include <stdbool.h>

#include "ui.h"

lv_obj_t *ui_AudioScreen = NULL;

/* 音效格对象与文本标签（格 12 = 状态格） */
static lv_obj_t *s_pad_cells[11] = {NULL};
static lv_obj_t *s_pad_key_labels[11] = {NULL};
static lv_obj_t *s_pad_file_labels[11] = {NULL};
static lv_obj_t *s_status_cell = NULL;

/* 布局：左右边距 8、上边距 6，格 100×41、间距 4/3 */
#define PAD_COLS 4
#define PAD_ROWS 3
#define PAD_CELL_W 100
#define PAD_CELL_H 41
#define PAD_GAP_X 4
#define PAD_GAP_Y 3
#define PAD_LEFT 8
#define PAD_TOP 6

/* 播放高亮色（与 App 品牌蓝一致的观感） */
#define PAD_HL_COLOR 0x4F8CFF
#define PAD_BORDER_COLOR 0x2A3542
#define PAD_BG_COLOR 0x141A22

static void pad_cell_layout(uint8_t idx, lv_obj_t *cell, bool is_status)
{
    const uint8_t row = idx / PAD_COLS;
    const uint8_t col = idx % PAD_COLS;
    lv_obj_set_size(cell, PAD_CELL_W, PAD_CELL_H);
    lv_obj_set_x(cell, PAD_LEFT + col * (PAD_CELL_W + PAD_GAP_X));
    lv_obj_set_y(cell, PAD_TOP + row * (PAD_CELL_H + PAD_GAP_Y));

    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(cell, 6, 0);
    lv_obj_set_style_bg_color(cell, lv_color_hex(PAD_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(cell, 200, 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, lv_color_hex(PAD_BORDER_COLOR), 0);
    lv_obj_set_style_pad_all(cell, 3, 0);
    (void)is_status;
}

/* 事件函数 */
void ui_event_AudioScreen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    uintptr_t key = (uintptr_t)lv_event_get_param(e);

    if (event_code == LV_EVENT_KEY && key == (uintptr_t)LV_KEY_RIGHT) {
        ui_set_active_screen_tag(UI_SCREEN_PC_STATUS);
        _ui_screen_change(&ui_PcStatusScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_PcStatusScreen_screen_init);
        lv_refr_now(NULL);
    }
    else if (event_code == LV_EVENT_KEY && key == (uintptr_t)LV_KEY_LEFT) {
        ui_set_active_screen_tag(UI_SCREEN_MUSIC);
        _ui_screen_change(&ui_MusicScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_MusicScreen_screen_init);
        lv_refr_now(NULL);
    }
    else if (event_code == LV_EVENT_KEY && key == (uintptr_t)LV_KEY_ENTER) {
        /* 旋钮单击：停止当前播放 */
        ui_audio_pad_stop();
    }
    else if (event_code == LV_EVENT_KEY && key == (uintptr_t)LV_KEY_ESC) {
        /* 旋钮双击：回主屏 */
        ui_set_active_screen_tag(UI_SCREEN_MAIN);
        _ui_screen_change(&ui_MainScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_MainScreen_screen_init);
        lv_refr_now(NULL);
    }
}

/* build functions */
void ui_AudioScreen_screen_init(void)
{
    ui_AudioScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_AudioScreen, LV_OBJ_FLAG_SCROLLABLE);
    ui_set_active_screen_tag(UI_SCREEN_AUDIO);

    static lv_style_t key_style;
    static bool key_style_ready = false;
    if (!key_style_ready) {
        lv_style_init(&key_style);
        lv_style_set_text_font(&key_style, &ui_font_BebasNeueFont24);
        lv_style_set_text_color(&key_style, lv_color_hex(0xF5F8FF));
        key_style_ready = true;
    }
    static lv_style_t file_style;
    static bool file_style_ready = false;
    if (!file_style_ready) {
        lv_style_init(&file_style);
        lv_style_set_text_font(&file_style, &ui_font_FontCKJGT16);
        lv_style_set_text_color(&file_style, lv_color_hex(0x9AA7B6));
        file_style_ready = true;
    }
    static lv_style_t status_style;
    static bool status_style_ready = false;
    if (!status_style_ready) {
        lv_style_init(&status_style);
        lv_style_set_text_font(&status_style, &ui_font_FontCKJGT16);
        lv_style_set_text_color(&status_style, lv_color_hex(0xF5F8FF));
        status_style_ready = true;
    }

    char buf[16];
    for (uint8_t i = 0; i < 11; ++i) {
        s_pad_cells[i] = lv_obj_create(ui_AudioScreen);
        pad_cell_layout(i, s_pad_cells[i], false);

        s_pad_key_labels[i] = lv_label_create(s_pad_cells[i]);
        lv_obj_add_style(s_pad_key_labels[i], &key_style, 0);
        snprintf(buf, sizeof(buf), "%u", (unsigned)(i + 1));
        lv_label_set_text(s_pad_key_labels[i], buf);
        lv_obj_align(s_pad_key_labels[i], LV_ALIGN_TOP_LEFT, 2, 0);

        s_pad_file_labels[i] = lv_label_create(s_pad_cells[i]);
        lv_obj_add_style(s_pad_file_labels[i], &file_style, 0);
        lv_label_set_text(s_pad_file_labels[i], "--");
        lv_label_set_long_mode(s_pad_file_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_pad_file_labels[i], PAD_CELL_W - 10);
        lv_obj_align(s_pad_file_labels[i], LV_ALIGN_BOTTOM_LEFT, 2, 0);
    }

    /* 格 12（第 4 行第 3 列）：状态格 */
    s_status_cell = lv_obj_create(ui_AudioScreen);
    pad_cell_layout(11, s_status_cell, true);
    lv_obj_t *title = lv_label_create(s_status_cell);
    lv_obj_add_style(title, &status_style, 0);
    lv_label_set_text(title, "SOUND PAD");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 0);
    lv_obj_t *hint = lv_label_create(s_status_cell);
    lv_obj_add_style(hint, &file_style, 0);
    lv_label_set_text(hint, "ENTER:STOP");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 2, 0);

    lv_obj_add_event_cb(ui_AudioScreen, ui_event_AudioScreen, LV_EVENT_ALL, NULL);
}

void ui_AudioScreen_screen_destroy(void)
{
    if (ui_AudioScreen) lv_obj_del(ui_AudioScreen);

    ui_AudioScreen = NULL;
    for (uint8_t i = 0; i < 11; ++i) {
        s_pad_cells[i] = NULL;
        s_pad_key_labels[i] = NULL;
        s_pad_file_labels[i] = NULL;
    }
    s_status_cell = NULL;
}

void ui_AudioScreen_set_pads(const char files[11][25])
{
    if (ui_AudioScreen == NULL) {
        return;
    }
    for (uint8_t i = 0; i < 11; ++i) {
        if (s_pad_file_labels[i] == NULL) {
            continue;
        }
        /* 未绑定显示 "--"，已绑定显示文件名（超长省略号截断） */
        lv_label_set_text(s_pad_file_labels[i],
                          files[i][0] == '\0' ? "--" : files[i]);
    }
}

void ui_AudioScreen_set_playing(uint8_t key)
{
    if (ui_AudioScreen == NULL) {
        return;
    }
    for (uint8_t i = 0; i < 11; ++i) {
        if (s_pad_cells[i] == NULL) {
            continue;
        }
        if (key == i + 1) {
            lv_obj_set_style_border_width(s_pad_cells[i], 2, 0);
            lv_obj_set_style_border_color(s_pad_cells[i],
                                          lv_color_hex(PAD_HL_COLOR), 0);
        } else {
            lv_obj_set_style_border_width(s_pad_cells[i], 1, 0);
            lv_obj_set_style_border_color(s_pad_cells[i],
                                          lv_color_hex(PAD_BORDER_COLOR), 0);
        }
    }
}
