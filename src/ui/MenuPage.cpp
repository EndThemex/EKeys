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
        {"Theme", PAGE_THEME},
        {"Backlight", PAGE_BACKLIGHT},
    };

    /* 每张卡片在「轨道」上的 x 坐标（与 buildUi() 中同步；只读不变量）。
     * 屏幕宽度 428：卡片宽 200、间距 28；
     * 第一张卡片 x = (428 - 200) / 2 = 114，使选中卡片位于屏幕中央；
     * 后续卡片沿 +X 铺开，每张卡片相对上一张 +228 (200+28)。
     * （仅前两张可见；最后一张完全在屏幕外，由顶部圆点指示全部 8 项。） */
    const int16_t MenuPage::CARD_X[MenuPage::ENTRY_COUNT] = {114, 342, 570, 798, 1026, 1254, 1482, 1710};

    /* 切换动画时长：280ms，配合 ease_in_out 形成"先慢后快再慢"的平滑到位。
     * 之前用过 overshoot（末端过冲再回弹），但 overshoot 在连续旋转时
     * 容易因每张卡片过冲方向不一致造成视觉撕裂感，已改为 ease_in_out。 */
    static constexpr uint32_t ANIM_DURATION_MS = 280;

    /* ===== 颜色辅助：集中维护，避免散落在代码各处 =====
     * 调色板：Background #050507 / Panel #0F0A18 / Panel Light #1E162C /
     *         Purple #492B80 / Primary Purple #9468F1 / Bright Purple #C7AAF6 /
     *         Text #F7F5F9 / Secondary Text #A69FAF */
    /* 屏幕底色（与 main.cpp / PageManager.cpp 保持一致） */
    static inline lv_color_t bgColor() { return lv_color_hex(0x050507); }
    /* 卡片描边：品牌主紫 */
    static inline lv_color_t cardBorderColor() { return lv_color_hex(0x9468F1); }
    /* 卡片描边更亮的外缘色 */
    static inline lv_color_t cardBorderBrightColor() { return lv_color_hex(0xC7AAF6); }
    /* 选中卡片：主文字色，高亮偏冷白 */
    static inline lv_color_t cardActiveColor() { return lv_color_hex(0xF7F5F9); }
    /* 未选中卡片：Secondary Text */
    static inline lv_color_t cardNormalColor() { return lv_color_hex(0xA69FAF); }
    /* 指示点：选中色 = Primary Purple；未选中 = Secondary Text */
    static inline lv_color_t dotActiveColor() { return lv_color_hex(0x9468F1); }
    static inline lv_color_t dotNormalColor() { return lv_color_hex(0xA69FAF); }

    MenuPage::MenuPage()
        : Page(/*id=*/PAGE_MENU, "Menu", lv_color_hex(0x9468F1)) {}

    /* ---- 静态动画回调 ---- */

    /* 用 style_translate_x 而不是 lv_obj_set_x：
     *   - lv_obj_set_x 会触发父级 layout 重算，动画逐帧调用易引起子级
     *     坐标抖动（视觉上出现"断层"）。
     *   - translate_x 只改 style，零布局开销，与基线 pos 叠加。
     * 注意：lv_obj_get_x() 在 LVGL8.3 已包含 translate 偏移，所以这里
     * 直接把动画值 v 当作"相对基线 x 的偏移"应用：起点 v=0 即不偏移。 */
    void MenuPage::animSetX(void *var, int32_t v)
    {
        lv_obj_t *obj = (lv_obj_t *)var;
        if (obj != nullptr)
            lv_obj_set_style_translate_x(obj, (lv_coord_t)v, 0);
    }

    /* 选中卡片"上抬"反馈：translate_y 0 -> -3 -> 0 (轻弹)。
     * 同样用 translate_*，避免 layout 重算。 */
    void MenuPage::animSetY(void *var, int32_t v)
    {
        lv_obj_t *obj = (lv_obj_t *)var;
        if (obj != nullptr)
            lv_obj_set_style_translate_y(obj, (lv_coord_t)v, 0);
    }

    /* 动画结束回调：本设计里**故意不做任何事**。
     *
     * 关键设计：每张卡片基线 x 永远固定在 CARD_X[i]；translate_x 是一个
     * "持续存在的偏移量"，用来表达"整组卡片相对 selected_ 当前位置的偏移"。
     * 切换动画只是把 translate_x 从一个值平滑过渡到另一个值；
     * 动画结束后 translate_x 自然停留在新值上——这就是 selected_ 当前应
     * 该呈现的最终位置。**不**清零，否则所有卡片会瞬跳回 CARD_X[i] 基线，
     * 选中卡片不再位于中央。
     *
     * 例如：selected_=0 时 translate_x=0；selected_=1 时 translate_x=-204
     * （整组左移 204，卡片 1 的可视 x = CARD_X[1] + (-204) = 328 - 204 = 124，
     * 即屏幕中央）。
     *
     * 注意：ready_cb 也会被 opacity / text_color 动画触发（a->var 同样是
     * 同一 card / label），但这些动画本身就没注册 ready_cb，所以这里其实是
     * 冗余保险——防止以后有人给 opacity 动画加 ready_cb 时被错误调用。 */
    void MenuPage::animReadyCb(lv_anim_t *a)
    {
        if (a == nullptr)
            return;
        lv_obj_t *obj = (lv_obj_t *)a->var;
        if (obj == nullptr)
            return;
        /* translate_x / translate_y 动画结束后都把对应 style 清零，
         * 让"基线"成为终值：translate_x 的终值就是 selected_ 的目标偏移，
         * translate_y 的终值就是 0（卡片回到原位）。
         *
         * 注意：当前实现里 translate_x 不在 ready_cb 里清零——
         * 见上方注释「动画结束回调：本设计里**故意不做任何事**」。
         * 这里只清 translate_y（-3px 的"上抬"反馈需要回归 0）。 */
        if (a->exec_cb == animSetY)
        {
            lv_obj_set_style_translate_y(obj, 0, 0);
        }
        /* animSetX: no-op，保持 translate_x 在终值（selected_ 偏移） */
    }

    /* ---- 指示点更新 ----
     * 设计：7 个底点永远保持"空心灰圆"样式不动；当前 selected 由
     * dots_highlight_（实心紫方块）的 x 位置表达。
     * updateDots 只在 onEnter 时调用一次，把高亮点瞬时放到 selected_ 位置。
     * 切换过程中的滑动由 animateToSelected() 启动动画完成。
     */
    void MenuPage::updateDots()
    {
        /* 7 个底点统一设为"空心灰圆"，样式不变（始终 DOT_SIZE x DOT_SIZE） */
        for (uint8_t i = 0; i < DOT_COUNT; ++i)
        {
            if (dots_[i] == nullptr)
                continue;
            lv_obj_set_style_bg_opa(dots_[i], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_color(dots_[i], dotNormalColor(), LV_PART_MAIN);
            lv_obj_set_style_border_width(dots_[i], 1, LV_PART_MAIN);
            lv_obj_set_style_radius(dots_[i], DOT_RADIUS, LV_PART_MAIN);
        }
        /* 高亮点瞬时定位到当前 selected_ 的中心 x
         * 基线 x = dots_start_x - DOT_SIZE/2（0 号底点中心）；
         * translate_x = selected_ * DOT_SPACING（与 animateToSelected 终值语义一致）。 */
        if (dots_highlight_ != nullptr)
        {
            lv_obj_set_style_translate_x(dots_highlight_, (lv_coord_t)((int32_t)selected_ * (int32_t)DOT_SPACING), 0);
            lv_obj_set_style_translate_y(dots_highlight_, 0, 0);
        }
    }

    /* ---- UI 构建 ---- */
    void MenuPage::buildUi()
    {
        lv_obj_t *root_obj = root();

        /* root 保持透明，透出屏幕级 PNG 背景图（PageManager::begin() 设置） */
        lv_obj_set_style_bg_opa(root_obj, LV_OPA_TRANSP, LV_PART_MAIN);

        /* 顶部不再画标题与分割线：原顶部标题区会与切换动画中正在"飘过中央"的
         * 卡片重叠；去掉后上方腾出约 30px 给卡片轨道，圆点指示器放在卡片上方
         * 留出 ~12px 余量，按键 hint 在卡片下方，避免所有元素互压。 */

        /* 卡片轨道：所有卡片以「基线 x」+ translate_x 表达位移
         * 基线 x = CARD_X[i]，translate_x 默认 0。
         * 切换时只动 translate_x，避免父级 layout 重算带来"断层"
         * （参见 docs/08-menu-highlight-jitter.md §2.1）。 */
        for (uint8_t i = 0; i < ENTRY_COUNT; ++i)
        {
            /* 卡片容器 */
            lv_obj_t *card = lv_obj_create(root_obj);
            lv_obj_remove_style_all(card);
            lv_obj_set_size(card, CARD_WIDTH, CARD_HEIGHT);
            /* 未选中：透明填充，仅有浅描边 */
            lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_color(card, cardBorderColor(), LV_PART_MAIN);
            lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
            lv_obj_set_style_border_opa(card, LV_OPA_40, LV_PART_MAIN);
            lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
            lv_obj_set_pos(card, CARD_X[i], CARD_Y);

            /* 卡片内的文字 label */
            lv_obj_t *label = lv_label_create(card);
            lv_label_set_text(label, ENTRIES_[i].name);
            lv_obj_set_style_text_color(label,
                                        (i == selected_) ? cardActiveColor() : cardNormalColor(),
                                        LV_PART_MAIN);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_pos(label, CARD_TEXT_PAD_L, CARD_TEXT_PAD_T);

            cards_[i] = card;
            items_[i] = label;

            /* 选中卡片：填充 + 加粗描边（描边色更亮、更不透明） */
            if (i == selected_)
            {
                lv_obj_set_style_bg_color(card, cardBorderColor(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(card, LV_OPA_30, LV_PART_MAIN);
                lv_obj_set_style_border_color(card, cardBorderBrightColor(), LV_PART_MAIN);
                lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
                lv_obj_set_style_border_opa(card, LV_OPA_COVER, LV_PART_MAIN);
            }
        }

        /* 顶部 7 个分页指示底点（空心灰圆，永远不动）：
         * 总宽 = (DOT_COUNT - 1) * DOT_SPACING = 6 * 14 = 84；
         * 居中起点 x = (428 - 84) / 2 = 172。 */
        const int16_t dots_total_w = (int16_t)((DOT_COUNT - 1) * DOT_SPACING);
        const int16_t dots_start_x = (int16_t)((SCREEN_W_PX - dots_total_w) / 2);
        for (uint8_t i = 0; i < DOT_COUNT; ++i)
        {
            lv_obj_t *dot = lv_obj_create(root_obj);
            lv_obj_remove_style_all(dot);
            /* 统一 8x8，避免 size 变化触发 layout 重算 */
            lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
            lv_obj_set_style_radius(dot, DOT_RADIUS, LV_PART_MAIN);
            /* 未选中：空心 */
            lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_color(dot, dotNormalColor(), LV_PART_MAIN);
            lv_obj_set_style_border_width(dot, 1, LV_PART_MAIN);
            /* 底点中心对齐到 dots_start_x + i * DOT_SPACING，
             * 所以底点左上角 = 中心 - DOT_SIZE/2 = 中心 - 4。 */
            const int16_t dx = (int16_t)(dots_start_x + i * DOT_SPACING - DOT_SIZE / 2);
            lv_obj_set_pos(dot, dx, DOT_Y);
            dots_[i] = dot;
        }
        /* 高亮点：1 个实心紫色方块（带圆角），8x8，
         * 初始位置由 updateDots() 设为当前 selected_ 对应的底点中心。 */
        dots_highlight_ = lv_obj_create(root_obj);
        lv_obj_remove_style_all(dots_highlight_);
        lv_obj_set_size(dots_highlight_, DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_radius(dots_highlight_, DOT_RADIUS, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dots_highlight_, dotActiveColor(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dots_highlight_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(dots_highlight_, 0, LV_PART_MAIN);
        lv_obj_set_pos(dots_highlight_, dots_start_x - DOT_SIZE / 2, DOT_Y);
        /* 把高亮点瞬时定位到 selected_ */
        updateDots();

        /* 底部提示 —— 按 docs/10-input-mapping-rule.md §5 L 类型模板。
         * 428 屏宽单行可放下模板整串（"K1 back  KNOB pick  K2 enter  K3..K9 jump"），
         * 不再拆成左右两段。 */
        lv_obj_t *hint = lv_label_create(root_obj);
        char hint_buf[80];
        buildHintLabel(kind(), hint_buf, sizeof(hint_buf));
        lv_label_set_text(hint, hint_buf);
        lv_obj_set_style_text_color(hint, lv_color_hex(0xA69FAF), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    }

    /* ---- 页面生命周期 ---- */
    void MenuPage::onEnter()
    {
        /* 回到菜单：先清掉所有残留动画，避免子页残留动画干扰。 */
        onExit();

        /* 把每张卡片瞬时归位到基线 x = CARD_X[i]，并把 translate_x
         * 设为当前 selected_ 对应的偏移量，确保"选中卡片在屏幕中央"。
         * 同时根据 selected_ 立即应用正确的"选中/未选中"样式
         * （不再依赖动画状态机，避免首帧闪烁或样式不一致）。 */
        const int32_t tx = -(int32_t)CARD_X[selected_] + (int32_t)CARD_X[0];
        for (uint8_t i = 0; i < ENTRY_COUNT; ++i)
        {
            if (cards_[i] != nullptr)
            {
                lv_obj_set_x(cards_[i], CARD_X[i]);
                lv_obj_set_style_translate_x(cards_[i], (lv_coord_t)tx, 0);
                if (i == selected_)
                {
                    /* 选中：紫色填充 + 亮紫色加粗描边 */
                    lv_obj_set_style_bg_color(cards_[i], cardBorderColor(), LV_PART_MAIN);
                    lv_obj_set_style_bg_opa(cards_[i], LV_OPA_30, LV_PART_MAIN);
                    lv_obj_set_style_border_color(cards_[i], cardBorderBrightColor(), LV_PART_MAIN);
                    lv_obj_set_style_border_width(cards_[i], 2, LV_PART_MAIN);
                    lv_obj_set_style_border_opa(cards_[i], LV_OPA_COVER, LV_PART_MAIN);
                }
                else
                {
                    /* 未选中：透明填充 + 暗紫细描边 */
                    lv_obj_set_style_bg_opa(cards_[i], LV_OPA_TRANSP, LV_PART_MAIN);
                    lv_obj_set_style_border_color(cards_[i], cardBorderColor(), LV_PART_MAIN);
                    lv_obj_set_style_border_width(cards_[i], 1, LV_PART_MAIN);
                    lv_obj_set_style_border_opa(cards_[i], LV_OPA_40, LV_PART_MAIN);
                }
            }
            if (items_[i] != nullptr)
            {
                lv_obj_set_style_text_color(items_[i],
                                            (i == selected_) ? cardActiveColor() : cardNormalColor(),
                                            LV_PART_MAIN);
            }
        }
        updateDots();
    }

    void MenuPage::onExit()
    {
        /* 离开菜单时取消未结束的动画，避免动画 lambda 在 obj 已被删除后触发。 */
        for (uint8_t i = 0; i < ENTRY_COUNT; ++i)
        {
            if (cards_[i] != nullptr)
                lv_anim_del(cards_[i], nullptr);
            if (items_[i] != nullptr)
                lv_anim_del(items_[i], nullptr);
        }
        if (dots_highlight_ != nullptr)
            lv_anim_del(dots_highlight_, nullptr);
    }

    /* ---- 动画 ---- */
    /* 切换策略（丝滑版，4 个优化合并）：
     *
     * 1) translate_x 动画接力（方案 1）：
     *    旧实现每次 onEncoder 都 lv_anim_del 杀掉旧动画，从旧值 → 新值重跑。
     *    连续转 3 格时，每一格动画都"从 0 起步"，肉眼看到"反复抽动"。
     *    新实现：保留旧动画，只改 values 把"起点"设为当前 translate_x 实测值，
     *    让动画从「现在所在位置」→「新目标」平滑接续。
     *
     * 2) 选中样式瞬时切换：颜色/描边/填充在切换瞬间就生效，不做 lerp。
     *    卡片位置滑动是动画，二者解耦。
     *
     * 3) 选中卡片"上抬"反馈（方案 3）：新选中卡片做
     *    translate_y 0 → -3 → 0 的轻弹动画（overshoot path 让中间值过冲），
     *    与位置滑动同步启动。ready_cb 把 translate_y 清零。
     *
     * 4) 高亮点滑动（方案 4）：dots_highlight_ 用 translate_x
     *    从旧底点中心 → 新底点中心。translate_x 表示相对 0 号底点的偏移
     *    = selected_ * DOT_SPACING。 */
    void MenuPage::animateToSelected(uint8_t fromIdx, uint8_t toIdx)
    {
        if (fromIdx == toIdx)
            return;

        /* ===== 1) 选中样式瞬时切换 ===== */
        auto applyCardStyle = [this](uint8_t idx, bool active)
        {
            lv_obj_t *card = cards_[idx];
            if (card == nullptr)
                return;
            if (active)
            {
                lv_obj_set_style_bg_color(card, cardBorderColor(), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(card, LV_OPA_30, LV_PART_MAIN);
                lv_obj_set_style_border_color(card, cardBorderBrightColor(), LV_PART_MAIN);
                lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
                lv_obj_set_style_border_opa(card, LV_OPA_COVER, LV_PART_MAIN);
            }
            else
            {
                lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, LV_PART_MAIN);
                lv_obj_set_style_border_color(card, cardBorderColor(), LV_PART_MAIN);
                lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
                lv_obj_set_style_border_opa(card, LV_OPA_40, LV_PART_MAIN);
            }
        };
        applyCardStyle(fromIdx, false);
        applyCardStyle(toIdx, true);

        if (items_[fromIdx] != nullptr)
            lv_obj_set_style_text_color(items_[fromIdx], cardNormalColor(), LV_PART_MAIN);
        if (items_[toIdx] != nullptr)
            lv_obj_set_style_text_color(items_[toIdx], cardActiveColor(), LV_PART_MAIN);

        /* ===== 2) 卡片 translate_x 动画（接力式） =====
         * 起点 = 当前 translate_x 实测值（接力）；终点 = toIdx 对应的偏移。
         *
         * 注意：currentTx0 来自 cards_[0]，但每张卡片都各自跑动画，
         * 各自有自己的"当前值"。理论上 LVGL 同一帧内所有动画用同一个 dt，
         * 所以每张卡片的中途值应该完全一致；但用同一实测值做起点
         * 是更安全的做法（消除任何可能的 stagger 累积误差）。 */
        const int32_t toTx = -(int32_t)CARD_X[toIdx] + (int32_t)CARD_X[0];
        const int32_t currentTx0 = (cards_[0] != nullptr)
                                       ? (int32_t)lv_obj_get_style_translate_x(cards_[0], LV_PART_MAIN)
                                       : toTx;

        for (uint8_t i = 0; i < ENTRY_COUNT; ++i)
        {
            if (cards_[i] == nullptr)
                continue;
            /* 仅删旧的 animSetX 动画（不删 animSetY，避免打断上抬） */
            lv_anim_del(cards_[i], animSetX);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, cards_[i]);
            lv_anim_set_exec_cb(&a, animSetX);
            /* 关键：每张卡片都用同一个 currentTx0 作为起点，
             * 保证 7 张卡片在动画起点和过程中永远保持相对位移一致
             * （i > 0 的卡片相对 i = 0 的差值始终 = CARD_X[i] - CARD_X[0]），
             * 视觉上"整组平移"没有 stagger。 */
            lv_anim_set_values(&a, currentTx0, toTx);
            lv_anim_set_time(&a, ANIM_DURATION_MS);
            lv_anim_set_ready_cb(&a, animReadyCb);
            lv_anim_set_user_data(&a, this);
#if LV_USE_PATH_DEFAULT
            /* 改用 ease_in_out：取消 overshoot。
             * overshoot 在动画末端让卡片"过冲再回弹"，
             * 但 LVGL 8.3 的 overshoot 是固定公式，过冲量与动画进度无关——
             * 用户连续旋转时看到的"撕裂"很大一部分来自每张卡片
             * overshoot 过冲方向不一致造成的视觉不齐。
             * ease_in_out 是平滑减速到位，没有反弹，
             * 与卡片整体平移的语义（"滑到目标位置"）更贴合。 */
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
#endif
            lv_anim_start(&a);
        }

        /* ===== 3) 选中卡片 translate_y 上抬反馈（接力式） =====
         * 起点 = 当前 translate_y 实测值；终点 = 0（卡片回到原位）。
         * 路径 = ease_in_out：从 -3px 平滑减速回 0，
         * 不再用 overshoot 弹跳（连续旋转时弹跳方向不一致会撕裂）。 */
        if (cards_[toIdx] != nullptr)
        {
            const int32_t currentTy = (int32_t)lv_obj_get_style_translate_y(cards_[toIdx], LV_PART_MAIN);
            lv_anim_del(cards_[toIdx], animSetY);
            lv_anim_t ay;
            lv_anim_init(&ay);
            lv_anim_set_var(&ay, cards_[toIdx]);
            lv_anim_set_exec_cb(&ay, animSetY);
            lv_anim_set_values(&ay, currentTy, 0);
            lv_anim_set_time(&ay, ANIM_DURATION_MS);
            lv_anim_set_ready_cb(&ay, animReadyCb);
            lv_anim_set_user_data(&ay, this);
#if LV_USE_PATH_DEFAULT
            lv_anim_set_path_cb(&ay, lv_anim_path_ease_in_out);
#endif
            lv_anim_start(&ay);
        }

        /* ===== 4) 高亮点 translate_x 滑动（接力式） ===== */
        if (dots_highlight_ != nullptr)
        {
            const int32_t targetDotTx = (int32_t)toIdx * (int32_t)DOT_SPACING;
            const int32_t currentDotTx = (int32_t)lv_obj_get_style_translate_x(dots_highlight_, LV_PART_MAIN);

            lv_anim_del(dots_highlight_, animSetX);
            lv_anim_t ad;
            lv_anim_init(&ad);
            lv_anim_set_var(&ad, dots_highlight_);
            lv_anim_set_exec_cb(&ad, animSetX);
            lv_anim_set_values(&ad, currentDotTx, targetDotTx);
            lv_anim_set_time(&ad, ANIM_DURATION_MS);
            lv_anim_set_ready_cb(&ad, animReadyCb);
            lv_anim_set_user_data(&ad, this);
#if LV_USE_PATH_DEFAULT
            lv_anim_set_path_cb(&ad, lv_anim_path_ease_in_out);
#endif
            lv_anim_start(&ad);
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
     * 当前共 7 项，idx ∈ [0,6]；越界返回 false，基类直接丢弃。 */
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
