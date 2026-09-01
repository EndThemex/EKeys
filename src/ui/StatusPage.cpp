#include "ui/StatusPage.h"
#include "ui/Pages.h"

namespace ekeys
{

    StatusPage::StatusPage(RGBLightControl &rgb)
        : Page(/*id=*/PAGE_STATUS, "Status", lv_color_hex(0x00FFCC)), rgb_(rgb) {}

    void StatusPage::buildUi()
    {
        lv_obj_t *root_obj = root();

        lv_obj_t *title = lv_label_create(root_obj);
        lv_label_set_text(title, "Status");
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

        lv_obj_t *line = lv_obj_create(root_obj);
        lv_obj_set_size(line, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 28);

        lv_obj_t *heap = lv_label_create(root_obj);
        lv_label_set_text(heap, "Heap: --");
        lv_obj_set_style_text_color(heap, lv_color_hex(0x00FFCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(heap, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(heap, LV_ALIGN_TOP_LEFT, 8, 40);
        heap_label_ = heap;

        lv_obj_t *up = lv_label_create(root_obj);
        lv_label_set_text(up, "Up: 0s");
        lv_obj_set_style_text_color(up, lv_color_hex(0x00FFCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(up, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(up, LV_ALIGN_TOP_LEFT, 8, 64);
        uptime_label_ = up;

        lv_obj_t *rgb = lv_label_create(root_obj);
        lv_label_set_text(rgb, "RGB: --");
        lv_obj_set_style_text_color(rgb, lv_color_hex(0xFF66CC), LV_PART_MAIN);
        lv_obj_set_style_text_font(rgb, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(rgb, LV_ALIGN_TOP_LEFT, 8, 88);
        rgb_label_ = rgb;

        lv_obj_t *hint = lv_label_create(root_obj);
        lv_label_set_text(hint, "KEY1 back");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x808080), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    }

    void StatusPage::onEnter()
    {
        enter_ms_ = millis();
        refresh();
    }

    void StatusPage::serviceTick()
    {
        /* 每帧刷一次，开销可忽略（3 个 lv_label_set_text_fmt） */
        refresh();
    }

    void StatusPage::refresh()
    {
        if (heap_label_ != nullptr)
        {
            lv_label_set_text_fmt(heap_label_, "Heap: %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
        }
        if (uptime_label_ != nullptr)
        {
            uint32_t s = (millis() - enter_ms_) / 1000;
            lv_label_set_text_fmt(uptime_label_, "Up: %us", (unsigned)s);
        }
        if (rgb_label_ != nullptr)
        {
            lv_label_set_text_fmt(rgb_label_, "RGB: %s%s",
                                  RGBLightControl::effectName(rgb_.currentEffect()),
                                  rgb_.isEnabled() ? "" : " (off)");
        }
    }

} // namespace ekeys