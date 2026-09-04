/**
 * @file lv_conf.h
 * EKeys 项目 LVGL 8.3 配置。
 *
 * 只覆盖与 v8 默认值不同的项，其余选项由 lvgl 内部 lv_conf_internal.h
 * 提供默认值。PlatformIO 会把项目 include/ 目录加入全局头文件搜索路径，
 * lvgl 源码内的 __has_include("lv_conf.h") 能自动发现本文件，无需额外 build flag。
 *
 * 注意：本文件曾被误删导致编译报
 *   fatal error: ../../lv_conf.h: No such file or directory
 * （lv_conf_internal.h 找不到配置时回退到 lvgl 库上一级的相对路径）。
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* RGB565 渲染（与 NV3007 面板一致；0 = 小端，flush_cb 走 draw16bitRGBBitmap 分支） */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* 字体：项目 UI 实际用到 14/20/28（v8 默认只开 14）
 * LV_FONT_DEFAULT 必须与已启用的字号同步（v8 默认指向 montserrat_14）。 */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

/* 内存池：ESP32-MINI-N8 无 PSRAM、内部 DRAM 紧张，沿用 48KB 默认量级 */
#define LV_MEM_SIZE (48U * 1024U)

/* tick 由主循环 lv_tick_inc() 驱动（src/main.cpp），不用自定义 tick 源 */
#define LV_TICK_CUSTOM 0

#endif /* LV_CONF_H */
