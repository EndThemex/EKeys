#include "ui/BlePage.h"
#include "ui/Pages.h"

namespace ekeys
{

    /* 与 BleKeyboardSink.cpp 中构造保持一致；不依赖第三方库的 getter。 */
    static constexpr const char *BLE_VENDOR_NAME = "EKeys Inc";

    BlePage::BlePage(BleKeyboardSink &ble)
        : Page(/*id=*/PAGE_BLE, "BLE", lv_color_hex(0x4DA3FF)), ble_(ble) {}

    void BlePage::buildUi()
    {
        lv_obj_t *root_obj = root();

        lv_obj_t *title = lv_label_create(root_obj);
        lv_label_set_text(title, "BLE");
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

        lv_obj_t *line = lv_obj_create(root_obj);
        lv_obj_set_size(line, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 28);

        /* 状态行：ON/OFF + Connected/Advertising */
        lv_obj_t *status = lv_label_create(root_obj);
        lv_label_set_text(status, "State: --");
        lv_obj_set_style_text_color(status, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(status, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(status, LV_ALIGN_TOP_LEFT, 8, 36);
        status_label_ = status;

        lv_obj_t *device = lv_label_create(root_obj);
        lv_label_set_text(device, "Device: --");
        lv_obj_set_style_text_color(device, lv_color_hex(0x4DA3FF), LV_PART_MAIN);
        lv_obj_set_style_text_font(device, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(device, LV_ALIGN_TOP_LEFT, 8, 60);
        device_label_ = device;

        lv_obj_t *vendor = lv_label_create(root_obj);
        lv_label_set_text(vendor, "Vendor: --");
        lv_obj_set_style_text_color(vendor, lv_color_hex(0x9CA3AF), LV_PART_MAIN);
        lv_obj_set_style_text_font(vendor, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(vendor, LV_ALIGN_TOP_LEFT, 8, 84);
        vendor_label_ = vendor;

        lv_obj_t *hint = lv_label_create(root_obj);
        char hint_buf[80];
        buildHintLabel(kind(), hint_buf, sizeof(hint_buf));
        lv_label_set_text(hint, hint_buf);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    }

    void BlePage::onEnter()
    {
        refresh();
    }

    void BlePage::onConfirm()
    {
        /* KEY2 / 旋钮按下 = 进入 KeyMap 配置子页 */
        requestPush(PAGE_KEYMAP);
    }

    void BlePage::onSelectKey(uint8_t /*keyId*/)
    {
        /* 基类已按 kind() 路由到 selectAction(idx)；本函数保留为空避免误导。 */
    }

    /* A 类型 selectAction：
     *   idx=0 → toggle BLE 开关
     *   idx=1 → reconnect（预留）
     *   idx=2 → clear bond（预留）
     *   idx>=3 → 越界返回 false。
     *
     * 注：reconnect / clear bond 当前 BleKeyboardSink 接口未必暴露，
     * 这里先占位，确保规则可声明；后续接入时直接实现 idx=1/2 即可。 */
    bool BlePage::selectAction(uint8_t idx)
    {
        switch (idx)
        {
        case 0:
            ble_.setEnabled(!ble_.isEnabled());
            refresh();
            return true;
        case 1:
            /* TODO: reconnect —— 待 BleKeyboardSink 提供接口后实现 */
            return false;
        case 2:
            /* TODO: clear bond —— 待 BleKeyboardSink 提供接口后实现 */
            return false;
        default:
            return false;
        }
    }

    void BlePage::serviceTick()
    {
        /* 每帧刷一次：开销仅 3 次文本设置 + 1 次 isConnected()，可忽略。 */
        refresh();
    }

    void BlePage::refresh()
    {
        if (status_label_ == nullptr)
            return;

#if EKEYS_ENABLE_BLE
        const bool enabled = ble_.isEnabled();
        const bool connected = ble_.isConnected();

        if (!enabled)
        {
            lv_label_set_text(status_label_, "State: OFF");
            lv_obj_set_style_text_color(status_label_, lv_color_hex(0x808080), LV_PART_MAIN);
        }
        else if (connected)
        {
            lv_label_set_text(status_label_, "State: ON  |  Connected");
            lv_obj_set_style_text_color(status_label_, lv_color_hex(0x00FFCC), LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text(status_label_, "State: ON  |  Advertising");
            lv_obj_set_style_text_color(status_label_, lv_color_hex(0xFFCC00), LV_PART_MAIN);
        }

        if (device_label_ != nullptr)
            lv_label_set_text_fmt(device_label_, "Device: %s", EKEYS_DEVICE_NAME);
        if (vendor_label_ != nullptr)
            lv_label_set_text(vendor_label_, BLE_VENDOR_NAME);
#else
        lv_label_set_text(status_label_, "State: Disabled");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0x808080), LV_PART_MAIN);
        if (device_label_ != nullptr)
            lv_label_set_text(device_label_, "Device: -");
        if (vendor_label_ != nullptr)
            lv_label_set_text(vendor_label_, "Vendor: -");
#endif
    }

} // namespace ekeys
