#pragma once
#include "ui/Page.h"
#include "ble/BleKeyboardSink.h"
#include <lvgl.h>

namespace ekeys
{

    /* 蓝牙状态页：显示 BLE 连接状态、设备名、EKEYS_ENABLE_BLE 开关。
     *
     * 该页只读展示，不修改任何 BLE 状态。旋钮 / KEY2 无操作；
     * KEY1 退出由 PageManager 默认行为处理。
     *
     * 屏幕 428x142 布局：
     *   y=4   标题 "BLE"
     *   y=28  分割线
     *   y=40  Status: <text>          （绿/黄/灰 三色对应 Connected/Advertising/Disabled）
     *   y=64  Device: <name>
     *   y=88  Vendor: <name>
     *   底右   "KEY1 back"
     */
    class BlePage : public Page
{
    public:
        BlePage(BleKeyboardSink &ble);

        void onEnter() override;

        /* 交互：KEY2 / 旋钮按下 = 进入 KeyMap 子页 */
        void onConfirm() override;
        void onSelectKey(uint8_t /*keyId*/) override;

        /* PageKind：A 即时动作 —— KEY3..KEY9 直接触发第 idx 个动作。
         *   idx=0 → toggle BLE 开关  (KEY3)
         *   idx=1 → reconnect         (KEY4) —— 预留
         *   idx=2 → clear bond        (KEY5) —— 预留
         *   idx>=3 → 越界返回 false，基类丢弃。 */
        PageKind kind() const override { return PageKind::Action; }
        bool selectAction(uint8_t idx) override;

        /* 每帧调用一次，更新状态文本（避免陈旧）。 */
        void serviceTick();

        /* 旋钮旋转不用于本页 UI 调整 → 穿透到 BLE 方向键（前进/后退）。
         * 但单击/KEY2 由本页消费用于 toggle 开关。 */
        bool consumesEncoder() const override { return false; }

    private:
        void buildUi() override;
        void refresh();

        BleKeyboardSink &ble_;

        lv_obj_t *status_label_{nullptr};
        lv_obj_t *device_label_{nullptr};
        lv_obj_t *vendor_label_{nullptr};
    };

} // namespace ekeys
