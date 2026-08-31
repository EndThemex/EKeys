/*
 * ui_minimal.cpp
 *
 * 阶段 02 最小主屏：
 *
 *   [ 标题"EKeys"  ]  [ 时间 HH:MM:SS ]
 *
 * DisplayTask 持有 LABE引用，并通过 setTimeLabel() 更新右侧标签。
 */

#include "ui_minimal.h"

#include <Arduino.h>
#include <lvgl.h>

namespace ekeys {

namespace {

lv_obj_t *g_title_label = nullptr;
lv_obj_t *g_time_label  = nullptr;

}  // namespace

void ui_minimal::create()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), LV_PART_MAIN);

    g_title_label = lv_label_create(screen);
    lv_label_set_text(g_title_label, "EKeys");
    lv_obj_set_style_text_color(g_title_label, lv_color_hex(0xFFFFFF),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(g_title_label, &lv_font_montserrat_20,
                               LV_PART_MAIN);
    lv_obj_align(g_title_label, LV_ALIGN_LEFT_MID, 15, 0);

    g_time_label = lv_label_create(screen);
    lv_label_set_text(g_time_label, "--:--:--");
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(0x00FF88),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_28,
                               LV_PART_MAIN);
    lv_obj_align(g_time_label, LV_ALIGN_RIGHT_MID, -15, 0);
}

void ui_minimal::setTimeLabel(const char *text)
{
    if (g_time_label != nullptr && text != nullptr) {
        lv_label_set_text(g_time_label, text);
    }
}

}  // namespace ekeys
