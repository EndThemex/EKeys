/**
 * @file ui_GameScreen.h
 * @brief 游戏菜单一级页：图标 + 标题，嵌入旋钮轮播（docs/14 §4.1）。
 *
 * 键值：LEFT→AI，RIGHT→PC_STATUS，ENTER→游戏选择二级页，ESC→主页。
 */
#ifndef UI_GAME_SCREEN_H
#define UI_GAME_SCREEN_H

#include "ui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern lv_obj_t *ui_GameScreen;

    void ui_GameScreen_screen_init(void);
    void ui_GameScreen_screen_destroy(void);
    void ui_GameScreen_apply_language(uint8_t lang);
    void ui_event_GameScreen(lv_event_t *e);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
