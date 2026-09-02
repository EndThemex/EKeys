#pragma once
#include "ui/Page.h"
#include "ui/Pages.h" /* SCREEN_W_PX / SCREEN_H_PX — 这里要提前 include，下面 CARD_* 会用到 */
#include <lvgl.h>

namespace ekeys
{

    /* 主菜单页：旋钮在卡片之间循环（横向滑动），KEY2 进入当前项，KEY1 退出。
     *
     * 视觉：每个菜单项是一张"卡片"，从屏幕左缘开始横向铺开；
     * 选中卡片停在屏幕中央附近（露出右侧下一张卡片作为"即将到来"的提示），
     * 卡片带紫色描边 + 半透明填充。底部有一排分页指示点。
     * 切换时通过 translate_x 平滑滑动整组卡片。
     *
     * 注意：根据当前设计，旋钮在其他子页会交给子页自身处理（用于调整数值等），
     * 所以本类的 onEncoder() 只负责切菜单项。 */
    class MenuPage : public Page
    {
    public:
        MenuPage();

        /* Page API：旋钮切项，KEY2 进入 */
        void onEncoder(int8_t delta) override;
        void onConfirm() override;
        void onEnter() override;
        void onExit() override;

        /* PageKind：L 列表选择 —— KEY3..KEY9 直接跳到第 idx 项（idx = keyId - 3）。
         * 当前菜单共 7 项，idx ∈ [0,6] 有效；超出范围 → 返回 false，无操作。 */
        PageKind kind() const override { return PageKind::List; }
        bool selectItem(uint8_t idx) override;

        /* 菜单页的旋钮旋转 = 切菜单项 + 发 BLE 方向键（不消费） */
        bool consumesEncoder() const override { return false; }

    private:
        void buildUi() override;

        /* 动画 / 高亮 */
        void animateToSelected(uint8_t fromIdx, uint8_t toIdx);
        static void animSetX(void *var, int32_t v);
        static void animReadyCb(lv_anim_t *a);

        /* 指示点更新：把 dots_[selected_] 设为实心，其它设为空心 */
        void updateDots();

        struct MenuEntry
        {
            const char *name;
            uint8_t pageId;
        };

        static constexpr uint8_t ENTRY_COUNT = 7;
        static const MenuEntry ENTRIES_[ENTRY_COUNT];

        /* 每张卡片在「轨道」上的 x 坐标（与 buildUi() 中同步；只读不变量）。
         * 屏幕宽度 428：卡片 180px + 间距 24px，第一张 x = (428-180)/2 = 124，
         * 让选中卡片位于屏幕中央；其余卡片沿 +X 方向铺开。
         * 计算：124 + i * (180 + 24) = 124 + i * 204 */
        static const int16_t CARD_X[ENTRY_COUNT]; /* 定义见 MenuPage.cpp */

        /* 卡片视觉常量
         *
         * 屏幕 142×428：去掉顶部标题后，可分配给卡片的纵向空间 ≈ 100px。
         * 选 CARD_HEIGHT = 88（占 100px 中的 88%），CARD_WIDTH = 200；
         * 横向 CARD_GAP = 28，使右侧"下一张卡片"露出 ≈ 200px 提示。
         *
         * 纵向布局：
         *   顶部圆点指示器：y=10（直径 8，占据 6~14）
         *   卡片轨道起点：y=24（距圆点 10px）
         *   卡片底：24 + 88 = 112
         *   底部按键 hint：LV_ALIGN_BOTTOM_RIGHT（y≈128，距卡片底 16px）
         *   切换动画中卡片可在中央"飘过"，不会撞到 hint。 */
        static constexpr int16_t CARD_WIDTH = 200;
        static constexpr int16_t CARD_HEIGHT = 88;
        static constexpr int16_t CARD_GAP = 28;
        static constexpr int16_t CARD_Y = 24;
        /* 卡片内文字相对卡片左上角的偏移（更大的卡片留更大内边距） */
        static constexpr int16_t CARD_TEXT_PAD_L = 16;
        static constexpr int16_t CARD_TEXT_PAD_T = 16;

        /* 指示点视觉常量 */
        static constexpr uint8_t DOT_COUNT = ENTRY_COUNT;
        static constexpr int16_t DOT_RADIUS = 3;
        static constexpr int16_t DOT_RADIUS_ACTIVE = 4;
        static constexpr int16_t DOT_SPACING = 14;
        /* 圆点放在屏幕最顶部 (y=10)，圆点直径 8，居中后下边缘在 y=14，
         * 与卡片顶 (y=24) 留 10px 间距。 */
        static constexpr int16_t DOT_Y = 10;

        /* 当前选中项索引 */
        uint8_t selected_{0};

        /* 菜单项卡片容器（每个卡片是一个 lv_obj，用 style_border_* 表达选中态） */
        lv_obj_t *cards_[ENTRY_COUNT]{nullptr};
        /* 菜单项 label（每个 label 挂在对应 card 上） */
        lv_obj_t *items_[ENTRY_COUNT]{nullptr};
        /* 指示点 */
        lv_obj_t *dots_[DOT_COUNT]{nullptr};
    };

} // namespace ekeys
