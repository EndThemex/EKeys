/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --bpp 1 --size 14 --font F:/New_Project/FunModularKeyboard/test/LVGL/assets/BebasNeue-Regular.ttf -o F:/New_Project/FunModularKeyboard/test/LVGL/assets\ui_font_BebasNeueFont14.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "ui.h"

#ifndef UI_FONT_BEBASNEUEFONT14
#define UI_FONT_BEBASNEUEFONT14 1
#endif

#if UI_FONT_BEBASNEUEFONT14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xfc, 0x30,

    /* U+0022 "\"" */
    0xb6, 0x80,

    /* U+0023 "#" */
    0x29, 0xa7, 0x96, 0x59, 0x6f, 0x94, 0x51, 0x40,

    /* U+0024 "$" */
    0x23, 0xb7, 0x8e, 0x38, 0xfb, 0xdb, 0x88,

    /* U+0025 "%" */
    0xe5, 0x52, 0xa5, 0x4b, 0x1f, 0xca, 0x95, 0x2a,
    0x9c,

    /* U+0026 "&" */
    0x76, 0x31, 0xb7, 0xef, 0x7b, 0xdf, 0xc0,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x7b, 0x6d, 0xb6, 0xd9, 0x80,

    /* U+0029 ")" */
    0xcd, 0xb6, 0xdb, 0x6f, 0x0,

    /* U+002A "*" */
    0x30, 0x8f, 0xcc, 0x48,

    /* U+002B "+" */
    0x21, 0x3e, 0x42, 0x0,

    /* U+002C "," */
    0xf8,

    /* U+002D "-" */
    0xe0,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x8, 0xc4, 0x23, 0x11, 0x88, 0x46, 0x0,

    /* U+0030 "0" */
    0x76, 0xf7, 0xbd, 0xef, 0x7b, 0xdb, 0x80,

    /* U+0031 "1" */
    0x3f, 0x33, 0x33, 0x33, 0x33,

    /* U+0032 "2" */
    0x76, 0xf6, 0x33, 0xb9, 0x98, 0xc7, 0xc0,

    /* U+0033 "3" */
    0x76, 0xf6, 0x33, 0xc, 0x7b, 0xdb, 0x80,

    /* U+0034 "4" */
    0x19, 0xce, 0xf5, 0xaf, 0x7f, 0x18, 0xc0,

    /* U+0035 "5" */
    0xfe, 0x31, 0xed, 0x8c, 0x7b, 0xdb, 0x80,

    /* U+0036 "6" */
    0x76, 0xf7, 0x8f, 0xef, 0x7b, 0xdb, 0x80,

    /* U+0037 "7" */
    0xf8, 0xc6, 0x23, 0x18, 0x8c, 0x63, 0x0,

    /* U+0038 "8" */
    0x76, 0xf7, 0xb7, 0x6f, 0x7b, 0xdb, 0x80,

    /* U+0039 "9" */
    0x76, 0xf7, 0xbd, 0xfc, 0x7b, 0xdb, 0x80,

    /* U+003A ":" */
    0xc0, 0xc,

    /* U+003B ";" */
    0xc0, 0xf, 0x80,

    /* U+003C "<" */
    0x1f, 0x30, 0x70,

    /* U+003D "=" */
    0xf8, 0x3e,

    /* U+003E ">" */
    0xc1, 0x8f, 0x80,

    /* U+003F "?" */
    0x7b, 0x3c, 0xc3, 0x18, 0xc6, 0x18, 0x1, 0x80,

    /* U+0040 "@" */
    0x1e, 0x18, 0x98, 0x3b, 0xdf, 0x6f, 0xb7, 0xdd,
    0xfe, 0xc0, 0x30, 0x8f, 0x80,

    /* U+0041 "A" */
    0x31, 0x9c, 0xe5, 0xad, 0x6f, 0xce, 0x40,

    /* U+0042 "B" */
    0xf6, 0xf7, 0xbf, 0x6f, 0x7b, 0xdf, 0x80,

    /* U+0043 "C" */
    0x76, 0xf7, 0x8c, 0x63, 0x7b, 0xdb, 0x80,

    /* U+0044 "D" */
    0xf6, 0xf7, 0xbd, 0xef, 0x7b, 0xdf, 0x80,

    /* U+0045 "E" */
    0xfe, 0x31, 0x8f, 0x63, 0x18, 0xc7, 0xc0,

    /* U+0046 "F" */
    0xfe, 0x31, 0x8f, 0x63, 0x18, 0xc6, 0x0,

    /* U+0047 "G" */
    0x76, 0xf7, 0x8d, 0xef, 0x7b, 0xdb, 0x80,

    /* U+0048 "H" */
    0xde, 0xf7, 0xbf, 0xef, 0x7b, 0xde, 0xc0,

    /* U+0049 "I" */
    0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0x6d, 0xb6, 0xdb, 0x78,

    /* U+004B "K" */
    0xcb, 0x6d, 0x3c, 0xf3, 0xcd, 0xb6, 0xdb, 0x20,

    /* U+004C "L" */
    0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc7, 0xc0,

    /* U+004D "M" */
    0xef, 0xdf, 0xbf, 0x7e, 0xfd, 0xfd, 0xeb, 0xd7,
    0xac,

    /* U+004E "N" */
    0xde, 0xff, 0xff, 0xff, 0xfb, 0xde, 0xc0,

    /* U+004F "O" */
    0x76, 0xf7, 0xbd, 0xef, 0x7b, 0xdb, 0x80,

    /* U+0050 "P" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x18, 0xc6, 0x0,

    /* U+0051 "Q" */
    0x76, 0xf7, 0xbd, 0xef, 0x7b, 0xdb, 0xc2,

    /* U+0052 "R" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x7b, 0xde, 0xc0,

    /* U+0053 "S" */
    0x76, 0xf7, 0xc7, 0x38, 0xfb, 0xdb, 0x80,

    /* U+0054 "T" */
    0xfb, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0x0,

    /* U+0055 "U" */
    0xde, 0xf7, 0xbd, 0xef, 0x7b, 0xdb, 0x80,

    /* U+0056 "V" */
    0xce, 0x56, 0xb5, 0xa9, 0xce, 0x71, 0x80,

    /* U+0057 "W" */
    0xdb, 0xda, 0xda, 0x5a, 0x5a, 0x5a, 0x5a, 0x7e,
    0x66, 0x66,

    /* U+0058 "X" */
    0xc9, 0x67, 0x9c, 0x30, 0xc7, 0x16, 0x5b, 0x20,

    /* U+0059 "Y" */
    0xcd, 0x27, 0x9e, 0x30, 0xc3, 0xc, 0x30, 0xc0,

    /* U+005A "Z" */
    0xf3, 0x32, 0x66, 0x4c, 0xcf,

    /* U+005B "[" */
    0xfb, 0x6d, 0xb6, 0xdb, 0x80,

    /* U+005C "\\" */
    0xc2, 0x10, 0xc2, 0x18, 0x42, 0x18, 0x40,

    /* U+005D "]" */
    0xed, 0xb6, 0xdb, 0x6f, 0x80,

    /* U+005E "^" */
    0x31, 0x96, 0x90,

    /* U+005F "_" */
    0xf0,

    /* U+0060 "`" */
    0xc0,

    /* U+0061 "a" */
    0x31, 0x9c, 0xe5, 0xad, 0x6f, 0xce, 0x40,

    /* U+0062 "b" */
    0xf6, 0xf7, 0xbf, 0x6f, 0x7b, 0xdf, 0x80,

    /* U+0063 "c" */
    0x76, 0xf7, 0x8c, 0x63, 0x7b, 0xdb, 0x80,

    /* U+0064 "d" */
    0xf6, 0xf7, 0xbd, 0xef, 0x7b, 0xdf, 0x80,

    /* U+0065 "e" */
    0xfe, 0x31, 0x8f, 0x63, 0x18, 0xc7, 0xc0,

    /* U+0066 "f" */
    0xfe, 0x31, 0x8f, 0x63, 0x18, 0xc6, 0x0,

    /* U+0067 "g" */
    0x76, 0xf7, 0x8d, 0xef, 0x7b, 0xdb, 0x80,

    /* U+0068 "h" */
    0xde, 0xf7, 0xbf, 0xef, 0x7b, 0xde, 0xc0,

    /* U+0069 "i" */
    0xff, 0xff, 0xf0,

    /* U+006A "j" */
    0x6d, 0xb6, 0xdb, 0x78,

    /* U+006B "k" */
    0xcb, 0x6d, 0x3c, 0xf3, 0xcd, 0xb6, 0xdb, 0x20,

    /* U+006C "l" */
    0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc7, 0xc0,

    /* U+006D "m" */
    0xef, 0xdf, 0xbf, 0x7e, 0xfd, 0xfd, 0xeb, 0xd7,
    0xac,

    /* U+006E "n" */
    0xde, 0xff, 0xff, 0xff, 0xfb, 0xde, 0xc0,

    /* U+006F "o" */
    0x76, 0xf7, 0xbd, 0xef, 0x7b, 0xdb, 0x80,

    /* U+0070 "p" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x18, 0xc6, 0x0,

    /* U+0071 "q" */
    0x76, 0xf7, 0xbd, 0xef, 0x7b, 0xdb, 0xc2,

    /* U+0072 "r" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x7b, 0xde, 0xc0,

    /* U+0073 "s" */
    0x76, 0xf7, 0xc7, 0x38, 0xfb, 0xdb, 0x80,

    /* U+0074 "t" */
    0xfb, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0x0,

    /* U+0075 "u" */
    0xde, 0xf7, 0xbd, 0xef, 0x7b, 0xdb, 0x80,

    /* U+0076 "v" */
    0xce, 0x56, 0xb5, 0xa9, 0xce, 0x71, 0x80,

    /* U+0077 "w" */
    0xdb, 0xda, 0xda, 0x5a, 0x5a, 0x5a, 0x5a, 0x7e,
    0x66, 0x66,

    /* U+0078 "x" */
    0xc9, 0x67, 0x9c, 0x30, 0xc7, 0x16, 0x5b, 0x20,

    /* U+0079 "y" */
    0xcd, 0x27, 0x9e, 0x30, 0xc3, 0xc, 0x30, 0xc0,

    /* U+007A "z" */
    0xf3, 0x32, 0x66, 0x4c, 0xcf,

    /* U+007B "{" */
    0x76, 0x66, 0x6c, 0x66, 0x66, 0x70,

    /* U+007C "|" */
    0xff, 0xfc,

    /* U+007D "}" */
    0xe6, 0x66, 0x63, 0x66, 0x66, 0xe0,

    /* U+007E "~" */
    0x6c, 0xc0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 36, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 47, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 75, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 6, .adv_w = 92, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 14, .adv_w = 90, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 21, .adv_w = 132, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 30, .adv_w = 93, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 37, .adv_w = 42, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 38, .adv_w = 62, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 43, .adv_w = 62, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 48, .adv_w = 95, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 52, .adv_w = 90, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 56, .adv_w = 42, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 57, .adv_w = 60, .box_w = 3, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 58, .adv_w = 42, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 59, .adv_w = 87, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 66, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 73, .adv_w = 90, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 78, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 85, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 92, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 99, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 106, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 113, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 120, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 127, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 42, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 136, .adv_w = 42, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 139, .adv_w = 90, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 142, .adv_w = 90, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 144, .adv_w = 90, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 147, .adv_w = 81, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 155, .adv_w = 156, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 168, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 175, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 86, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 91, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 196, .adv_w = 81, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 77, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 210, .adv_w = 88, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 217, .adv_w = 94, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 224, .adv_w = 43, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 227, .adv_w = 59, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 93, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 239, .adv_w = 77, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 246, .adv_w = 121, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 255, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 86, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 276, .adv_w = 90, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 283, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 290, .adv_w = 83, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 297, .adv_w = 82, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 304, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 86, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 318, .adv_w = 125, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 328, .adv_w = 91, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 336, .adv_w = 88, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 344, .adv_w = 81, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 349, .adv_w = 62, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 354, .adv_w = 87, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 361, .adv_w = 62, .box_w = 3, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 366, .adv_w = 90, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 369, .adv_w = 67, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 370, .adv_w = 112, .box_w = 2, .box_h = 1, .ofs_x = 2, .ofs_y = 11},
    {.bitmap_index = 371, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 378, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 385, .adv_w = 86, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 392, .adv_w = 91, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 399, .adv_w = 81, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 406, .adv_w = 77, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 413, .adv_w = 88, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 420, .adv_w = 94, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 427, .adv_w = 43, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 430, .adv_w = 59, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 434, .adv_w = 93, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 442, .adv_w = 77, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 449, .adv_w = 121, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 458, .adv_w = 96, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 465, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 472, .adv_w = 86, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 479, .adv_w = 90, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 486, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 493, .adv_w = 83, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 500, .adv_w = 82, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 507, .adv_w = 90, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 514, .adv_w = 86, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 521, .adv_w = 125, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 531, .adv_w = 91, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 539, .adv_w = 88, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 547, .adv_w = 81, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 552, .adv_w = 62, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 558, .adv_w = 112, .box_w = 1, .box_h = 14, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 560, .adv_w = 62, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 566, .adv_w = 90, .box_w = 5, .box_h = 2, .ofs_x = 0, .ofs_y = 4}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 0, 0, 0, 0, 0, 0, 1,
    0, 2, 0, 0, 0, 0, 3, 0,
    4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 5, 6, 7, 8, 9, 10, 11,
    12, 0, 0, 13, 14, 15, 0, 0,
    9, 16, 9, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 2, 0, 0, 0,
    0, 0, 6, 7, 8, 9, 10, 11,
    12, 0, 0, 0, 14, 15, 0, 0,
    9, 16, 9, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 2, 0, 0, 0
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 0, 0, 1, 0, 0, 0, 2,
    1, 0, 3, 4, 0, 5, 6, 5,
    7, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 8, 8, 0, 0, 0,
    9, 10, 11, 0, 12, 0, 0, 0,
    12, 0, 0, 13, 0, 0, 0, 0,
    12, 0, 12, 0, 14, 15, 16, 17,
    18, 19, 20, 21, 0, 0, 3, 0,
    0, 0, 11, 0, 12, 0, 0, 0,
    12, 0, 0, 0, 0, 0, 0, 0,
    12, 0, 12, 0, 14, 15, 16, 17,
    18, 19, 20, 21, 0, 0, 3, 0
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -2, 0,
    0, 0, 0, -3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 2,
    0, 2, 0, 3, 0, 2, 2, 1,
    2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -4, 0, -14, 0,
    -16, 0, -4, -2, -8, -11, -3, 0,
    0, 0, 0, 0, 0, -31, 0, 0,
    0, -6, 0, -10, 0, 2, 0, 2,
    2, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -3, 0,
    -2, 0, 0, 0, 0, 0, -4, -3,
    0, -12, 0, 2, -10, 2, -4, 2,
    2, -7, -1, 1, 0, 0, -1, -10,
    0, -6, -3, 0, -10, 1, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    -2, -4, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, -2, -2, 0,
    -1, 0, 0, 0, -1, 0, 0, 0,
    0, 0, 0, 0, -1, 0, -1, 0,
    0, 0, -4, -4, 0, 0, 0, 2,
    0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 2, 0, 3, 2, -16, -1,
    -7, 0, 1, -1, -7, 0, -11, 0,
    2, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -3, -3, 0, 0, 0, 0, 0,
    0, 0, -2, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 1, -4, 1, -8, 0,
    1, -2, -2, 0, -4, 0, -2, 0,
    0, 0, 0, 0, 0, 0, -16, 2,
    3, -22, 2, -16, 2, 2, -10, 0,
    1, -1, 2, 0, -17, 0, -10, -4,
    0, -20, 0, 0, 0, 0, 2, -21,
    0, -15, 0, 2, 0, -8, 0, -13,
    1, 0, 0, 0, 0, -3, -1, 0,
    -1, 0, 0, 0, 1, 0, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 0, -4, 0, 0, 0, 0,
    0, 0, -1, 0, 0, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, -1,
    -1, 0, 2, 0, 3, 2, -16, -16,
    -10, -8, 1, -6, -10, -1, -11, 0,
    2, 0, 1, 1, 0, 1, 0, 0,
    0, 0, 0, 0, 0, -2, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, -1, 2, 0,
    -12, -4, -6, -3, 0, -2, -6, 0,
    -11, 0, 1, 0, 1, 1, 0, 0,
    0, 2, 0, 2, 0, -6, -2, 0,
    -1, 0, -1, -3, 0, -5, 0, 1,
    0, 1, 1, 0, 0, 0, 0, -1,
    1, -4, 1, -8, 2, 1, -2, -2,
    0, -4, 0, -1, 0, 0, 0, 0,
    0, 0, 0, 2, -4, 2, -2, -21,
    -11, -12, -9, -2, -9, -10, -4, -13,
    -4, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, -3, 0, 2,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 25,
    .right_class_cnt     = 21,
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
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 1,
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
const lv_font_t ui_font_BebasNeueFont14 = {
#else
lv_font_t ui_font_BebasNeueFont14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 15,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
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



#endif /*#if UI_FONT_BEBASNEUEFONT14*/

