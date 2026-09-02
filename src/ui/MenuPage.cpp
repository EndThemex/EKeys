#include "ui/MenuPage.h"
#include "ui/Pages.h"
#include <Arduino.h>

namespace ekeys
{

    /* 菜单项表：顺序即旋钮旋转方向。 */
    const MenuPage::MenuEntry MenuPage::ENTRIES_[MenuPage::ENTRY_COUNT] = {
        {"RGB Control", PAGE_RGB},
        {"Pomodoro", PAGE_TOMATO},
        {"Mic", PAGE_MIC},
        {"Status", PAGE_STATUS},
        {"BLE", PAGE_BLE},
        {"Neumo", PAGE_NEUMO},
    };

    /* 每行的 y 坐标（与 buildUi() 中同步；只读不变量）。
     * 这里提供 ODR-used 的定义；constexpr 修饰暗示它在编译期可获取，
     * 但为了让链接器找到符号，必须有这一个 .cpp 中的定义。
     * 屏幕高度 142：6 行起始 24/42/60/78/96/114，每行高 18，最后一行底部 132，留出 ~10px 给底部 hint。 */
    const int16_t MenuPage::ROW_Y[MenuPage::ENTRY_COUNT] = {24, 42, 60, 78, 96, 114};

    static constexpr uint32_t ANIM_DURATION_MS = 200;

    /* ===== 颜色辅助：集中维护，避免散落在代码各处 ===== */
    /* 屏幕底色（与 main.cpp / PageManager.cpp 保持一致） */
    static inline lv_color_t bgColor() { return lv_color_hex(0x101820); }
    /* 高亮条：品牌亮蓝偏青，靠对比+饱和度来"抢眼"，不是单纯提亮灰 */
    static inline lv_color_t highlightColor() { return lv_color_hex(0x2A87FF); }
    /* 高亮条更亮的外缘描边（透明叠加） */
    static inline lv_color_t highlightBorderColor() { return lv_color_hex(0x5FB1FF); }
    /* 选中行：纯白 + 略偏暖，整体亮度高 */
    static inline lv_color_t rowActiveColor() { return lv_color_hex(0xFFFFFF); }
    /* 未选中行：比之前的 #808080 更暗，且带蓝调，与高亮条统一冷色调 */
    static inline lv_color_t rowNormalColor() { return lv_color_hex(0x5C6470); }
    /* 选中行右侧的"激活态"小指示色（> 圆点） */
    static inline lv_color_t indicatorColor() { return lv_color_hex(0x5FB1FF); }

    MenuPage::MenuPage()
        : Page(/*id=*/PAGE_MENU, "Menu", lv_color_hex(0xFFFFFF)) {}

    /* ---- 静态动画回调 ---- */

    void MenuPage::animSetY(void *var, int32_t v)
    {
        /* 用 style_translate_y 而不是 lv_obj_set_y：
         *   - lv_obj_set_y 会触发父级 layout 重算，动画逐帧调用易引起子级
         *     坐标抖动（视觉上出现"断层"）。
         *   - translate_y 只改 style，零布局开销，与基线 pos 叠加。
         * 注意：lv_obj_get_y() 在 LVGL8.3 已包含 translate 偏移，所以这里
         * 直接把动画值 v 当作"相对基线 y 的偏移"应用：起点 v=0 即不偏移。 */
        lv_obj_t *obj = (lv_obj_t *)var;
        if (obj != nullptr)
            lv_obj_set_style_translate_y(obj, (lv_coord_t)v, 0);
    }

    /* 透明度插值：v 是 0..255 的归一化进度，直接套到 obj 的 bg_opa / opa */
    void MenuPage::animSetOpacity(void *var, int32_t v)
    {
        lv_obj_t *obj = (lv_obj_t *)var;
        if (obj != nullptr)
            lv_obj_set_style_bg_opa(obj, (lv_opa_t)v, LV_PART_MAIN);
    }

    /* 文字颜色插值：v 0..255，mix 在 rowNormalColor -> rowActiveColor 之间 */
    void MenuPage::animSetTextColor(void *var, int32_t v)
    {
        lv_obj_t *obj = (lv_obj_t *)var;
        if (obj != nullptr)
        {
            lv_color_t mixed = lv_color_mix(rowActiveColor(), rowNormalColor(), (uint8_t)(255 - v));
            lv_obj_set_style_text_color(obj, mixed, LV_PART_MAIN);
        }
    }

