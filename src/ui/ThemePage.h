#pragma once
#include "ui/Page.h"
#include "ui/Pages.h"
#include <lvgl.h>

namespace ekeys
{

    /* 主题色查看页（Theme）
     *
     * 用途：在屏端"看一眼"当前 UI 调色板的全部颜色，辅助 UI 走查 / 设计评审。
     * 屏幕 428x142：顶部标题 + 5×2 网格，每格一个色块 + 名称 + HEX。
     * KEY1 退出；本页为只读展示（不消费旋钮）。
     */
    class ThemePage : public Page
    {
    public:
        ThemePage();

        /* ReadOnly：旋钮穿透到 BLE 方向键，KEY3..KEY9 全部无操作 */
        PageKind kind() const override { return PageKind::ReadOnly; }
        bool consumesEncoder() const override { return false; }

    private:
        void buildUi() override;
        void teardownUi() override;
    };

} // namespace ekeys