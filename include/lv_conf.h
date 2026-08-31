/*
 * lv_conf.h
 *
 * 本地 LVGL 配置覆盖（docs/02-display-lvgl-port.md §2.1）。
 *
 * 用法：在 platformio.ini 的 build_flags 追加：
 *
 *     -DLV_CONF_INCLUDE_SIMPLE
 *     -I include
 *
 * LVGL 8.3.11 通过这两个宏找到本文件。
 *
 * 本文件只覆盖与本项目实际相关的开关；未列出的项保持
 * lv_conf_template.h 中的默认值。
 *
 * 参考基线版本：lvgl@8.3.11（与 platformio.ini lib_deps 对齐）。
 *
 * 注意：头守卫名固定为 LV_CONF_H，lv_conf_internal.h 通过该宏判断
 * 用户是否提供了 lv_conf.h。
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* clang-format off */

/*====================
   COLOR SETTINGS
 *====================*/

/* 16-bit RGB565 与 Arduino_GFX::draw16bitRGBBitmap 对齐 */
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0    /* 与 src/display/LvglPort.cpp 中 #if 分支对齐 */

/* 半透明合成：本项目不使用 */
#define LV_COLOR_SCREEN_TRANSP 0

/*====================
   MEMORY SETTINGS
 *====================*/

/* 使用 LVGL 自带内存池，heap 由 lv_init() 后 48 KB 起步即可 */
#define LV_MEM_CUSTOM      0
#define LV_MEM_SIZE        (48U * 1024U)
#define LV_MEM_BUF_MAX_NUM 16

/*===========================
   HAL / TICK 设置
 *===========================*/

/* 使用 millis()，与 src/display/LvglPort::tick() 路径一致 */
#define LV_TICK_CUSTOM     0

/*====================
   调试 / 断言
 *====================*/

/* 阶段 06 之后再考虑打开更深度的断言以减少构建尺寸 */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* 仅打印错误日志；debug 日志关闭以减少二进制体积 */
#define LV_USE_LOG 0

/*====================
   字体设置
 *====================*/

/* 阶段 01 / 02 仅用到 20 / 28 两种 Montserrat；其它字号按需开 */
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* 压缩版（bpp=3）体积更小；本阶段不启用 */
#define LV_FONT_MONTSERRAT_20_COMPRESSED 0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0

/* 默认字体：14 已关闭，指向已启用的 20，否则编译报 undeclared */
#define LV_FONT_DEFAULT &lv_font_montserrat_20

/*====================
   Demo / 示例
 *====================*/

/* 关闭所有 LVGL 自带 demo（FEATURE_DOC §8.4：界面自定义） */
#define LV_USE_DEMO_WIDGETS        0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK      0
#define LV_USE_DEMO_STRESS         0
#define LV_USE_DEMO_MUSIC          0

/*====================
   主题 / 控件（按需最小集）
 *====================*/

/* 阶段 05 SquareLine 接管前手动添加的 label / btn / arc 等基础控件保留默认开启 */

#endif  /* LV_CONF_H */
