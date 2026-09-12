/*
 * ui_AudioScreenSecondary.c
 *
 * 音效页二级屏（Sound Pad board）。原 ui_AudioScreen 的 11 音效格迁移至此。
 * 布局与 ui_KeyMappedSecondary 一致：左侧信息卡 60×134 + 右侧键位卡 358×134，
 * 11 个音效格 82×36 按 3/4/4 三行排布（与物理矩阵键位、键位映射页一致），
 * 第 0 行第 4 列空槽为状态格。屏 428×142。
 *
 * 触发链：矩阵键 1~11 在 DisplayTask 处截胡 → AudioPad::trigger → 高亮本屏；
 * 旋钮 ENTER 调 ui_audio_pad_stop → AudioPad::stop；ESC 回 AudioScreen 一级屏。
 */

#include "ui_AudioScreenSecondary.h"

#include <stdbool.h>
#include <stdio.h>

#include "ui.h"

lv_obj_t *ui_AudioScreenSecondary = NULL;

/* 音效格对象与文本标签（第 12 格 = 状态格） */
static lv_obj_t *s_pad_cells[11] = {NULL};
static lv_obj_t *s_pad_key_labels[11] = {NULL};
static lv_obj_t *s_pad_file_labels[11] = {NULL};
static lv_obj_t *s_status_cell = NULL;

/* 双卡结构（与 ui_KeyMappedSecondary 同构） */
static lv_obj_t *s_shell = NULL;
static lv_obj_t *s_app_card = NULL;
static lv_obj_t *s_keys_card = NULL;
static lv_obj_t *s_playing_label = NULL; /* 信息卡底部：正在播放的键位 */

/* 隐形转发按钮（触屏兜底，与其他二级页同款布局） */
static lv_obj_t *s_button_left = NULL;
static lv_obj_t *s_button_right = NULL;
static lv_obj_t *s_button_enter = NULL;
static lv_obj_t *s_button_exit = NULL;

/* 键位格尺寸与坐标（键位卡内，与 ui_KeyMappedSecondary 的 kKeyRects 一致） */
#define PAD_CELL_W 82
#define PAD_CELL_H 36
#define PAD_STATUS_X 268 /* 第 0 行第 4 列空槽：状态格 */
#define PAD_STATUS_Y 4

static const lv_coord_t kPadRects[11][2] = {
    // 第 0 行：键 1/2/3（左对齐）
    {4, 4}, {92, 4}, {180, 4},
    // 第 1 行：键 4/5/6/7（满 4 列）
    {4, 46}, {92, 46}, {180, 46}, {268, 46},
    // 第 2 行：键 8/9/10/11（满 4 列）
    {4, 88}, {92, 88}, {180, 88}, {268, 88},
};

/* 播放高亮色（与 App 品牌蓝一致的观感） */
#define PAD_HL_COLOR 0x4F8CFF
#define PAD_BORDER_COLOR 0x2B3442
#define PAD_BG_COLOR 0x0F141B

static void pad_cell_style(lv_obj_t *cell)
{
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(cell, 8, 0);
    lv_obj_set_style_bg_color(cell, lv_color_hex(PAD_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, lv_color_hex(PAD_BORDER_COLOR), 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    lv_obj_set_style_shadow_width(cell, 0, 0);
}

static void pad_cell_layout(uint8_t idx, lv_obj_t *cell)
{
    lv_obj_set_size(cell, PAD_CELL_W, PAD_CELL_H);
    lv_obj_set_pos(cell, kPadRects[idx][0], kPadRects[idx][1]);
    pad_cell_style(cell);
}

/* 卡片样式（与 ui_KeyMappedSecondary 的 style_card / style_key_cell 一致） */
static void pad_style_card(lv_obj_t *obj, lv_color_t bg_color, lv_color_t border_color)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, border_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void pad_forward_key(uint32_t key)
{
    lv_obj_t *active_screen = lv_scr_act();
    lv_group_t *group = lv_group_get_default();
    lv_obj_t *target = group ? lv_group_get_focused(group) : active_screen;
    if (target)
    {
        lv_event_send(target, LV_EVENT_KEY, (void *)key);
    }
}

/* 事件函数 */
void ui_event_AudioScreenSecondary(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    uintptr_t key = (uintptr_t)lv_event_get_param(e);

    if (event_code == LV_EVENT_KEY && key == (uintptr_t)LV_KEY_ENTER) {
        /* 旋钮单击：停止当前播放 */
        ui_audio_pad_stop();
    }
    else if (event_code == LV_EVENT_KEY && key == (uintptr_t)LV_KEY_ESC) {
        /* 旋钮双击：回一级页 AudioScreen */
        ui_set_active_screen_tag(UI_SCREEN_AUDIO);
        _ui_screen_change(&ui_AudioScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_AudioScreen_screen_init);
        lv_refr_now(NULL);
    }
}

/* 隐形按钮转发（与其他二级页一致） */
static void ui_event_ButtonLeftAudioSecondary(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        pad_forward_key(LV_KEY_LEFT);
    }
}

static void ui_event_ButtonRightAudioSecondary(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        pad_forward_key(LV_KEY_RIGHT);
    }
}

