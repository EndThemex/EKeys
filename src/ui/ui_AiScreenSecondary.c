/*
 * ui_AiScreenSecondary.c
 *
 * AI Mode 二级功能页：428×142 全屏 lv_img + 状态占位 + 右上角 HUD。
 * 进入本页后 DisplayTask 才激活 AiMode（串口 / WiFi 收发与 hello 握手）。
 *
 * 布局（docs/13 §8.1）：
 *   - lv_img 0,0 428×142，尺寸与帧源完全一致（避免 LVGL 平铺重复）；
 *   - 无流时居中显示 "AI" + 状态副行；有流时隐藏占位，HUD 叠在右上角；
 *   - HUD 半透明黑底，BebasNeueFont14。
 *
 * 键值约定：ENTER 切换 HUD，ESC 回 AI 一级入口页；矩阵键 101~111 忽略。
 */

#include "ui_AiScreenSecondary.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ui_AiScreen.h"
#include "ui_helpers.h"

lv_obj_t *ui_AiScreenSecondary = NULL;
static lv_obj_t *s_ai_img = NULL;
static lv_obj_t *s_ai_state_big = NULL;
static lv_obj_t *s_ai_state_sub = NULL;
static lv_obj_t *s_ai_state_info = NULL;
static lv_obj_t *s_ai_hud = NULL;
static lv_style_t s_ai_hud_style;
static bool s_ai_hud_style_ready = false;

static uint8_t s_ai_lang = 0;
static uint8_t s_ai_state = UI_AI_STATE_WAITING;
static char s_ai_info[40] = {0};
static bool s_ai_hud_user_hidden = false;

static const char *ai_default_info(void)
{
    return "USB CDC / TCP 30001";
}

