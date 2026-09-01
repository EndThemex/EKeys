#pragma once
#include "ui/Page.h"
#include "ble/BleKeyboardSink.h"
#include <lvgl.h>

namespace ekeys
{

    /* BLE → KeyMap 配置子页
     *
     * 功能：
     *   - 旋钮：切换当前编辑的物理按键（1..9）
     *   - KEY2：选中"profile 行" → 进入下一项：选 profile（4 个预设循环）
     *   - KEY3..KEY9：直接跳到对应物理键，进入单键编辑（按一下切换该键
     *     在"当前 profile"里的下一个候选 HID 码）
     *
     * 屏幕 428x142 布局：
     *   y=4    标题 "KeyMap"
     *   y=24   分割线
     *   y=30   Profile: <name>   <active>
     *   y=50   分割线
     *   y=58..118  3x3 矩阵：keyId 1..9 映射的小标签
     *                  选中行高亮
     *   y=124  hint: KNOB pick  KEY2 profile  K1 back
     *
     * 单击旋钮 = 等价 KEY2（profile 行循环切预设）
     */
    class KeyMapPage : public Page
    {
    public:
        KeyMapPage(BleKeyboardSink &ble);

        void onEnter() override;
        void onExit() override;

        void onEncoder(int8_t delta) override;        // 旋钮旋转：选按键
        void onConfirm() override;                   // KEY2 / 旋钮按下：切 profile
        void onSelectKey(uint8_t keyId) override;    // KEY3..9 跳到该键

    private:
        void buildUi() override;
        void teardownUi() override;
        void refresh(); // 重新生成矩阵标签 + profile 文本

        /* 把当前"选中 keyId"对应键库码 → 显示短标签（1~4 个 ASCII 字符）。
         * 直接读 BLE_KEY_MAP[1..9]，所以不需要参数。 */
        static void labelForKeyId(uint8_t keyId, char *out, size_t outLen);

        BleKeyboardSink &ble_;

        /* 当前焦点 keyId（1..9），默认 1 */
        uint8_t selectedKeyId_{1};

        /* UI 对象指针 */
        lv_obj_t *profile_label_{nullptr};
        lv_obj_t *cell_labels_[10]{nullptr}; // index 0 占位
        lv_obj_t *cell_bg_[10]{nullptr};     // 高亮底色
    };

} // namespace ekeys