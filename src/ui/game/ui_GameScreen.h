/*
 * ui_GameScreen.h
 *
 * 程序化创建的前台玩法屏（docs/13 §4.1）。
 * 不走 SquareLine 生成，避免重导出覆盖；主题系统后续再接入。
 */

#ifndef UI_GAME_SCREEN_H
#define UI_GAME_SCREEN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

    /* 惰性创建并返回游戏屏（首次调用时创建） */
    lv_obj_t *ui_GameScreen_create(void);

    /* 更新屏上提示文本（label 会拷贝字符串） */
    void ui_GameScreen_set_text(const char *text);

    /* 底部 11 格按键指示条：key_id 1~11 高亮/熄灭（越界忽略） */
    void ui_GameScreen_set_cell(uint8_t key_id, bool on);

    /* 指示条全部熄灭/点亮（回合边界 / 失败反馈用） */
    void ui_GameScreen_set_all_cells(bool on);

    /* 销毁游戏屏（当前不在游戏屏时调用） */
    void ui_GameScreen_destroy(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* UI_GAME_SCREEN_H */