    void MenuPage::animReadyCb(lv_anim_t *a)
    {
        /* 动画结束时把 translate_y 复位为 0，并把基线 y 推到当前 selected_ 行。
         * 这样下一次 onEncoder 启动新动画时，translate_y 起点是干净的 0，
         * 避免连续旋转时高亮条/指示符瞬跳回旧基线。 */
        if (a == nullptr)
            return;
        MenuPage *self = (MenuPage *)a->user_data;
        if (self == nullptr)
            return;
        lv_obj_t *obj = (lv_obj_t *)a->var;
        if (obj == nullptr)
            return;
        if (obj != self->highlight_ && obj != self->indicator_)
            return;
        lv_obj_set_y(obj, ROW_Y[self->selected_]);
        lv_obj_set_style_translate_y(obj, 0, 0);
    }

    /* ---- UI 构建 ---- */
    void MenuPage::buildUi()
    {
        lv_obj_t *root_obj = root();

        /* root 保持透明，透出屏幕级 PNG 背景图（PageManager::begin() 设置） */
        lv_obj_set_style_bg_opa(root_obj, LV_OPA_TRANSP, LV_PART_MAIN);

        /* 顶部标题栏 */
        lv_obj_t *title = lv_label_create(root_obj);
        lv_label_set_text(title, "EKeys Menu");
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

        /* 顶部分割线：颜色比之前更冷亮一点，配合蓝色高亮条 */
        lv_obj_t *line = lv_obj_create(root_obj);
        lv_obj_set_size(line, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x1F2A38), LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 28);

