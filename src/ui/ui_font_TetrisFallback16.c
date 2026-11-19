/*******************************************************************************
 * 俄罗斯方块键位提示后备字体（16px / 1bpp）
 *
 * 原 ui_font_FontCKJGT16 字库缺：转、软、逆、重、钮、退，
 * 导致「旋转/软降/逆转/重开/旋钮/退出」等提示显示为方块。
 * 本文件只生成这 6 个字形，经 ui_font_FontCKJGT16.fallback 链补全；
 * 其余字符仍使用原字体，避免整体风格变化。
 *
 * 生成命令（LVGL lv_font_conv 1.5.3，源字体为系统黑体）：
 * npx lv_font_conv --font C:/Windows/Fonts/simhei.ttf --bpp 1 --size 16 \
 *   --format lvgl --symbols 转软逆重钮退 -o src/ui/ui_font_TetrisFallback16.c
 ******************************************************************************/

/* 与项目其它 ui_font_*.c 一致：由 ui_types.h 统一处理 lvgl.h 的包含路径 */
#include "ui_types.h"

#ifndef UI_FONT_TETRISFALLBACK16
#define UI_FONT_TETRISFALLBACK16 1
#endif

#if UI_FONT_TETRISFALLBACK16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+8F6C "转" */
    0x30, 0x60, 0x40, 0x83, 0xf7, 0xf1, 0x2, 0x4,
    0x4, 0xb, 0x7f, 0xf7, 0x20, 0x7e, 0x40, 0x18,
    0xfc, 0x30, 0x13, 0xf0, 0x40, 0xcf, 0x1, 0x86,
    0x3, 0x3, 0x0,

    /* U+8F6F "软" */
    0x10, 0x40, 0x60, 0x83, 0xfb, 0x1, 0x7, 0xe2,
    0x18, 0x4b, 0x25, 0x96, 0x8, 0x7f, 0x10, 0x18,
    0x20, 0x30, 0xc0, 0xf9, 0x47, 0xc6, 0x81, 0x88,
    0x83, 0x60, 0xc6, 0x40, 0x0,

    /* U+9000 "退" */
    0x67, 0xf8, 0x48, 0x10, 0x90, 0x21, 0x3f, 0xc0,
    0x40, 0x80, 0x81, 0x39, 0xfe, 0x12, 0x46, 0x24,
    0xd8, 0x48, 0xe0, 0x9e, 0x61, 0x30, 0x67, 0x80,
    0x10, 0xff, 0x80,

    /* U+9006 "逆" */
    0x2, 0x10, 0xc6, 0x20, 0x80, 0x81, 0x3f, 0xe0,
    0x4, 0x0, 0x8, 0x39, 0x12, 0x12, 0x24, 0x27,
    0xf8, 0x41, 0x90, 0x82, 0x1, 0x38, 0x7, 0xa0,
    0x10, 0xff, 0x80,

    /* U+91CD "重" */
    0x0, 0xe3, 0xfc, 0x0, 0x81, 0xff, 0xf0, 0x20,
    0x1f, 0xf0, 0x88, 0x87, 0xfc, 0x22, 0x21, 0xff,
    0x0, 0x80, 0xff, 0xe0, 0x20, 0x7f, 0xfc,

    /* U+94AE "钮" */
    0x20, 0x1, 0x8f, 0xe7, 0x88, 0xb0, 0x22, 0xf8,
    0x88, 0x8f, 0xe2, 0x8, 0xbe, 0x22, 0x20, 0x88,
    0x82, 0x22, 0x88, 0x8c, 0x62, 0x67, 0xfc
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 256, .box_w = 15, .box_h = 14, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 27, .adv_w = 256, .box_w = 15, .box_h = 15, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 56, .adv_w = 256, .box_w = 15, .box_h = 14, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 83, .adv_w = 256, .box_w = 15, .box_h = 14, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 110, .adv_w = 256, .box_w = 13, .box_h = 14, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 133, .adv_w = 256, .box_w = 14, .box_h = 13, .ofs_x = 1, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x3, 0x94, 0x9a, 0x261, 0x542
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 36716, .range_length = 1347, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 6, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_TetrisFallback16 = {
#else
lv_font_t ui_font_TetrisFallback16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 15,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_TETRISFALLBACK16*/

