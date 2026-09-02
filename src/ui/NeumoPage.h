#pragma once
#include "ui/Page.h"
#include <lvgl.h>

namespace ekeys
{

    /* 拟态（Neumorphism）组件展示页
     *
     * 视觉：与项目深色 PNG 背景刻意区分，本页用浅色"拟态基底" (#DDE3EA)
     *       + 一对阴影（左上浅、右下深）来模拟凸起/凹陷。
     *       屏幕 428x142 横屏，整体单行布局：输入框 + 按钮 + 开关 + 复选框。
     *
     * 注意：本页不消费旋钮，按 KEY1 退出。
     */
    class NeumoPage : public Page
    {
    public:
        NeumoPage();

        /* ReadOnly 风格：旋钮穿透到 BLE 方向键，KEY3..KEY9 全部无操作。 */
        PageKind kind() const override { return PageKind::ReadOnly; }
        bool consumesEncoder() const override { return false; }

    private:
        void buildUi() override;
        void teardownUi() override;

        /* 拟态样式辅助 */
        static void applyNeumoRaised(lv_obj_t *obj, lv_coord_t radius = 10);
        static void applyNeumoSunken(lv_obj_t *obj, lv_coord_t radius = 10);
        static void applyNeumoButton(lv_obj_t *btn);
        static void applyNeumoSwitch(lv_obj_t *sw);
        static void applyNeumoCheckbox(lv_obj_t *cb, lv_coord_t boxSize = 18);

        /* 事件回调（静态 + user_data） */
        static void onBtnClick(lv_event_t *e);
        static void onSwitchChange(lv_event_t *e);
        static void onCheckboxChange(lv_event_t *e);
        static void onTextareaFocused(lv_event_t *e);
        static void onTextareaDefocused(lv_event_t *e);

        /* 状态显示标签 */
        lv_obj_t *status_label_{nullptr};
        lv_obj_t *textarea_{nullptr};

        bool switch_on_{false};
        bool checkbox_on_{false};
        uint8_t btn_count_{0};
    };

} // namespace ekeys