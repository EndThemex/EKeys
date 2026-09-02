#pragma once
#include "ui/Page.h"
#include "ui/Pages.h" /* SCREEN_W_PX / SCREEN_H_PX — 这里要提前 include，下面 ROW_W 会用到 */
#include <lvgl.h>

namespace ekeys
{

    /* 主菜单页：旋钮在菜单项之间循环，KEY2 进入当前项，KEY1 退出。
     *
     * 视觉：每项是一行标题，底部有一个高亮条（rect）跟随选中项平滑滑动，
     * 选中行的文字由"普通灰"变成"白色"。整体采用 LVGL 动画 API，资源占用极低。
     *
     * 注意：根据当前设计，旋钮在其他子页会交给子页自身处理（用于调整数值等），
     * 所以本类的 onEncoder() 只负责切菜单项。
     */
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
         * 当前菜单共 5 项，所以 idx ∈ [0,4] 有效；KEY3=idx0, KEY4=idx1, ...
         * KEY8=idx5 已越界 → 返回 false，无操作；KEY9=idx6 同上。 */
        PageKind kind() const override { return PageKind::List; }
        bool selectItem(uint8_t idx) override;

        /* 菜单页的旋钮旋转 = 切菜单项 + 发 BLE 方向键（不消费） */
        bool consumesEncoder() const override { return false; }

    private:
        void buildUi() override;

        /* 动画 / 高亮 */
        void animateToSelected(uint8_t fromIdx, uint8_t toIdx);
        static void animSetY(void *var, int32_t v);
        static void animSetOpacity(void *var, int32_t v);
        static void animSetTextColor(void *var, int32_t v);
        static void animReadyCb(lv_anim_t *a);

        struct MenuEntry
        {
            const char *name;
            uint8_t pageId;
        };

        static constexpr uint8_t ENTRY_COUNT = 7;
        static const MenuEntry ENTRIES_[ENTRY_COUNT];

        /* 每行的 y 坐标（与 buildUi() 中同步；只读不变量）。
         * 注意：static constexpr 数组在 GNU 工具链下需要类外定义（C++14 必须），
         * 改用类内 inline constexpr（C++17 起 ODR-usable），下面这些
         * 整数常量改为 inline constexpr value，数组仍放 .cpp。 */
        static const int16_t ROW_Y[ENTRY_COUNT]; /* 定义见 MenuPage.cpp */
        static constexpr int16_t ROW_HEIGHT = 16;
        static constexpr int16_t ROW_LEFT = 10;
        /* 高亮条宽度 = 当前所有菜单项中最长标题的渲染宽度 + 左右内边距。
         * 不再固定为全屏宽度，避免出现长条拖出整个屏幕的视觉负担。 */
        static constexpr int16_t ROW_BAR_PAD_R = 10; /* 文字右侧余量 */

        /* 当前选中项索引 */
        uint8_t selected_{0};

        /* 菜单项 label */
        lv_obj_t *items_[ENTRY_COUNT]{nullptr};

        /* 高亮条 */
        lv_obj_t *highlight_{nullptr};

        /* 选中指示符 ">" */
        lv_obj_t *indicator_{nullptr};
    };

} // namespace ekeys
