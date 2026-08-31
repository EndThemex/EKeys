/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --bpp 1 --size 16 --font F:/New_Project/FunModularKeyboard/test/LVGL/assets/BebasNeue-Regular.ttf -o F:/New_Project/FunModularKeyboard/test/LVGL/assets\ui_font_BebasNeueFont16.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "ui.h"

#ifndef UI_FONT_BEBASNEUEFONT16
#define UI_FONT_BEBASNEUEFONT16 1
#endif

#if UI_FONT_BEBASNEUEFONT16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xc,

    /* U+0022 "\"" */
    0xbb, 0xa0,

    /* U+0023 "#" */
    0x2c, 0xb2, 0xdf, 0x69, 0xa6, 0xbf, 0x49, 0x65,
    0x80,

    /* U+0024 "$" */
    0x36, 0x73, 0x8e, 0x39, 0xe3, 0xe, 0x72, 0x60,

    /* U+0025 "%" */
    0xe2, 0xa4, 0xa4, 0xa8, 0xa8, 0xff, 0x15, 0x15,
    0x25, 0x25, 0x47,

    /* U+0026 "&" */
    0x73, 0xc, 0x30, 0xd9, 0xfd, 0xb6, 0xdb, 0x6f,
    0x80,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x7b, 0x6d, 0xb6, 0xdb, 0x30,

    /* U+0029 ")" */
    0xcd, 0xb6, 0xdb, 0x6d, 0xe0,

    /* U+002A "*" */
    0x10, 0x23, 0xf1, 0x86, 0x81, 0x0,

    /* U+002B "+" */
    0x20, 0x8f, 0xc8, 0x20,

    /* U+002C "," */
    0xf8,

    /* U+002D "-" */
    0xf0,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0xc, 0x20, 0x86, 0x10, 0xc2, 0x8, 0x61, 0xc,
    0x0,

    /* U+0030 "0" */
    0x76, 0xf7, 0xbd, 0xef, 0x7b, 0xde, 0xdc,

    /* U+0031 "1" */
    0x33, 0xf3, 0x33, 0x33, 0x33, 0x30,

    /* U+0032 "2" */
    0x76, 0xf7, 0xb3, 0x99, 0xdc, 0xc6, 0x3e,

    /* U+0033 "3" */
    0x76, 0xf6, 0x31, 0x98, 0x63, 0xde, 0xdc,

    /* U+0034 "4" */
    0x18, 0xe3, 0x8e, 0x79, 0x65, 0xb6, 0xfc, 0x61,
    0x80,

    /* U+0035 "5" */
    0xfe, 0x31, 0x8f, 0x6c, 0x63, 0xde, 0xdc,

    /* U+0036 "6" */
    0x76, 0xf7, 0x8f, 0x6f, 0x7b, 0xde, 0xdc,

    /* U+0037 "7" */
    0xf8, 0xc6, 0x23, 0x18, 0xc4, 0x63, 0x18,

    /* U+0038 "8" */
    0x7b, 0x3c, 0xf3, 0xcd, 0xec, 0xf3, 0xcf, 0x37,
    0x80,

    /* U+0039 "9" */
    0x76, 0xf7, 0xbd, 0xed, 0xe3, 0xde, 0xdc,

    /* U+003A ":" */
    0xc0, 0xc,

    /* U+003B ";" */
    0xc0, 0xf, 0x80,

    /* U+003C "<" */
    0xb, 0xb0, 0xe1, 0x80,

    /* U+003D "=" */
    0xf8, 0x1, 0xf0,

    /* U+003E ">" */
    0x83, 0x86, 0xec, 0x0,

    /* U+003F "?" */
    0x7b, 0x3c, 0xf3, 0x1c, 0xe7, 0x18, 0x60, 0x6,
    0x0,

    /* U+0040 "@" */
    0xf, 0x86, 0x19, 0x81, 0xb7, 0xbd, 0xb7, 0xb6,
    0xf6, 0xde, 0xde, 0xdf, 0x8c, 0x1, 0xc2, 0xf,
    0x80,

    /* U+0041 "A" */
    0x30, 0xe3, 0x9e, 0x79, 0xa6, 0x93, 0x7f, 0x3c,
    0xc0,

    /* U+0042 "B" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x7b, 0xde, 0xfc,

    /* U+0043 "C" */
    0x76, 0xf7, 0xbc, 0x63, 0x1b, 0xde, 0xdc,

    /* U+0044 "D" */
    0xf6, 0xf7, 0xbd, 0xef, 0x7b, 0xde, 0xfc,

    /* U+0045 "E" */
    0xfe, 0x31, 0x8c, 0x7b, 0x18, 0xc6, 0x3e,

    /* U+0046 "F" */
    0xfe, 0x31, 0x8c, 0x7b, 0x18, 0xc6, 0x30,

    /* U+0047 "G" */
    0x76, 0xf7, 0xbc, 0x7f, 0x7b, 0xde, 0xdc,

    /* U+0048 "H" */
    0xcf, 0x3c, 0xf3, 0xcf, 0xfc, 0xf3, 0xcf, 0x3c,
    0xc0,

    /* U+0049 "I" */
    0xff, 0xff, 0xfc,

    /* U+004A "J" */
    0x33, 0x33, 0x33, 0x33, 0x33, 0xe0,

    /* U+004B "K" */
    0xcf, 0x6d, 0xb4, 0xf3, 0xcf, 0xb6, 0xdb, 0x3c,
    0xc0,

    /* U+004C "L" */
    0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x3e,

    /* U+004D "M" */
    0xef, 0xdf, 0xbf, 0x7e, 0xfd, 0xfb, 0xfb, 0xd7,
    0xaf, 0x58,

    /* U+004E "N" */
    0xef, 0xbe, 0xfb, 0xff, 0x7d, 0xf7, 0xdf, 0x7c,
    0xc0,

    /* U+004F "O" */
    0x76, 0xf7, 0xbd, 0xef, 0x7b, 0xde, 0xdc,

    /* U+0050 "P" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x18, 0xc6, 0x30,

    /* U+0051 "Q" */
    0x73, 0x6d, 0xb6, 0xdb, 0x6d, 0xb6, 0xdb, 0x67,
    0x83,

    /* U+0052 "R" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x7b, 0xde, 0xf6,

    /* U+0053 "S" */
    0x76, 0xf7, 0x8e, 0x38, 0xe3, 0xde, 0xdc,

    /* U+0054 "T" */
    0xf9, 0x8c, 0x63, 0x18, 0xc6, 0x31, 0x8c,

    /* U+0055 "U" */
    0xde, 0xf7, 0xbd, 0xef, 0x7b, 0xde, 0xdc,

    /* U+0056 "V" */
    0xcf, 0x34, 0xd2, 0x69, 0xa7, 0x9e, 0x38, 0xc3,
    0x0,

    /* U+0057 "W" */
    0xcd, 0xee, 0xf7, 0x4b, 0xa5, 0x52, 0xa9, 0xd4,
    0xee, 0x77, 0x3b, 0x9d, 0xc0,

    /* U+0058 "X" */
    0xcd, 0xb6, 0x8e, 0x38, 0xc3, 0x9e, 0x69, 0x3c,
    0xc0,

    /* U+0059 "Y" */
    0xcf, 0x36, 0x9e, 0x78, 0xc3, 0xc, 0x30, 0xc3,
    0x0,

    /* U+005A "Z" */
    0xf8, 0xc4, 0x63, 0x11, 0x8c, 0x46, 0x3e,

    /* U+005B "[" */
    0xfb, 0x6d, 0xb6, 0xdb, 0x70,

    /* U+005C "\\" */
    0xc1, 0x6, 0x8, 0x20, 0xc1, 0x6, 0x8, 0x20,
    0xc0,

    /* U+005D "]" */
    0xed, 0xb6, 0xdb, 0x6d, 0xf0,

    /* U+005E "^" */
    0x30, 0xe6, 0x93,

    /* U+005F "_" */
    0xf8,

    /* U+0060 "`" */
    0x40,

    /* U+0061 "a" */
    0x30, 0xe3, 0x9e, 0x79, 0xa6, 0x93, 0x7f, 0x3c,
    0xc0,

    /* U+0062 "b" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x7b, 0xde, 0xfc,

    /* U+0063 "c" */
    0x76, 0xf7, 0xbc, 0x63, 0x1b, 0xde, 0xdc,

    /* U+0064 "d" */
    0xf6, 0xf7, 0xbd, 0xef, 0x7b, 0xde, 0xfc,

    /* U+0065 "e" */
    0xfe, 0x31, 0x8c, 0x7b, 0x18, 0xc6, 0x3e,

    /* U+0066 "f" */
    0xfe, 0x31, 0x8c, 0x7b, 0x18, 0xc6, 0x30,

    /* U+0067 "g" */
    0x76, 0xf7, 0xbc, 0x7f, 0x7b, 0xde, 0xdc,

    /* U+0068 "h" */
    0xcf, 0x3c, 0xf3, 0xcf, 0xfc, 0xf3, 0xcf, 0x3c,
    0xc0,

    /* U+0069 "i" */
    0xff, 0xff, 0xfc,

    /* U+006A "j" */
    0x33, 0x33, 0x33, 0x33, 0x33, 0xe0,

    /* U+006B "k" */
    0xcf, 0x6d, 0xb4, 0xf3, 0xcf, 0xb6, 0xdb, 0x3c,
    0xc0,

    /* U+006C "l" */
    0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x3e,

    /* U+006D "m" */
    0xef, 0xdf, 0xbf, 0x7e, 0xfd, 0xfb, 0xfb, 0xd7,
    0xaf, 0x58,

    /* U+006E "n" */
    0xef, 0xbe, 0xfb, 0xff, 0x7d, 0xf7, 0xdf, 0x7c,
    0xc0,

    /* U+006F "o" */
    0x76, 0xf7, 0xbd, 0xef, 0x7b, 0xde, 0xdc,

    /* U+0070 "p" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x18, 0xc6, 0x30,

    /* U+0071 "q" */
    0x73, 0x6d, 0xb6, 0xdb, 0x6d, 0xb6, 0xdb, 0x67,
    0x83,

    /* U+0072 "r" */
    0xf6, 0xf7, 0xbd, 0xfb, 0x7b, 0xde, 0xf6,

    /* U+0073 "s" */
    0x76, 0xf7, 0x8e, 0x38, 0xe3, 0xde, 0xdc,

    /* U+0074 "t" */
    0xf9, 0x8c, 0x63, 0x18, 0xc6, 0x31, 0x8c,

    /* U+0075 "u" */
    0xde, 0xf7, 0xbd, 0xef, 0x7b, 0xde, 0xdc,

    /* U+0076 "v" */
    0xcf, 0x34, 0xd2, 0x69, 0xa7, 0x9e, 0x38, 0xc3,
    0x0,

    /* U+0077 "w" */
    0xcd, 0xee, 0xf7, 0x4b, 0xa5, 0x52, 0xa9, 0xd4,
    0xee, 0x77, 0x3b, 0x9d, 0xc0,

    /* U+0078 "x" */
    0xcd, 0xb6, 0x8e, 0x38, 0xc3, 0x9e, 0x69, 0x3c,
    0xc0,

    /* U+0079 "y" */
    0xcf, 0x36, 0x9e, 0x78, 0xc3, 0xc, 0x30, 0xc3,
    0x0,

    /* U+007A "z" */
    0xf8, 0xc4, 0x63, 0x11, 0x8c, 0x46, 0x3e,

    /* U+007B "{" */
    0x76, 0x66, 0x6c, 0x66, 0x66, 0x63,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xfc,

    /* U+007D "}" */
    0xe6, 0x66, 0x63, 0x66, 0x66, 0x6c,

    /* U+007E "~" */
    0x66, 0x60
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 41, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 54, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 86, .box_w = 4, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 6, .adv_w = 105, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 15, .adv_w = 102, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 151, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 34, .adv_w = 107, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 43, .adv_w = 48, .box_w = 1, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 44, .adv_w = 71, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 49, .adv_w = 71, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 54, .adv_w = 108, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 60, .adv_w = 102, .box_w = 6, .box_h = 5, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 64, .adv_w = 48, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 65, .adv_w = 69, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 66, .adv_w = 48, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 67, .adv_w = 100, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 76, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 83, .adv_w = 102, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 89, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 102, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 112, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 119, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 133, .adv_w = 102, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 48, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 151, .adv_w = 48, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 154, .adv_w = 102, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 158, .adv_w = 102, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 161, .adv_w = 102, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 165, .adv_w = 93, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 178, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 191, .adv_w = 103, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 103, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 98, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 214, .adv_w = 104, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 93, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 228, .adv_w = 88, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 235, .adv_w = 100, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 108, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 251, .adv_w = 49, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 254, .adv_w = 68, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 260, .adv_w = 106, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 88, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 276, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 286, .adv_w = 109, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 295, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 302, .adv_w = 99, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 309, .adv_w = 102, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 318, .adv_w = 103, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 325, .adv_w = 95, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 332, .adv_w = 93, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 339, .adv_w = 103, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 346, .adv_w = 98, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 355, .adv_w = 143, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 368, .adv_w = 104, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 377, .adv_w = 101, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 386, .adv_w = 93, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 71, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 398, .adv_w = 100, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 407, .adv_w = 71, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 412, .adv_w = 102, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 7},
    {.bitmap_index = 415, .adv_w = 77, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 416, .adv_w = 128, .box_w = 3, .box_h = 1, .ofs_x = 2, .ofs_y = 12},
    {.bitmap_index = 417, .adv_w = 103, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 426, .adv_w = 103, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 433, .adv_w = 98, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 440, .adv_w = 104, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 447, .adv_w = 93, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 454, .adv_w = 88, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 461, .adv_w = 100, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 468, .adv_w = 108, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 477, .adv_w = 49, .box_w = 2, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 480, .adv_w = 68, .box_w = 4, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 486, .adv_w = 106, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 495, .adv_w = 88, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 502, .adv_w = 138, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 512, .adv_w = 109, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 521, .adv_w = 102, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 528, .adv_w = 99, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 535, .adv_w = 102, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 544, .adv_w = 103, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 551, .adv_w = 95, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 558, .adv_w = 93, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 565, .adv_w = 103, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 572, .adv_w = 98, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 581, .adv_w = 143, .box_w = 9, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 594, .adv_w = 104, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 603, .adv_w = 101, .box_w = 6, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 612, .adv_w = 93, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 619, .adv_w = 71, .box_w = 4, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 625, .adv_w = 128, .box_w = 2, .box_h = 15, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 629, .adv_w = 71, .box_w = 4, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 635, .adv_w = 102, .box_w = 6, .box_h = 2, .ofs_x = 0, .ofs_y = 4}
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
    0, 0, 0, 0, 0, 0, -3, 0,
    0, 0, 0, -3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 3,
    0, 3, 0, 4, 0, 3, 3, 1,
    3, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, -5, 0, -16, 0,
    -19, 0, -5, -2, -9, -13, -4, 0,
    0, 0, 0, 0, 0, -36, 0, 0,
    0, -6, 0, -12, 0, 3, 0, 3,
    3, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -4, 0,
    -3, 0, 0, 0, 0, 0, -5, -4,
    0, -14, 0, 3, -12, 3, -5, 3,
    3, -8, -1, 1, -1, 0, -1, -12,
    -1, -7, -4, 1, -12, 1, -2, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -1, 0,
    -2, -4, 0, 0, 0, 0, 1, 0,
    0, 0, 0, 1, 1, 0, 1, 0,
    1, 0, 0, 0, 0, -2, -2, 0,
    -2, 0, 0, 0, -2, 0, 0, 0,
    0, 0, -1, 0, -1, 0, -1, 0,
    -1, 0, -4, -5, 0, 0, 0, 3,
    0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 1,
    0, 1, 3, 0, 4, 3, -19, -1,
    -8, 0, 1, -1, -8, 0, -13, 0,
    3, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, -3, -4, 0, 0, 0, 0, 0,
    -1, 0, -3, 0, 0, 0, -1, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, -1, 1, -5, 1, -9, 0,
    1, -3, -2, 0, -4, 0, -2, 0,
    0, 0, 0, 0, 0, 0, -19, 3,
    4, -26, 3, -19, 3, 3, -12, 0,
    1, -1, 3, 0, -20, 0, -12, -4,
    0, -23, 0, 0, 0, 0, 3, -24,
    0, -17, 0, 3, 0, -9, 0, -15,
    1, 1, 0, 0, 0, -3, -1, -1,
    -1, 0, 0, 0, 1, 0, 3, 1,
    0, 0, 0, 0, 0, 0, -1, 0,
    -1, -1, 0, -4, 0, 0, 0, 0,
    0, 0, -1, 0, 0, 0, 0, -1,
    0, 0, 0, 0, 0, 0, 0, -2,
    -2, 0, 3, 0, 4, 3, -19, -19,
    -12, -9, 1, -6, -12, -1, -13, 0,
    3, 0, 1, 1, 0, 1, 0, 0,
    0, 0, 0, -1, 0, -3, 0, 0,
    0, -1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, -1, 3, 0,
    -14, -5, -6, -4, 0, -3, -7, -1,
    -12, 0, 1, 0, 1, 1, 0, 0,
    0, 3, 0, 3, 0, -6, -2, 0,
    -1, 0, -1, -4, 0, -5, 0, 1,
    0, 1, 1, 0, 0, 0, 0, -1,
    1, -5, 1, -9, 3, 1, -3, -2,
    1, -4, 0, -2, 0, 0, 0, 0,
    0, 0, 0, 3, -5, 3, -3, -24,
    -13, -14, -10, -3, -10, -12, -5, -15,
    -4, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, -4, 0, 3,
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
const lv_font_t ui_font_BebasNeueFont16 = {
#else
lv_font_t ui_font_BebasNeueFont16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
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



#endif /*#if UI_FONT_BEBASNEUEFONT16*/

