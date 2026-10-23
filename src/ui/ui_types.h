/**
 * @file ui_types.h
 * @brief UI 公共基础层：LVGL 头、字体符号声明、ui_init/ui_destroy。
 *
 * 分层约定（用于减少头文件改动带来的重编译范围）：
 *   ui_types.h  ← ui_helpers.h / ui_comp*.h / 每个 ui_<屏>.h
 *   ui.h        ← 仅聚合，供需要访问全部屏幕的文件（ui.c / DisplayTask 等）使用；
 *                 普通源文件请直接 include 自己需要的 ui_<屏>.h，不要 include ui.h。
 */
#ifndef UI_TYPES_H
#define UI_TYPES_H

#if defined __has_include
#if __has_include("lvgl.h")
#include "lvgl.h"
#elif __has_include("lvgl/lvgl.h")
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif
#else
#include "lvgl.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  // EVENTS

  extern lv_obj_t *ui____initial_actions0;

  // FONTS
  LV_FONT_DECLARE(ui_font_BebasNeueFont48);
  LV_FONT_DECLARE(ui_font_BebasNeueFont24);
  LV_FONT_DECLARE(ui_font_BebasNeueFont16);
  LV_FONT_DECLARE(ui_font_BebasNeueFont28);
  LV_FONT_DECLARE(ui_font_BebasNeueFont86);
  LV_FONT_DECLARE(ui_font_BebasNeueFont36);
  LV_FONT_DECLARE(ui_font_BebasNeueFont14);
  LV_FONT_DECLARE(ui_font_FontCKJGT16);
  LV_FONT_DECLARE(ui_font_FontCKJGT24);
  LV_FONT_DECLARE(ui_font_FontCKJGT28);

  // SCREEN NAVIGATION

  typedef enum
  {
    UI_SCREEN_UNKNOWN = 0,
    UI_SCREEN_MAIN,
    UI_SCREEN_KEYMAPPED,
    UI_SCREEN_KEYMAPPED_SECONDARY,
    UI_SCREEN_MUSIC,
    UI_SCREEN_MUSIC_SECONDARY,
    UI_SCREEN_AUDIO,
    UI_SCREEN_AUDIO_SECONDARY,
    UI_SCREEN_PC_STATUS,
    UI_SCREEN_PC_STATUS_SECONDARY,
    UI_SCREEN_HA,
    UI_SCREEN_HA_SECONDARY,
    UI_SCREEN_SETTING,
    UI_SCREEN_SETTING_SECONDARY
  } ui_screen_tag_t;

  ui_screen_tag_t ui_get_active_screen_tag(void);
  void ui_set_active_screen_tag(ui_screen_tag_t tag);

  // UI INIT
  void ui_init(void);
  void ui_destroy(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
