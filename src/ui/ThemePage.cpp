#include "ui/ThemePage.h"
#include <Arduino.h>
#include <stdlib.h> /* strtoul */

namespace ekeys
{

    /* ===== 调色板（与 MenuPage.cpp / PageManager.cpp 保持一致） =====
     * 名称 -> HEX：集中维护在这里，方便走查。
     * "Label" 字段是真正渲染到色块里的短名；"name" 是给走查文档用的全称。 */
    struct Swatch
    {
        const char *label; /* 色块内显示的短名（≤ 10 字符） */
        const char *hex;   /* 6 位 HEX 字符串，便于 label 直渲 */
    };

    /* 顺序与"设计文档"分组一致：背景 / 面板 / 紫色阶梯 / 文字 / 强调 */
    static const Swatch SWATCHES[] = {
        {"BG", "050507"},
        {"Panel", "0F0A18"},
        {"Panel L", "1E162C"},
        {"Purple", "492B80"},
        {"Pri.P", "9468F1"},
        {"Br.P", "C7AAF6"},
        {"Text", "F7F5F9"},
        {"Sec.Tx", "A69FAF"},
        {"Orange", "E96F10"},
        {"Alert", "E62319"},
    };
    static constexpr uint8_t SWATCH_COUNT = sizeof(SWATCHES) / sizeof(SWATCHES[0]);

    /* 5 列 × 2 行 */
    static constexpr uint8_t COLS = 5;
    static constexpr uint8_t ROWS = 2;

    /* 网格区域（去掉左右各 8px 边距） */
    static constexpr int16_t GRID_LEFT = 8;
    /* 顶部标题 20pt@2 占 ~24px；底部 hint 14pt 占 ~14px。
     * 142 - 24 - 14 = 104 可用 → 两行 + 间距。
     * GRID_TOP=30，swatch 高 32 + 间距 14 = 46，CELL_H=60；
     * row1 起始 90，底部 90+32=122；hint @ y=128 → OK。 */
    static constexpr int16_t GRID_TOP = 30;
    /* 单格宽度：(428 - 8*2) / 5 ≈ 82.4 → 取 82 */
    static constexpr int16_t CELL_W = 82;
    /* 单格高度 = 色块 32 + 行间距 14 + 文字 14 = 60 */
    static constexpr int16_t CELL_H = 60;
    /* 色块尺寸：宽 76 / 高 32，留 3px 居中 */
    static constexpr int16_t SW_W = 76;
    static constexpr int16_t SW_H = 32;

    /* HE 文本字段：色块下方 14pt 灰字
     * HEX 文本（实际颜色值）放在色块下方居中。 */
    static constexpr int16_t HEX_FONT_H = 14;

    /* 按亮度自动选择文字色：
     *   - sRGB 折算亮度 > 0.55 → 暗色文字（用 Background #050507）
     *   - 否则 → 亮色文字（用 Text #F7F5F9）
     * 该函数对 10 个调色板值都准确，避免"白底白字"。 */
    static lv_color_t contrastTextColor(uint32_t rgb565_or_24)
    {
        /* 把 24-bit RGB 拆出来：调用方传入的是 24 位 HEX（0xRRGGBB） */
        const uint8_t r = (rgb565_or_24 >> 16) & 0xFF;
        const uint8_t g = (rgb565_or_24 >> 8) & 0xFF;
        const uint8_t b = rgb565_or_24 & 0xFF;
        /* 感知亮度近似（Rec.601）：覆盖人眼对绿更敏感的事实 */
        const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        return (lum > 140.0f) ? lv_color_hex(0x050507) : lv_color_hex(0xF7F5F9);
    }

    ThemePage::ThemePage()
        : Page(/*id=*/PAGE_THEME, "Theme", lv_color_hex(0x9468F1)) {}

