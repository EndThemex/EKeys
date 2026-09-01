#include "ui/RgbPage.h"
#include "ui/Pages.h"
#include <FastLED.h>

namespace ekeys
{

    RgbPage::RgbPage(RGBLightControl &rgb)
        : Page(/*id=*/PAGE_RGB, "RGB", lv_color_hex(0xFF66CC)), rgb_(rgb) {}

    static const char *modeName(RgbPage::Mode m)
    {
        switch (m)
        {
        case RgbPage::Mode::Effect:
            return "Effect";
        case RgbPage::Mode::Bright:
            return "Bright";
        case RgbPage::Mode::Power:
            return "Power";
        default:
            return "?";
        }
    }

    void RgbPage::buildUi()
    {
        lv_obj_t *root_obj = root();

        lv_obj_t *title = lv_label_create(root_obj);
        lv_label_set_text(title, "RGB");
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

        lv_obj_t *line = lv_obj_create(root_obj);
        lv_obj_set_size(line, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 28);

        /* 模式行 */
        lv_obj_t *mode_row = lv_label_create(root_obj);
        lv_label_set_text(mode_row, "Mode: --");
        lv_obj_set_style_text_color(mode_row, lv_color_hex(0x00FFCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(mode_row, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(mode_row, LV_ALIGN_TOP_LEFT, 8, 40);
        mode_label_ = mode_row;

        /* 当前值行 */
        lv_obj_t *val_row = lv_label_create(root_obj);
        lv_label_set_text(val_row, "Value: --");
        lv_obj_set_style_text_color(val_row, lv_color_hex(0xFF66CC), LV_PART_MAIN);
        lv_obj_set_style_text_font(val_row, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(val_row, LV_ALIGN_TOP_LEFT, 8, 70);
        value_label_ = val_row;

        /* 提示行 */
        lv_obj_t *hint = lv_label_create(root_obj);
        lv_label_set_text(hint, "KEY1 back");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    }

    void RgbPage::onEnter()
    {
        refresh();
    }

    void RgbPage::onEncoder(int8_t delta)
    {
        switch (mode_)
        {
        case Mode::Effect:
            /* 不允许通过旋钮跳到 Off（Off 是"灯效 0"）。要切到 Off，
             * 进 Power 模式用单击或旋转。 */
            rgb_.cycleEffect(delta);
            break;
        case Mode::Bright:
        {
            /* 步长 16 / 步；FastLED 0~255 */
            int16_t b = (int16_t)rgb_.brightness() + delta * 16;
            if (b < 0)
                b = 0;
            if (b > 255)
                b = 255;
            rgb_.setBrightnessLevel((uint8_t)b);
            break;
        }
        case Mode::Power:
            if (delta > 0)
                rgb_.setEnabled(true);
            else
                rgb_.setEnabled(false);
            break;
        default:
            break;
        }
        refresh();
    }

    void RgbPage::onConfirm()
    {
        /* KEY2 / 旋钮按下 = 切到下一个模式。
         * 注：原"任意模式下单击 = toggle on/off"已统一到"进入/确认"语义，
         * 可通过进入 Power 模式后旋转来切换 on/off。 */
        mode_ = (Mode)(((uint8_t)mode_ + 1) % (uint8_t)Mode::Count);
        refresh();
    }

    void RgbPage::refresh()
    {
        if (mode_label_ != nullptr)
        {
            lv_label_set_text_fmt(mode_label_, "Mode: %s", modeName(mode_));
        }
        if (value_label_ != nullptr)
        {
            switch (mode_)
            {
            case Mode::Effect:
                lv_label_set_text_fmt(value_label_, "Value: %s",
                                      RGBLightControl::effectName(rgb_.currentEffect()));
                break;
            case Mode::Bright:
                lv_label_set_text_fmt(value_label_, "Value: %u/255", rgb_.brightness());
                break;
            case Mode::Power:
                lv_label_set_text_fmt(value_label_, "Value: %s",
                                      rgb_.isEnabled() ? "ON" : "OFF");
                break;
            default:
                break;
            }
        }
    }

} // namespace ekeys