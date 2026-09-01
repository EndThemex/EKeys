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

        static constexpr uint8_t ENTRY_COUNT = 4;
        static const MenuEntry ENTRIES_[ENTRY_COUNT];

        /* 每行的 y 坐标（与 buildUi() 中同步；只读不变量）。
         * 注意：static constexpr 数组在 GNU 工具链下需要类外定义（C++14 必须），
         * 改用类内 inline constexpr（C++17 起 ODR-usable），下面这些
         * 整数常量改为 inline constexpr value，数组仍放 .cpp。 */
        static const int16_t ROW_Y[ENTRY_COUNT]; /* 定义见 MenuPage.cpp */
        static constexpr int16_t ROW_HEIGHT = 20;
        static constexpr int16_t ROW_LEFT = 10;
        static constexpr int16_t ROW_W = SCREEN_W_PX - 20;

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
