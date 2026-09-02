#pragma once
#include "ui/Page.h"
#include "rgb/RGBLightControl.h"
#include <lvgl.h>

namespace ekeys
{

    /* RGB 控制页
     *
     * 交互：
     *   旋钮旋转 (KEY2 之外的"调整")：
     *     旋钮在 3 个"模式"之间循环——
     *       MODE_EFFECT   : 旋转切灯效
     *       MODE_BRIGHT   : 旋转调亮度
     *       MODE_ENABLE    : 旋转 toggle on/off
     *     KEY2（"进入/确认"）= 切到下一个模式
     *     旋钮单击 = 直接 toggle 启用（任何模式都生效）
     *   KEY1 = 退出回菜单
     *
     * UI：
     *   第 1 行: 标题 "RGB"
     *   第 2 行: 当前模式名 (Effect / Bright / Power)
     *   第 3 行: 当前值    (Rainbow Wave / 80% / ON)
     *   第 4 行: 提示      "KNOB adjust  KEY2 mode"
     */
    class RgbPage : public Page
    {
    public:
        RgbPage(RGBLightControl &rgb);

        /* Page API */
        void onEnter() override;
        void onEncoder(int8_t delta) override;
        void onConfirm() override;

        /* PageKind：M 多模式控制 —— KEY3..KEY9 直接进入第 idx 个模式。
         * 当前 3 个模式（Effect / Bright / Power），idx ∈ [0,2] 有效。
         * idx=0 → Effect, idx=1 → Bright, idx=2 → Power；越界返回 false。 */
        PageKind kind() const override { return PageKind::Mode; }
        bool selectMode(uint8_t idx) override;

        /* RgbPage 用旋钮调灯效/亮度/开关 → 消费旋转，不发 BLE 方向键 */
        bool consumesEncoder() const override { return true; }

    private:
        void buildUi() override;
        void refresh();

    public:
        enum class Mode : uint8_t
        {
            Effect = 0,
            Bright = 1,
            Power = 2,
            Count = 3
        };

        RGBLightControl &rgb_;
        Mode mode_{Mode::Effect};

        lv_obj_t *mode_label_{nullptr};
        lv_obj_t *value_label_{nullptr};
    };

} // namespace ekeys