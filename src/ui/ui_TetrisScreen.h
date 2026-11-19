/**
 * @file ui_TetrisScreen.h
 * @brief 俄罗斯方块竖屏游戏页（docs/14 §4.3）：142×428 全屏。
 *
 * 本页不注册 LV_EVENT_KEY 导航——旋钮输入由 DisplayTask 直接分派给
 * tetrisGame_handleInput()，避免误触切屏。仅提供绘图原语供 TetrisGame 门面调用。
 *
 * 布局：左侧 3×4 实体键位提示图（末行两键 + 旋钮）+ 右侧 NEXT/HOLD/分数
 *       + 10×20 索引4bit画布 + 底部操作提示/状态。
 */
#ifndef UI_TETRIS_SCREEN_H
#define UI_TETRIS_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern lv_obj_t *ui_TetrisScreen;

    void ui_TetrisScreen_screen_init(void);
    void ui_TetrisScreen_screen_destroy(void);
    void ui_TetrisScreen_apply_language(uint8_t lang);

    /* 棋盘重绘：cells[20][10] 的值 0=空 1..7=方块 8=幽灵；仅变化时调用 */
    void ui_TetrisScreen_draw_board(const uint8_t cells[20][10]);

    /* HUD：分数 / 等级 / 消行 / 连击 */
    void ui_TetrisScreen_set_hud(uint32_t score, uint8_t level,
                                 uint32_t lines, int16_t combo);

    /* 预览：next[0..2] 与 hold（-1=无）；用形状字母显示 */
    void ui_TetrisScreen_set_preview(int8_t next0, int8_t next1, int8_t next2,
                                     int8_t hold, bool hold_used);

    /* 状态覆盖层：state 0=游戏中 1=暂停 2=结束；text 可为空 */
    void ui_TetrisScreen_set_status(uint8_t state, const char *text);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