    void ThemePage::buildUi()
    {
        lv_obj_t *root_obj = root();
        lv_obj_set_style_bg_opa(root_obj, LV_OPA_TRANSP, LV_PART_MAIN);

        /* 顶部标题 */
        lv_obj_t *title = lv_label_create(root_obj);
        lv_label_set_text(title, "Theme Palette");
        lv_obj_set_style_text_color(title, lv_color_hex(0xF7F5F9), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 2);

        /* 标题右上方贴 accent 小色块，呼应高亮色 */
        lv_obj_t *accent = lv_obj_create(root_obj);
        lv_obj_remove_style_all(accent);
        lv_obj_set_size(accent, 14, 14);
        lv_obj_set_style_bg_color(accent, lv_color_hex(0x9468F1), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(accent, 3, LV_PART_MAIN);
        lv_obj_align(accent, LV_ALIGN_TOP_RIGHT, -8, 6);

        /* 5x2 网格 */
        for (uint8_t i = 0; i < SWATCH_COUNT; ++i)
        {
            const uint8_t col = i % COLS;
            const uint8_t row = i / COLS;
            const int16_t cellX = GRID_LEFT + col * CELL_W;
            const int16_t cellY = GRID_TOP + row * CELL_H;

            const uint32_t rgb24 = (uint32_t)strtoul(SWATCHES[i].hex, nullptr, 16);

            /* 色块：去掉默认边框 + shadow，用 1px 描边做分隔 */
            lv_obj_t *sw = lv_obj_create(root_obj);
            lv_obj_remove_style_all(sw);
            lv_obj_set_size(sw, SW_W, SW_H);
            lv_obj_set_style_bg_color(sw, lv_color_hex(rgb24), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(sw, 4, LV_PART_MAIN);
            lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(sw, lv_color_hex(0x1E162C), LV_PART_MAIN);
            lv_obj_set_style_border_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
            /* 块内水平居中：单格中心 x = cellX + CELL_W/2 - SW_W/2 */
            lv_obj_set_pos(sw,
                           (lv_coord_t)(cellX + (CELL_W - SW_W) / 2),
                           (lv_coord_t)cellY);

            /* 颜色名称直接放在色块内部居中，文字色按 swatch 亮度自动对比。
             * 字体降到 14pt，留出 4px 上 / 4px 下内边距（SW_H=32，14pt 字符高 ~14，居中后上下各 ~9）。
             * 注意：本项目使用的 LVGL 版本未提供 lv_label_set_align()，
             *       文本对齐要通过 style 属性 LV_STYLE_TEXT_ALIGN 设置。 */
            lv_obj_t *name = lv_label_create(sw);
            lv_label_set_text(name, SWATCHES[i].label);
            lv_obj_set_style_text_color(name, contrastTextColor(rgb24), LV_PART_MAIN);
            lv_obj_set_style_text_font(name, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_align(name, LV_ALIGN_CENTER);
            lv_obj_set_width(name, SW_W - 6);
            lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

            /* HEX 文字放在色块下方居中，灰字（Secondary Text） */
            lv_obj_t *hex = lv_label_create(root_obj);
            lv_label_set_text_fmt(hex, "#%s", SWATCHES[i].hex);
            lv_obj_set_style_text_color(hex, lv_color_hex(0xA69FAF), LV_PART_MAIN);
            lv_obj_set_style_text_font(hex, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_align(hex, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            /* 居中到单格：x = cellX + (CELL_W - 56)/2（"#" + 6hex ≈ 56px@14pt） */
            lv_obj_set_pos(hex,
                           (lv_coord_t)(cellX + (CELL_W - 56) / 2),
                           (lv_coord_t)(cellY + SW_H + 2));
            lv_obj_set_width(hex, 56);
            lv_label_set_long_mode(hex, LV_LABEL_LONG_CLIP);
        }

        /* 底部 hint：只读页用 R 模板（Page::buildHintLabel） */
        lv_obj_t *hint = lv_label_create(root_obj);
        char hint_buf[80];
        buildHintLabel(kind(), hint_buf, sizeof(hint_buf));
        lv_label_set_text(hint, hint_buf);
        lv_obj_set_style_text_color(hint, lv_color_hex(0xA69FAF), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -2);
    }

    void ThemePage::teardownUi()
    {
        /* root 删掉时子树会一并释放；这里不需要额外清理 */
    }

} // namespace ekeys