static void ui_event_ButtonEnterAudioSecondary(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        pad_forward_key(LV_KEY_ENTER);
    }
}

static void ui_event_ButtonExitAudioSecondary(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        pad_forward_key(LV_KEY_ESC);
    }
}

/* build functions */
void ui_AudioScreenSecondary_screen_init(void)
{
    ui_AudioScreenSecondary = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_AudioScreenSecondary, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_AudioScreenSecondary, lv_color_hex(0x070A0F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AudioScreenSecondary, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_AudioScreenSecondary, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_AudioScreenSecondary, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_set_active_screen_tag(UI_SCREEN_AUDIO_SECONDARY);

    /* 背景壳（与 ui_KeyMappedSecondary 的 Shell 一致） */
    s_shell = lv_obj_create(ui_AudioScreenSecondary);
    lv_obj_set_size(s_shell, 428, 142);
    lv_obj_align(s_shell, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_shell, LV_OBJ_FLAG_SCROLLABLE);
    pad_style_card(s_shell, lv_color_hex(0x0C0F14), lv_color_hex(0x0C0F14));
    lv_obj_set_style_border_width(s_shell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 左侧信息卡（60×134）：标题 + 图标徽章 + 播放键位指示 */
    s_app_card = lv_obj_create(ui_AudioScreenSecondary);
    lv_obj_set_size(s_app_card, 60, 134);
    lv_obj_align(s_app_card, LV_ALIGN_TOP_LEFT, 4, 4);
    pad_style_card(s_app_card, lv_color_hex(0x161C25), lv_color_hex(0x2E3947));

    lv_obj_t *title = lv_label_create(s_app_card);
    lv_label_set_text(title, "SOUND");
    lv_obj_set_width(title, 60 - 12);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0x8FA0B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &ui_font_BebasNeueFont14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *icon_badge = lv_obj_create(s_app_card);
    lv_obj_set_size(icon_badge, 50, 50);
    lv_obj_align(icon_badge, LV_ALIGN_CENTER, 0, -4);
    lv_obj_clear_flag(icon_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(icon_badge, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(icon_badge, lv_color_hex(0x0D1015), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(icon_badge, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(icon_badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(icon_badge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *badge_icon = lv_label_create(icon_badge);
    lv_label_set_text(badge_icon, LV_SYMBOL_AUDIO);
    lv_obj_center(badge_icon);
    lv_obj_set_style_text_font(badge_icon, &ui_font_BebasNeueFont36, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(badge_icon, lv_color_hex(0xF5F7FA), LV_PART_MAIN | LV_STATE_DEFAULT);

    s_playing_label = lv_label_create(s_app_card);
    lv_label_set_text(s_playing_label, "--");
    lv_obj_set_width(s_playing_label, 60 - 10);
    lv_label_set_long_mode(s_playing_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_playing_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_playing_label, lv_color_hex(0x8FA0B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_playing_label, &ui_font_BebasNeueFont14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_playing_label, LV_ALIGN_BOTTOM_MID, 0, -12);

    /* 右侧键位卡（358×134）：11 个音效格 + 状态格 */
    s_keys_card = lv_obj_create(ui_AudioScreenSecondary);
    lv_obj_set_size(s_keys_card, 358, 134);
    lv_obj_align(s_keys_card, LV_ALIGN_TOP_RIGHT, -4, 4);
    pad_style_card(s_keys_card, lv_color_hex(0x121820), lv_color_hex(0x273242));

    static lv_style_t key_style;
    static bool key_style_ready = false;
    if (!key_style_ready) {
        lv_style_init(&key_style);
        lv_style_set_text_font(&key_style, &ui_font_BebasNeueFont14);
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

    char buf[16];
    for (uint8_t i = 0; i < 11; ++i) {
        s_pad_cells[i] = lv_obj_create(s_keys_card);
        pad_cell_layout(i, s_pad_cells[i]);

        s_pad_key_labels[i] = lv_label_create(s_pad_cells[i]);
        lv_obj_add_style(s_pad_key_labels[i], &key_style, 0);
        snprintf(buf, sizeof(buf), "%u", (unsigned)(i + 1));
        lv_label_set_text(s_pad_key_labels[i], buf);
        lv_obj_align(s_pad_key_labels[i], LV_ALIGN_TOP_LEFT, 4, 1);

        s_pad_file_labels[i] = lv_label_create(s_pad_cells[i]);
        lv_obj_add_style(s_pad_file_labels[i], &file_style, 0);
        lv_label_set_text(s_pad_file_labels[i], "--");
        lv_label_set_long_mode(s_pad_file_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_pad_file_labels[i], PAD_CELL_W - 8);
        lv_obj_align(s_pad_file_labels[i], LV_ALIGN_BOTTOM_LEFT, 4, -1);
    }

    /* 第 0 行第 4 列空槽：状态格（与键位映射页的 profile 序号格同位） */
    s_status_cell = lv_obj_create(s_keys_card);
    lv_obj_set_size(s_status_cell, PAD_CELL_W, PAD_CELL_H);
    lv_obj_set_pos(s_status_cell, PAD_STATUS_X, PAD_STATUS_Y);
    pad_cell_style(s_status_cell);

    lv_obj_t *hint = lv_label_create(s_status_cell);
    lv_obj_add_style(hint, &key_style, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x8FA0B5), 0);
    lv_label_set_text(hint, "ENTER:STOP");
    lv_obj_center(hint);

    /* 隐形转发按钮（与其他二级页一致的按键布局） */
    s_button_left = lv_btn_create(ui_AudioScreenSecondary);
    lv_obj_set_size(s_button_left, 142, 32);
    lv_obj_align(s_button_left, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(s_button_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_button_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s_button_left, LV_OBJ_FLAG_SCROLLABLE);

    s_button_enter = lv_btn_create(ui_AudioScreenSecondary);
    lv_obj_set_size(s_button_enter, 144, 32);
    lv_obj_align(s_button_enter, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_button_enter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_button_enter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s_button_enter, LV_OBJ_FLAG_SCROLLABLE);

    s_button_right = lv_btn_create(ui_AudioScreenSecondary);
    lv_obj_set_size(s_button_right, 142, 32);
    lv_obj_align(s_button_right, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(s_button_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_button_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s_button_right, LV_OBJ_FLAG_SCROLLABLE);

    s_button_exit = lv_btn_create(ui_AudioScreenSecondary);
    lv_obj_set_size(s_button_exit, 48, 24);
    lv_obj_align(s_button_exit, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(s_button_exit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s_button_exit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(s_button_exit, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(s_button_left, ui_event_ButtonLeftAudioSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(s_button_enter, ui_event_ButtonEnterAudioSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(s_button_right, ui_event_ButtonRightAudioSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(s_button_exit, ui_event_ButtonExitAudioSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_AudioScreenSecondary, ui_event_AudioScreenSecondary, LV_EVENT_ALL, NULL);
}

void ui_AudioScreenSecondary_screen_destroy(void)
{
    if (ui_AudioScreenSecondary) lv_obj_del(ui_AudioScreenSecondary);

    ui_AudioScreenSecondary = NULL;
    for (uint8_t i = 0; i < 11; ++i) {
        s_pad_cells[i] = NULL;
        s_pad_key_labels[i] = NULL;
        s_pad_file_labels[i] = NULL;
    }
    s_status_cell = NULL;
    s_shell = NULL;
    s_app_card = NULL;
    s_keys_card = NULL;
    s_playing_label = NULL;
    s_button_left = NULL;
    s_button_right = NULL;
    s_button_enter = NULL;
    s_button_exit = NULL;
}

void ui_AudioScreenSecondary_set_pads(const char files[11][25])
{
    if (ui_AudioScreenSecondary == NULL) {
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

void ui_AudioScreenSecondary_set_playing(uint8_t key)
{
    if (ui_AudioScreenSecondary == NULL) {
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

    /* 信息卡底部：正在播放的键位（K01~K11），无播放显示 "--" */
    if (s_playing_label != NULL) {
        if (key >= 1 && key <= 11) {
            char buf[8];
            snprintf(buf, sizeof(buf), "K%02u", (unsigned)key);
            lv_label_set_text(s_playing_label, buf);
            lv_obj_set_style_text_color(s_playing_label,
                                        lv_color_hex(PAD_HL_COLOR), 0);
        } else {
            lv_label_set_text(s_playing_label, "--");
            lv_obj_set_style_text_color(s_playing_label,
                                        lv_color_hex(0x8FA0B5), 0);
        }
    }
}
