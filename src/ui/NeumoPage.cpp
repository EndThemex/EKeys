#include "ui/NeumoPage.h"
#include "ui/Pages.h"
#include <Arduino.h>

namespace ekeys
{

    /* ===== 拟态配色（与项目深色背景区分，整体使用浅色基底） =====
     * 拟态样式的核心：元素颜色 == 背景颜色，靠"阴影对"形成凸/凹体感。
     * 这里基底用 #DDE3EA（偏冷灰白），阴影一明一暗，模拟左上方光源。 */
    static constexpr uint32_t NEUMO_BASE = 0xDDE3EA;   // 元素/背景 同色
    static constexpr uint32_t NEUMO_LIGHT = 0xFFFFFF;  // 高光（左上）
    static constexpr uint32_t NEUMO_DARK = 0xA3B1C6;   // 投影（右下）
    static constexpr uint32_t NEUMO_TEXT = 0x4A5568;   // 文字
    static constexpr uint32_t NEUMO_ACCENT = 0x5B8DEF; // 强调（激活色）

    NeumoPage::NeumoPage()
        : Page(/*id=*/PAGE_NEUMO, "Neumo", lv_color_hex(NEUMO_ACCENT)) {}

    /* ---------- 样式辅助 ---------- */

    /* 凸起（raised）—— 外阴影：左上浅光 + 右下深影
     * 用 box-shadow 时，阴影宽度太大会让相邻控件互相污染。
     * 拟态小屏（428x142）通常 4~6px 已经足够"立体"。 */
    void NeumoPage::applyNeumoRaised(lv_obj_t *obj, lv_coord_t radius)
    {
        if (obj == nullptr)
            return;
        lv_obj_set_style_bg_color(obj, lv_color_hex(NEUMO_BASE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
        lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);

        /* 左上浅光（模拟受光面） */
        lv_obj_set_style_shadow_color(obj, lv_color_hex(NEUMO_LIGHT), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(obj, LV_OPA_70, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(obj, 6, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_x(obj, -4, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_y(obj, -4, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(obj, 0, LV_PART_MAIN);

        /* 第二个 shadow：右下深影（投影面）—— LVGL 8.3 单个 obj 只能配一套 shadow，
         * 这里用 ring + 错位实现"双阴影"会过度复杂；退而求其次用 darker border 模拟右下侧深度。 */
        lv_obj_set_style_outline_color(obj, lv_color_hex(NEUMO_DARK), LV_PART_MAIN);
        lv_obj_set_style_outline_opa(obj, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_outline_width(obj, 1, LV_PART_MAIN);
        lv_obj_set_style_outline_pad(obj, 2, LV_PART_MAIN);
    }

    /* 凹陷（sunken / pressed / 输入框未聚焦）—— inset 阴影，颜色相同方向相反
     * 用于输入框、Switch 凹槽、Checkbox 未勾选态。 */
    void NeumoPage::applyNeumoSunken(lv_obj_t *obj, lv_coord_t radius)
    {
        if (obj == nullptr)
            return;
        lv_obj_set_style_bg_color(obj, lv_color_hex(NEUMO_BASE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);
        lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);

        /* inset：左上变深（背光面），右下变亮（受光面对调）。
         * LVGL 8.3 style_shadow 不支持 inset 标记，这里用两个 lv_obj 套娃不可行；
         * 用一个 inset 阴影即可视觉接近"凹"。 */
        lv_obj_set_style_shadow_color(obj, lv_color_hex(NEUMO_DARK), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(obj, LV_OPA_60, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_x(obj, 3, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_y(obj, 3, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(obj, 0, LV_PART_MAIN);
    }

    /* 按钮：默认凸起，按下时切到 sunken（pressed 状态） */
    void NeumoPage::applyNeumoButton(lv_obj_t *btn)
    {
        if (btn == nullptr)
            return;
        applyNeumoRaised(btn, 8);

        /* 按下状态：凹陷感 */
        lv_obj_set_style_bg_color(btn, lv_color_hex(NEUMO_BASE), LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(btn, 0, LV_STATE_PRESSED);
        lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_STATE_PRESSED);

        /* 文字 */
        lv_obj_set_style_text_color(btn, lv_color_hex(NEUMO_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(btn, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    /* Switch：背景凹（sunken），指示圆点凸（raised） */
    void NeumoPage::applyNeumoSwitch(lv_obj_t *sw)
    {
        if (sw == nullptr)
            return;

        /* 整个 switch：凹陷底（关态）/ 强调底（开态） */
        lv_obj_set_style_bg_color(sw, lv_color_hex(NEUMO_BASE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);

        /* 凹陷阴影（左侧稍深） */
        lv_obj_set_style_shadow_color(sw, lv_color_hex(NEUMO_DARK), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(sw, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(sw, 3, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_x(sw, 2, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_y(sw, 2, LV_PART_MAIN);

        /* 指示器（knob）：凸起小圆球 */
        lv_obj_set_style_bg_color(sw, lv_color_hex(NEUMO_BASE), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);

        lv_obj_set_style_shadow_color(sw, lv_color_hex(NEUMO_DARK), LV_PART_KNOB);
        lv_obj_set_style_shadow_opa(sw, LV_OPA_50, LV_PART_KNOB);
        lv_obj_set_style_shadow_width(sw, 4, LV_PART_KNOB);
        lv_obj_set_style_shadow_ofs_x(sw, 2, LV_PART_KNOB);
        lv_obj_set_style_shadow_ofs_y(sw, 2, LV_PART_KNOB);

        /* 开态：底色换成强调色，让指示器对比更强 */
        lv_obj_set_style_bg_color(sw, lv_color_hex(NEUMO_ACCENT), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_CHECKED);
    }

    /* Checkbox：
     *   - 默认（未勾）：凹陷方框（INDICATOR）
     *   - 勾选：凹陷方框变成 accent 凸起（INDICATOR CHECKED）
     *
     * 注意：LVGL 8.x 的勾选方框画在 LV_PART_INDICATOR，不是 LV_PART_MAIN。
     * 之前把勾选背景设在 MAIN 上会让整个 checkbox 容器（连同 label 文字背景）
     * 一起变色，视觉错位。下面把样式全部下放到 INDICATOR。 */
    void NeumoPage::applyNeumoCheckbox(lv_obj_t *cb, lv_coord_t boxSize)
    {
        if (cb == nullptr)
            return;
        (void)boxSize; // box 大小由 lv_checkbox_set_size 控制

        /* MAIN：仅设文本相关样式 + 透明背景，避免文字区域出现方框色块 */
        lv_obj_set_style_bg_opa(cb, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(cb, 0, LV_PART_MAIN);
        lv_obj_set_style_text_color(cb, lv_color_hex(NEUMO_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_14, LV_PART_MAIN);

        /* INDICATOR：默认（未勾）—— 凹陷方框 */
        lv_obj_set_style_bg_color(cb, lv_color_hex(NEUMO_BASE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(cb, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(cb, 4, LV_PART_INDICATOR);
        lv_obj_set_style_border_width(cb, 0, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_color(cb, lv_color_hex(NEUMO_DARK), LV_PART_INDICATOR);
        lv_obj_set_style_shadow_opa(cb, LV_OPA_50, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_width(cb, 3, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_ofs_x(cb, 2, LV_PART_INDICATOR);
        lv_obj_set_style_shadow_ofs_y(cb, 2, LV_PART_INDICATOR);
        /* 默认 symbol（"✓"）颜色淡化，让未勾时只剩"凹陷方框" */
        lv_obj_set_style_text_color(cb, lv_color_hex(NEUMO_BASE), LV_PART_INDICATOR);

        /* INDICATOR CHECKED：变 accent 凸起（投影变浅 = 抬升感） */
        lv_obj_set_style_bg_color(cb, lv_color_hex(NEUMO_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_shadow_opa(cb, LV_OPA_30, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_shadow_ofs_y(cb, 1, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(cb, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_CHECKED);
    }

    /* ---------- 事件回调 ---------- */

    void NeumoPage::onBtnClick(lv_event_t *e)
    {
        NeumoPage *self = (NeumoPage *)lv_event_get_user_data(e);
        if (self == nullptr)
            return;
        self->btn_count_++;
        if (self->status_label_ != nullptr)
        {
            lv_label_set_text_fmt(self->status_label_, "btn:%u sw:%s cb:%s",
                                  (unsigned)self->btn_count_,
                                  self->switch_on_ ? "on" : "off",
                                  self->checkbox_on_ ? "on" : "off");
        }
    }

    void NeumoPage::onSwitchChange(lv_event_t *e)
    {
        NeumoPage *self = (NeumoPage *)lv_event_get_user_data(e);
        if (self == nullptr)
            return;
        lv_obj_t *sw = lv_event_get_target(e);
        self->switch_on_ = lv_obj_has_state(sw, LV_STATE_CHECKED);
        if (self->status_label_ != nullptr)
        {
            lv_label_set_text_fmt(self->status_label_, "btn:%u sw:%s cb:%s",
                                  (unsigned)self->btn_count_,
                                  self->switch_on_ ? "on" : "off",
                                  self->checkbox_on_ ? "on" : "off");
        }
    }

    void NeumoPage::onCheckboxChange(lv_event_t *e)
    {
        NeumoPage *self = (NeumoPage *)lv_event_get_user_data(e);
        if (self == nullptr)
            return;
        lv_obj_t *cb = lv_event_get_target(e);
        self->checkbox_on_ = lv_obj_has_state(cb, LV_STATE_CHECKED);
        if (self->status_label_ != nullptr)
        {
            lv_label_set_text_fmt(self->status_label_, "btn:%u sw:%s cb:%s",
                                  (unsigned)self->btn_count_,
                                  self->switch_on_ ? "on" : "off",
                                  self->checkbox_on_ ? "on" : "off");
        }
    }

    /* 输入框聚焦：视觉从 sunken（默认）切到 sunken + accent outline */
    void NeumoPage::onTextareaFocused(lv_event_t *e)
    {
        lv_obj_t *ta = lv_event_get_target(e);
        if (ta == nullptr)
            return;
        lv_obj_set_style_outline_color(ta, lv_color_hex(NEUMO_ACCENT), LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_outline_opa(ta, LV_OPA_60, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_outline_width(ta, 1, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_outline_pad(ta, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
    }

    void NeumoPage::onTextareaDefocused(lv_event_t *e)
    {
        lv_obj_t *ta = lv_event_get_target(e);
        if (ta == nullptr)
            return;
        lv_obj_set_style_outline_opa(ta, LV_OPA_TRANSP, LV_PART_MAIN);
    }

    /* ---------- UI 构建 ---------- */

    void NeumoPage::buildUi()
    {
        lv_obj_t *r = root();

        /* 整页基底：浅色拟态色，覆盖下层 PNG 背景 */
        lv_obj_set_style_bg_color(r, lv_color_hex(NEUMO_BASE), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(r, 6, LV_PART_MAIN);

        /* 标题条（左上） */
        lv_obj_t *title = lv_label_create(r);
        lv_label_set_text(title, "Neumo");
        lv_obj_set_style_text_color(title, lv_color_hex(NEUMO_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 2);

        /* ====== 主行：从左到右布局
         * 屏幕宽 428，pad 6 -> 可用区域 ~416.
         * 布局：textarea(150) gap(8) btn(64) gap(8) switch(46) gap(10) checkbox(80) = 366
         * 剩余 ~50px 给 "状态：" 前缀 + 文字
         * 纵坐标 y=30，留 30px 给底部状态文字 + hint
         */
        const lv_coord_t y0 = 30;
        const lv_coord_t h = 36;

        /* ---- 输入框（sunken） ---- */
        textarea_ = lv_textarea_create(r);
        lv_obj_set_size(textarea_, 150, h);
        lv_obj_set_pos(textarea_, 6, y0);
        lv_textarea_set_placeholder_text(textarea_, "Type here...");
        lv_textarea_set_one_line(textarea_, true);
        lv_textarea_set_max_length(textarea_, 24);

        applyNeumoSunken(textarea_, 8);
        lv_obj_set_style_text_color(textarea_, lv_color_hex(NEUMO_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(textarea_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_pad_left(textarea_, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_right(textarea_, 10, LV_PART_MAIN);
        /* 光标颜色 */
        lv_obj_set_style_border_color(textarea_, lv_color_hex(NEUMO_ACCENT), LV_PART_CURSOR);
        lv_obj_set_style_border_width(textarea_, 2, LV_PART_CURSOR);
        lv_obj_set_style_border_opa(textarea_, LV_OPA_COVER, LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(textarea_, LV_OPA_TRANSP, LV_PART_CURSOR);
        /* placeholder 颜色 */
        lv_obj_set_style_text_color(textarea_, lv_color_hex(0x94A3B8), LV_PART_TEXTAREA_PLACEHOLDER);

        lv_obj_add_event_cb(textarea_, onTextareaFocused, LV_EVENT_FOCUSED, this);
        lv_obj_add_event_cb(textarea_, onTextareaDefocused, LV_EVENT_DEFOCUSED, this);

        /* ---- 按钮（raised → pressed 时 sunken） ---- */
        lv_obj_t *btn = lv_btn_create(r);
        lv_obj_set_size(btn, 64, h);
        lv_obj_set_pos(btn, 164, y0);
        /* LVGL 8.x 的 lv_btn_create 不会自动创建 label 子节点；
         * 早期版本（v7）的 lv_obj_get_child(btn, 0) 取 label 的写法会导致
         * lv_label_set_text(NULL, ...) 空指针崩溃（ESP32 LoadProhibited 重启）。 */
        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, "Click");
        lv_obj_center(btn_label);
        applyNeumoButton(btn);
        lv_obj_add_event_cb(btn, onBtnClick, LV_EVENT_CLICKED, this);

        /* ---- Switch（sunken 底 + 凸起指示器） ---- */
        lv_obj_t *sw = lv_switch_create(r);
        lv_obj_set_size(sw, 46, h - 8); // 46 x 28
        lv_obj_set_pos(sw, 236, y0 + 4);
        applyNeumoSwitch(sw);
        lv_obj_add_event_cb(sw, onSwitchChange, LV_EVENT_VALUE_CHANGED, this);

        /* ---- Checkbox（sunken 方框 + 勾选后填 accent） ---- */
        lv_obj_t *cb = lv_checkbox_create(r);
        lv_obj_set_size(cb, 90, h);
        lv_obj_set_pos(cb, 290, y0);
        lv_checkbox_set_text(cb, " Check");
        applyNeumoCheckbox(cb);
        lv_obj_add_event_cb(cb, onCheckboxChange, LV_EVENT_VALUE_CHANGED, this);

        /* ---- 状态显示（右上） ---- */
        status_label_ = lv_label_create(r);
        lv_label_set_text(status_label_, "btn:0 sw:off cb:off");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(NEUMO_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(status_label_, LV_ALIGN_TOP_RIGHT, -4, 2);

        /* ---- 底部 hint：复用基类模板 ---- */
        lv_obj_t *hint = lv_label_create(r);
        char hint_buf[80];
        buildHintLabel(kind(), hint_buf, sizeof(hint_buf));
        lv_label_set_text(hint, hint_buf);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x94A3B8), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 6, -2);
    }

    void NeumoPage::teardownUi()
    {
        /* 子对象会随 root 一同被 lv_obj_del，无需手动释放 */
        status_label_ = nullptr;
        textarea_ = nullptr;
    }

} // namespace ekeys