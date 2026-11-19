/**
 * @file ui_GameSelectScreen.h
 * @brief 游戏选择二级页：旋转选游戏，ENTER 启动（docs/14 §4.2）。
 *
 * 键值：LEFT/RIGHT→切换选中，ENTER→启动选中游戏，ESC→回游戏菜单。
 * 本页注册后不接收矩阵键（DisplayTask 丢弃）；旋钮走 LV_EVENT_KEY。
 */
#ifndef UI_GAME_SELECT_SCREEN_H
#define UI_GAME_SELECT_SCREEN_H

#include "ui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern lv_obj_t *ui_GameSelectScreen;

    void ui_GameSelectScreen_screen_init(void);
    void ui_GameSelectScreen_screen_destroy(void);
    void ui_GameSelectScreen_apply_language(uint8_t lang);
    void ui_event_GameSelectScreen(lv_event_t *e);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
