#include "ui/BacklightPage.h"
#include "ui/Pages.h"
#include "ui/Backlight.h"
#include <Arduino.h>

namespace ekeys
{

    /* 配色（与项目深色背景协调）：
     *   文字：F7F5F9（亮白）
     *   强调：9468F1（主紫，与 MenuPage 卡片描边色一致）
     *   次级文字：A69FAF
     *   警告：FF66CC（粉红） — 用于 "DIM" 提示
     */
    static constexpr uint32_t COLOR_TEXT = 0xF7F5F9;
    static constexpr uint32_t COLOR_DIM_TEXT = 0xA69FAF;
    static constexpr uint32_t COLOR_ACCENT = 0x9468F1;
    static constexpr uint32_t COLOR_WARN = 0xFF66CC;

    BacklightPage::BacklightPage()
        : Page(/*id=*/PAGE_BACKLIGHT, "Backlight", lv_color_hex(COLOR_ACCENT)) {}

    void BacklightPage::buildUi()
    {
        lv_obj_t *r = root();
        /* root 透明，透出屏幕级 PNG 背景（与 MenuPage 一致） */
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, LV_PART_MAIN);

        /* ---- 标题（左上） ---- */
        lv_obj_t *title = lv_label_create(r);
        lv_label_set_text(title, "Backlight");
        lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

        /* ---- 进度条背景框（不可见边框 + 浅底） ----
         * 屏幕宽 428，y=44..58（高 14），左右各留 8px padding；
         * bar 总宽 = 428 - 16 = 412。 */
        lv_obj_t *bar_bg = lv_obj_create(r);
        lv_obj_remove_style_all(bar_bg);
        lv_obj_set_size(bar_bg, 412, 14);
        lv_obj_set_pos(bar_bg, 8, 44);
        lv_obj_set_style_bg_color(bar_bg, lv_color_hex(0x1E162C), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar_bg, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(bar_bg, 4, LV_PART_MAIN);

        /* ---- 进度条填充（紫色，左对齐，宽 0 起步） ---- */
        bar_fill_ = lv_obj_create(r);
        lv_obj_remove_style_all(bar_fill_);
        lv_obj_set_size(bar_fill_, 0, 14);
        lv_obj_set_pos(bar_fill_, 8, 44);
        lv_obj_set_style_bg_color(bar_fill_, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar_fill_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(bar_fill_, 4, LV_PART_MAIN);

        /* ---- 数值文字 "XX%"（进度条正下方，y=72） ---- */
        value_label_ = lv_label_create(r);
        lv_label_set_text(value_label_, "100%");
        lv_obj_set_style_text_color(value_label_, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(value_label_, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(value_label_, LV_ALIGN_TOP_MID, 0, 72);

        /* ---- DIM 警告（亮度 ≤ MIN_PCT 时显示，y=100，左对齐） ---- */
        dim_warn_ = lv_label_create(r);
        lv_label_set_text(dim_warn_, "DIM (min)");
        lv_obj_set_style_text_color(dim_warn_, lv_color_hex(COLOR_WARN), LV_PART_MAIN);
        lv_obj_set_style_text_font(dim_warn_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(dim_warn_, LV_ALIGN_TOP_LEFT, 8, 104);
        lv_obj_set_style_bg_opa(dim_warn_, LV_OPA_TRANSP, LV_PART_MAIN);
        /* 默认隐藏；refresh() 中按需显隐 */
        lv_obj_add_flag(dim_warn_, LV_OBJ_FLAG_HIDDEN);

        /* ---- 底部 hint（按 State 类型模板：KEY3..KEY9 直选档位） ---- */
        lv_obj_t *hint = lv_label_create(r);
        char hint_buf[80];
        buildHintLabel(kind(), hint_buf, sizeof(hint_buf));
        lv_label_set_text(hint, hint_buf);
        lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_DIM_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    }

    void BacklightPage::refresh()
    {
        const uint8_t pct = idxToPct(level_);
        const bool atMin = (pct <= MIN_PCT);

        /* 数值 label */
        if (value_label_ != nullptr)
        {
            lv_label_set_text_fmt(value_label_, "%u%%", pct);
        }

        /* 进度条填充宽度：
         *   full width 412 px 对应 MAX_PCT（=100%）；
         *   当前 fill = 412 * pct / 100%（整数） */
        if (bar_fill_ != nullptr)
        {
            const lv_coord_t w = (lv_coord_t)((uint16_t)412 * pct / 100U);
            lv_obj_set_size(bar_fill_, w, 14);
        }

        /* DIM 警告：≤ MIN_PCT 时显示 */
        if (dim_warn_ != nullptr)
        {
            if (atMin)
                lv_obj_clear_flag(dim_warn_, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(dim_warn_, LV_OBJ_FLAG_HIDDEN);
        }

        /* 实际写到 LEDC（防过度调暗由 Backlight::setBrightnessPct 内部 clamp） */
        Backlight::instance().setBrightnessPct(pct);
    }

    void BacklightPage::onEnter()
    {
        /* 同步全局当前亮度到 level_，避免外部改了亮度后回本页显示不一致 */
        level_ = pctToLevel(Backlight::instance().brightnessPct());
        refresh();
    }

    void BacklightPage::onEncoder(int8_t delta)
    {
        int16_t next = (int16_t)level_ + delta;
        /* 旋钮循环：顶部 +1 → 回到 0；底部 -1 → 跳到 LEVEL_COUNT-1。
         * 这样用户可以快速从最亮到最暗而不必"反向转多圈"。 */
        if (next < 0)
            next = LEVEL_COUNT - 1;
        else if (next >= LEVEL_COUNT)
            next = 0;
        level_ = (uint8_t)next;
        refresh();
    }

    /* S 类型 selectState：idx ∈ [0, LEVEL_COUNT-1] 直接切档；越界无操作。 */
    bool BacklightPage::selectState(uint8_t idx)
    {
        if (idx >= LEVEL_COUNT)
            return false;
        level_ = idx;
        refresh();
        return true;
    }

} // namespace ekeys