/* 根据状态刷新占位文案与 HUD 可见性 */
static void ai_apply_state_text(void)
{
    if (!s_ai_state_big || !s_ai_state_sub || !s_ai_state_info || !s_ai_hud)
    {
        return;
    }

    const bool zh = (s_ai_lang == 0);
    /* 中文副行/信息行需换成 CJK 字体（BebasNeue 仅 ASCII） */
    lv_obj_set_style_text_font(s_ai_state_sub,
                               zh ? &ui_font_FontCKJGT16 : &ui_font_BebasNeueFont14, 0);
    lv_obj_set_style_text_font(s_ai_state_info,
                               zh ? &ui_font_FontCKJGT16 : &ui_font_BebasNeueFont14, 0);

    if (s_ai_state == UI_AI_STATE_STREAMING)
    {
        lv_obj_add_flag(s_ai_state_big, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ai_state_sub, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ai_state_info, LV_OBJ_FLAG_HIDDEN);
        if (s_ai_hud_user_hidden)
        {
            lv_obj_add_flag(s_ai_hud, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(s_ai_hud, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    lv_obj_clear_flag(s_ai_state_big, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ai_state_sub, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ai_state_info, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ai_hud, LV_OBJ_FLAG_HIDDEN);

    if (s_ai_state == UI_AI_STATE_SERIAL)
    {
        /* 无 WiFi：自动落到 USB 串口通道，不再显示 WIFI OFF 死路 */
        lv_label_set_text(s_ai_state_big, "USB");
        lv_label_set_text(s_ai_state_sub, zh ? "串口模式 · 等待图像" : "SERIAL MODE - WAITING");
        lv_label_set_text(s_ai_state_info,
                          s_ai_info[0] ? s_ai_info : "USB CDC");
    }
    else if (s_ai_state == UI_AI_STATE_ERROR)
    {
        lv_label_set_text(s_ai_state_big, "AI");
        lv_label_set_text(s_ai_state_sub, s_ai_info[0] ? s_ai_info : "FRAME ERROR");
        lv_label_set_text(s_ai_state_info, "");
    }
    else /* UI_AI_STATE_WAITING */
    {
        lv_label_set_text(s_ai_state_big, "AI");
        lv_label_set_text(s_ai_state_sub, zh ? "等待图像输入" : "WAITING FOR FRAME");
        lv_label_set_text(s_ai_state_info,
                          s_ai_info[0] ? s_ai_info : ai_default_info());
    }
}

void ui_AiScreenSecondary_show_frame(const lv_img_dsc_t *dsc)
{
    if (!s_ai_img || !dsc)
    {
        return;
    }
    lv_img_set_src(s_ai_img, dsc);
    lv_obj_invalidate(s_ai_img);
    /* 首帧到达立即让出占位文案；HUD 由 AiStatus(STREAMING) 打开 */
    if (s_ai_state_big)
    {
        lv_obj_add_flag(s_ai_state_big, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ai_state_sub, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ai_state_info, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_AiScreenSecondary_set_state(uint8_t state, const char *info)
{
    s_ai_state = state;
    if (info != NULL)
    {
        snprintf(s_ai_info, sizeof(s_ai_info), "%s", info);
    }
    else
    {
        /* 清除上一条错误文案，避免状态切换后残留 */
        s_ai_info[0] = '\0';
    }
    ai_apply_state_text();
}

void ui_AiScreenSecondary_set_stats(uint16_t fps_x10, uint32_t bytes_per_s,
                                    uint8_t format, uint16_t dropped,
                                    uint16_t width, uint16_t height)
{
    (void)width;
    (void)height;
    if (!s_ai_hud)
    {
        return;
    }

    const char *fmt_name = "RAW";
    if (format == 1)
    {
        fmt_name = "JPEG";
    }
    else if (format == 2)
    {
        fmt_name = "RGB565";
    }
    else if (format == 3)
    {
        fmt_name = "RLE565";
    }

    char buf[48];
    if (dropped > 0)
    {
        snprintf(buf, sizeof(buf), "%s %u.%ufps %luKB/s D%u",
                 fmt_name, (unsigned)(fps_x10 / 10), (unsigned)(fps_x10 % 10),
                 (unsigned long)(bytes_per_s / 1024u), (unsigned)dropped);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%s %u.%ufps %luKB/s",
                 fmt_name, (unsigned)(fps_x10 / 10), (unsigned)(fps_x10 % 10),
                 (unsigned long)(bytes_per_s / 1024u));
    }
    lv_label_set_text(s_ai_hud, buf);
}

void ui_AiScreenSecondary_toggle_hud(void)
{
    s_ai_hud_user_hidden = !s_ai_hud_user_hidden;
    ai_apply_state_text();
}

void ui_AiScreenSecondary_apply_language(uint8_t lang)
{
    s_ai_lang = lang;
    ai_apply_state_text();
}

void ui_event_AiScreenSecondary(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code != LV_EVENT_KEY)
    {
        return;
    }

    uintptr_t key = (uintptr_t)lv_event_get_param(e);
    if (key == (uintptr_t)LV_KEY_ENTER)
    {
        ui_AiScreenSecondary_toggle_hud();
    }
    else if (key == (uintptr_t)LV_KEY_ESC)
    {
        ui_set_active_screen_tag(UI_SCREEN_AI);
        _ui_screen_change(&ui_AiScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_AiScreen_screen_init);
        lv_refr_now(NULL);
    }
    /* 旋钮 LEFT/RIGHT 与矩阵键 101~111 不改变 AI 功能页状态 */
}

void ui_AiScreenSecondary_screen_init(void)
{
    ui_AiScreenSecondary = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_AiScreenSecondary, LV_OBJ_FLAG_SCROLLABLE);
    ui_set_active_screen_tag(UI_SCREEN_AI_SECONDARY);

    if (!s_ai_hud_style_ready)
    {
        lv_style_init(&s_ai_hud_style);
        lv_style_set_bg_color(&s_ai_hud_style, lv_color_hex(0x000000));
        lv_style_set_bg_opa(&s_ai_hud_style, 140);
        lv_style_set_radius(&s_ai_hud_style, 4);
        lv_style_set_pad_all(&s_ai_hud_style, 4);
        lv_style_set_text_font(&s_ai_hud_style, &ui_font_BebasNeueFont14);
        lv_style_set_text_color(&s_ai_hud_style, lv_color_hex(0xFFFFFF));
        s_ai_hud_style_ready = true;
    }

    /* 全屏图像：widget 尺寸与帧源完全相同（428×142），避免 LVGL 平铺重复 */
    s_ai_img = lv_img_create(ui_AiScreenSecondary);
    lv_obj_set_pos(s_ai_img, 0, 0);
    lv_obj_set_size(s_ai_img, 428, 142);

    s_ai_state_big = lv_label_create(ui_AiScreenSecondary);
    lv_label_set_text(s_ai_state_big, "AI");
    lv_obj_set_style_text_font(s_ai_state_big, &ui_font_BebasNeueFont48, 0);
    lv_obj_set_style_text_color(s_ai_state_big, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_align(s_ai_state_big, LV_ALIGN_CENTER);
    lv_obj_set_y(s_ai_state_big, -20);

    s_ai_state_sub = lv_label_create(ui_AiScreenSecondary);
    lv_label_set_text(s_ai_state_sub, "WAITING FOR FRAME");
    lv_obj_set_style_text_font(s_ai_state_sub, &ui_font_BebasNeueFont14, 0);
    lv_obj_set_style_text_color(s_ai_state_sub, lv_color_hex(0x9AA4B2), 0);
    lv_obj_set_align(s_ai_state_sub, LV_ALIGN_CENTER);
    lv_obj_set_y(s_ai_state_sub, 18);

    s_ai_state_info = lv_label_create(ui_AiScreenSecondary);
    lv_label_set_text(s_ai_state_info, ai_default_info());
    lv_obj_set_style_text_font(s_ai_state_info, &ui_font_BebasNeueFont14, 0);
    lv_obj_set_style_text_color(s_ai_state_info, lv_color_hex(0x6B7480), 0);
    lv_obj_set_align(s_ai_state_info, LV_ALIGN_CENTER);
    lv_obj_set_y(s_ai_state_info, 36);

    s_ai_hud = lv_label_create(ui_AiScreenSecondary);
    lv_label_set_text(s_ai_hud, "WAITING");
    lv_obj_add_style(s_ai_hud, &s_ai_hud_style, 0);
    lv_obj_align(s_ai_hud, LV_ALIGN_TOP_RIGHT, -6, 4);
    lv_obj_add_flag(s_ai_hud, LV_OBJ_FLAG_HIDDEN);

    /* 重新初始化时复位页面状态，避免沿用上一次离开 AI 页时的 HUD 偏移 */
    s_ai_state = UI_AI_STATE_WAITING;
    s_ai_info[0] = '\0';
    s_ai_hud_user_hidden = false;
    ai_apply_state_text();

    lv_obj_add_event_cb(ui_AiScreenSecondary, ui_event_AiScreenSecondary, LV_EVENT_ALL, NULL);
}

void ui_AiScreenSecondary_screen_destroy(void)
{
    if (ui_AiScreenSecondary)
    {
        lv_obj_del(ui_AiScreenSecondary);
    }
    ui_AiScreenSecondary = NULL;
    s_ai_img = NULL;
    s_ai_state_big = NULL;
    s_ai_state_sub = NULL;
    s_ai_state_info = NULL;
    s_ai_hud = NULL;
}
