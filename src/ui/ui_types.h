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
  /* FontCKJGT16 原字库缺 转/软/逆/重/钮/退；该后备字体只含这 6 字，
   * 通过 ui_font_FontCKJGT16.fallback 链补全，其余字符仍用原字体。 */
  LV_FONT_DECLARE(ui_font_TetrisFallback16);
  LV_FONT_DECLARE(ui_font_FontCKJGT28);
  /* 48px 精简字体：仅含 "EKeys" 5 个字母 + 8 个内置 LV_SYMBOL 字形，
   * 替代体积 94 KB 的 lv_font_montserrat_48（见 ui_font_Symbol48.c 头注释）。
   * 如需在 48px 上显示其它字符，必须重新生成该字体。 */
  LV_FONT_DECLARE(ui_font_Symbol48);

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
    UI_SCREEN_SETTING_SECONDARY,
    /* AI 一级入口页（图标 + 标题）；旋钮单击进入 AI_SECONDARY 才开始收发 */
    UI_SCREEN_AI,
    /* AI 图像流二级页（docs/13）：矩阵键 101~111 不作焦点跳转，HID 照发 */
    UI_SCREEN_AI_SECONDARY,
    /* 游戏菜单一级页 / 游戏选择二级页 / 俄罗斯方块竖屏游戏页（docs/14） */
    UI_SCREEN_GAMES,
    UI_SCREEN_GAME_SELECT,
    UI_SCREEN_TETRIS
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
