#pragma once
#include "ui/Page.h"
#include "rgb/RGBLightControl.h"
#include <lvgl.h>

namespace ekeys
{

    /* 系统状态页：显示空闲堆、uptime、RGB 状态。
     * 旋钮 / KEY2 无操作；KEY1 退出。 */
    class StatusPage : public Page
    {
    public:
        StatusPage(RGBLightControl &rgb);

        void onEnter() override;

        /* PageKind：R 只读展示 —— 不响应 KEY3..KEY9 与旋钮。 */
        PageKind kind() const override { return PageKind::ReadOnly; }

        /* 每帧调用一次，刷新 heap/uptime/rgb 文本。
         * 不放 LVGL 定时器是因为 frame tick 已经由 loop() 推动。 */
        void serviceTick();

        /* StatusPage 是只读展示 → 旋钮穿透到 BLE 方向键 */
        bool consumesEncoder() const override { return false; }

    private:
        void buildUi() override;
        void refresh();

        RGBLightControl &rgb_;

        lv_obj_t *heap_label_{nullptr};
        lv_obj_t *uptime_label_{nullptr};
        lv_obj_t *rgb_label_{nullptr};

        uint32_t enter_ms_{0};
    };

} // namespace ekeys