        /* 高亮条 rect —— 先创建，这样后面的 labels 在 z 序上层
         * 重要：去除所有 LVGL shadow。shadow 在每帧把绘制包围盒扩大，
         * 触发布局/重算，导致动画过程中矩形左右出现 1~2px 的"断层"。
         * 用 1px 边框 + 微透明度代替"光感"。 */
        highlight_ = lv_obj_create(root_obj);
        lv_obj_remove_style_all(highlight_);
        /* 高亮条宽度先给一个占位值，待 label 创建完毕、按"最长标题"测量后
         * 再用 lv_obj_set_width() 改成真实宽度（见下方）。这样矩形只包住文字，
         * 不再横跨全屏。 */
        lv_obj_set_size(highlight_, 64, ROW_HEIGHT);
        lv_obj_set_style_bg_color(highlight_, highlightColor(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(highlight_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(highlight_, 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(highlight_, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(highlight_, highlightBorderColor(), LV_PART_MAIN);
        lv_obj_set_style_border_opa(highlight_, LV_OPA_60, LV_PART_MAIN);
        lv_obj_set_pos(highlight_, ROW_LEFT, ROW_Y[selected_]);
        /* 显式把布局关成 NONE。
         *  - LVGL 8.3 公共头不暴露 lv_layout_t / LV_LAYOUT_NONE / LV_LAYOUT_OFF
         *    这类"NONE"符号（layout 默认值就是 0）。
         *  - 我们也不需要 NONE 之外的其它 layout（菜单条不参与 flex / grid），
         *    所以直接不调用 lv_obj_set_layout()，依赖默认 NONE。
         * 保留这块空注释占位，便于以后若需要 grid/flex 时再设置。 */
        /* lv_obj_set_layout(highlight_, 0);  // 默认即 NONE，无需调用 */

        /* 菜单项 label：纯文本 + pad 让出 indicator */
        int16_t maxLabelW = 0;
        for (uint8_t i = 0; i < ENTRY_COUNT; ++i)
        {
            lv_obj_t *row = lv_label_create(root_obj);
            lv_label_set_text(row, ENTRIES_[i].name);
            lv_obj_set_style_text_color(row,
                                        (i == selected_) ? rowActiveColor() : rowNormalColor(),
                                        LV_PART_MAIN);
            lv_obj_set_style_text_font(row, &lv_font_montserrat_20, LV_PART_MAIN);
            lv_obj_set_pos(row, ROW_LEFT + 18, ROW_Y[i]);
            items_[i] = row;
            /* LVGL 8.3：在 label 上调用 lv_obj_update_layout() 后，
             * 可用 lv_obj_get_self_size() / lv_obj_get_width() 取得真实渲染宽度。*/
            lv_obj_update_layout(row);
            const int16_t w = lv_obj_get_self_width(row);
            if (w > maxLabelW)
                maxLabelW = w;
        }

        /* 根据"最长标题"重新调整高亮条尺寸：
         *   宽 = 文字区域起始 (ROW_LEFT+18) - 高亮条起始 (ROW_LEFT)
         *       + 最长标题像素宽度 + 右侧余量 (ROW_BAR_PAD_R)
         * 这样高亮条刚好包住文字，不再跨满全屏。 */
        if (highlight_ != nullptr && maxLabelW > 0)
        {
            const int16_t barW = (int16_t)(18 + (int32_t)maxLabelW + ROW_BAR_PAD_R);
            lv_obj_set_width(highlight_, barW);
        }

        /* 选中指示符：> 圆点三角形 —— 静态绘制，不参与动画
         * 仅当存在 selected_ 项；本菜单永远有一个 selected_。这里只画
         * 跟随 highlight_ 的 y 位置（在 onEncoder 里同步移动）。 */
        indicator_ = lv_label_create(root_obj);
        lv_label_set_text(indicator_, ">");
        lv_obj_set_style_text_color(indicator_, indicatorColor(), LV_PART_MAIN);
        lv_obj_set_style_text_font(indicator_, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_pos(indicator_, ROW_LEFT + 4, ROW_Y[selected_]);

        /* 底部提示 —— 按 docs/10-input-mapping-rule.md §5 L 类型模板。
         * 428 屏宽单行可放下模板整串（"K1 back  KNOB pick  K2 enter  K3..K9 jump"），
         * 不再拆成左右两段。 */
        lv_obj_t *hint = lv_label_create(root_obj);
        char hint_buf[80];
        buildHintLabel(kind(), hint_buf, sizeof(hint_buf));
        lv_label_set_text(hint, hint_buf);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x6B7280), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    }

    /* ---- 页面生命周期 ---- */
    void MenuPage::onEnter()
    {
        /* 回到菜单：把高亮条 / 指示符瞬时归位，并把 translate_y 清零。
         * 用 translate_y 替代 set_y，避免 layout 重算带来的"断层"。 */
        if (highlight_ != nullptr)
        {
            lv_obj_set_y(highlight_, ROW_Y[selected_]);
            lv_obj_set_style_translate_y(highlight_, 0, 0);
        }
        if (indicator_ != nullptr)
        {
            lv_obj_set_y(indicator_, ROW_Y[selected_]);
            lv_obj_set_style_translate_y(indicator_, 0, 0);
        }
        for (uint8_t i = 0; i < ENTRY_COUNT; ++i)
        {
            if (items_[i] == nullptr)
                continue;
            lv_obj_set_style_text_color(items_[i],
                                        (i == selected_) ? rowActiveColor() : rowNormalColor(),
                                        LV_PART_MAIN);
        }
    }

    void MenuPage::onExit()
    {
        /* 离开菜单时取消未结束的动画，避免动画 lambda 在 obj 已被删除后触发。 */
        if (highlight_ != nullptr)
            lv_anim_del(highlight_, nullptr);
        if (indicator_ != nullptr)
            lv_anim_del(indicator_, nullptr);
        for (uint8_t i = 0; i < ENTRY_COUNT; ++i)
        {
            if (items_[i] != nullptr)
                lv_anim_del(items_[i], nullptr);
        }
    }

    /* ---- 动画 ---- */
    void MenuPage::animateToSelected(uint8_t fromIdx, uint8_t toIdx)
    {
        if (highlight_ == nullptr || fromIdx == toIdx)
            return;

        /* 高亮条：基线 y 已经在 lv_obj_set_pos() 中写好。
         * 动画只通过 translate_y 改相对位移，避开 lv_obj_set_y
         * 触发的父级 layout 重算。
         * 动画结束时由 animReadyCb 把基线 y 推到新行 + translate_y 清零，
         * 防止连续旋转时 translate_y 残留导致高亮条瞬跳回旧基线。 */
        const int32_t deltaY = (int32_t)ROW_Y[toIdx] - (int32_t)ROW_Y[fromIdx];

        /* 1) 高亮条 y 滑动：translate_y 从 0 -> deltaY */
        {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, highlight_);
            lv_anim_set_exec_cb(&a, animSetY);
            lv_anim_set_values(&a, 0, deltaY);
            lv_anim_set_time(&a, ANIM_DURATION_MS);
            lv_anim_set_ready_cb(&a, animReadyCb);
            lv_anim_set_user_data(&a, this);
#if LV_USE_PATH_DEFAULT
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
#endif
            lv_anim_start(&a);

            /* 透明度脉冲 */
            lv_anim_t a_op;
            lv_anim_init(&a_op);
            lv_anim_set_var(&a_op, highlight_);
            lv_anim_set_exec_cb(&a_op, animSetOpacity);
            lv_anim_set_values(&a_op, (int32_t)LV_OPA_70, (int32_t)LV_OPA_COVER);
            lv_anim_set_time(&a_op, ANIM_DURATION_MS);
            lv_anim_start(&a_op);
        }

        /* 2) 指示符 > 也跟着滑。ready_cb 让它动画结束时也清零 translate_y。 */
        if (indicator_ != nullptr)
        {
            lv_anim_t ai;
            lv_anim_init(&ai);
            lv_anim_set_var(&ai, indicator_);
            lv_anim_set_exec_cb(&ai, animSetY);
            lv_anim_set_values(&ai, 0, deltaY);
            lv_anim_set_time(&ai, ANIM_DURATION_MS);
            lv_anim_set_ready_cb(&ai, animReadyCb);
            lv_anim_set_user_data(&ai, this);
#if LV_USE_PATH_DEFAULT
            lv_anim_set_path_cb(&ai, lv_anim_path_ease_out);
#endif
            lv_anim_start(&ai);
        }

        /* 3) 旧选中行文字从 white -> normalColor（lerp 出去） */
        if (items_[fromIdx] != nullptr)
        {
            lv_anim_t ac;
            lv_anim_init(&ac);
            lv_anim_set_var(&ac, items_[fromIdx]);
            lv_anim_set_exec_cb(&ac, animSetTextColor);
            lv_anim_set_values(&ac, 0, 255); /* 0=完全 active, 255=完全 normal */
            lv_anim_set_time(&ac, ANIM_DURATION_MS);
#if LV_USE_PATH_DEFAULT
            lv_anim_set_path_cb(&ac, lv_anim_path_ease_in_out);
#endif
            lv_anim_start(&ac);
        }

        /* 4) 新选中行文字从 normalColor -> white（lerp 进入） */
        if (items_[toIdx] != nullptr)
        {
            lv_anim_t ac2;
            lv_anim_init(&ac2);
            lv_anim_set_var(&ac2, items_[toIdx]);
            lv_anim_set_exec_cb(&ac2, animSetTextColor);
            lv_anim_set_values(&ac2, 255, 0); /* 255=完全 normal, 0=完全 active */
            lv_anim_set_time(&ac2, ANIM_DURATION_MS);
#if LV_USE_PATH_DEFAULT
            lv_anim_set_path_cb(&ac2, lv_anim_path_ease_in_out);
#endif
            lv_anim_start(&ac2);
        }
    }

    /* ---- 旋钮 / 确认 ---- */
    void MenuPage::onEncoder(int8_t delta)
    {
        int16_t next = (int16_t)selected_ + delta;
        if (next < 0)
            next = (int16_t)ENTRY_COUNT - 1;
        else if (next >= (int16_t)ENTRY_COUNT)
            next = 0;
        uint8_t from = selected_;
        selected_ = (uint8_t)next;
        animateToSelected(from, selected_);
    }

    void MenuPage::onConfirm()
    {
        /* KEY2 = 进入当前选中页 */
        requestPush(ENTRIES_[selected_].pageId);
    }

    /* L 类型 selectItem：KEY3..KEY9 直接跳到第 idx 项（idx = keyId - 3）。
     * 当前共 5 项，idx ∈ [0,4]；越界返回 false，基类直接丢弃。 */
    bool MenuPage::selectItem(uint8_t idx)
    {
        if (idx >= ENTRY_COUNT)
            return false;
        const uint8_t from = selected_;
        selected_ = idx;
        animateToSelected(from, selected_);
        return true;
    }

} // namespace ekeys
