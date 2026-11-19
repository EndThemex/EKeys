/*
 * TetrisGame.h
 *
 * 俄罗斯方块游戏门面（docs/14 §3.2 / §8.1）：
 *   - 对外暴露 C 链接 API，供 C 层 UI（ui_GameSelectScreen.c）与
 *     C++ 层 DisplayTask 调用；
 *   - 持有 TetrisCore，驱动重力时钟、输入分派、画布/HUD 渲染、LED 调色板；
 *   - 负责竖屏进入/退出的同步切换（切 rotation → LVGL 分辨率 → 建屏 → 开局）。
 */
#ifndef EKEYS_GAME_TETRIS_GAME_H
#define EKEYS_GAME_TETRIS_GAME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* 进入游戏（由游戏选择页 ENTER 调用）：同步切竖屏 + 建屏 + 开局 */
    void tetrisGame_enter(void);

    /* 退出游戏（暂停中双击 / GAME OVER 双击）：恢复横屏 + 回游戏菜单 */
    void tetrisGame_exit(void);

    /* 输入分派：action = LV_KEY_* 或 kMatrixKeyActionBase + key_id */
    void tetrisGame_handleInput(uint8_t action);

    /* 重力 / 锁定推进，由 DisplayTask 每帧调用（仅竖屏游戏页激活时生效） */
    void tetrisGame_tick(uint16_t dt_ms);

    /* 当前是否处于游戏页（DisplayTask 据此决定是否 tick / 屏蔽） */
    bool tetrisGame_isActive(void);

    /* 完全重开一局（GAME OVER 单击） */
    void tetrisGame_restart(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* EKEYS_GAME_TETRIS_GAME_H */
