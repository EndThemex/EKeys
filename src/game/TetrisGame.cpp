/*
 * TetrisGame.cpp
 *
 * 俄罗斯方块门面实现（docs/14 §4/§5/§6）：
 *   enter  → setRotation(竖) → setOrientation → 懒建屏 → load → tag → LED → reset
 *   input  → 旋钮 LEFT/RIGHT/ENTER/ESC + 矩阵键 101~111 映射到 Core 动作
 *   tick   → 重力推进 + 仅在状态变化时重绘画布/HUD
 *   exit   → 清 LED → setRotation(横) → setOrientation → load 菜单 → tag
 *
 * 全程在 DisplayTask 线程内同步执行（enter 由 UI 事件回调调用），
 * 保证横竖屏切换与切屏之间没有 lv_timer_handler 插入导致花屏。
 */
#include "TetrisGame.h"

#include <Arduino.h>

#include "display/DisplayDriver.h"
#include "display/LvglPort.h"
#include "game/TetrisConfig.h"
#include "game/TetrisCore.h"
#include "message_types.h"
#include "rgb/RGBDriver.h"
#include "rgb/RGBLightControl.h"
#include "ui/ui_GameScreen.h"
#include "ui/ui_TetrisScreen.h"

namespace
{
    using namespace ekeys;
    using namespace ekeys::tetris;

    Core g_core;
    bool g_active = false;

    /* 把 8 个功能色 + 3 个熄灭色填成 11 颗 LED 调色板 */
    void applyGameLedPalette()
    {
        RgbColor palette[RGBDriver::kLedCount];
        for (uint8_t i = 0; i < RGBDriver::kLedCount; ++i)
        {
            if (i < kActionLedCount)
            {
                const Rgb &c = kActionColors[i];
                palette[i] = {c.r, c.g, c.b};
            }
            else
            {
                palette[i] = {0, 0, 0};
            }
        }
        RGBLightControl::instance().setGamePalette(palette);
    }

    /* Core 状态 → UI 全量刷新（棋盘 / HUD / 预览 / 覆盖层） */
    void refreshView()
    {
        uint8_t cells[kVisibleRows][kCols];
        for (uint8_t row = 0; row < kVisibleRows; ++row)
        {
            for (uint8_t col = 0; col < kCols; ++col)
            {
                cells[row][col] = g_core.cellAt(col, row + kHiddenRows);
            }
        }
        ui_TetrisScreen_draw_board(cells);

        ui_TetrisScreen_set_hud(g_core.score(), g_core.level(),
                                g_core.lines(), g_core.combo());
        ui_TetrisScreen_set_preview(
            static_cast<int8_t>(g_core.nextAt(0)),
            static_cast<int8_t>(g_core.nextAt(1)),
            static_cast<int8_t>(g_core.nextAt(2)),
            g_core.hasHold() ? static_cast<int8_t>(g_core.holdPiece()) : -1,
            g_core.holdUsed());

        using ekeys::tetris::State;
        const State st = g_core.state();
        ui_TetrisScreen_set_status(st == State::Over ? 2
                                   : st == State::Paused ? 1
                                                         : 0,
                                   nullptr);
    }
} // namespace

/* ------------------------------------------------------------------ */
/* C 链接 API                                                           */
/* ------------------------------------------------------------------ */

extern "C" void tetrisGame_enter(void)
{
    if (g_active)
    {
        return;
    }

    /*
     * 同步切换：rotation → LVGL 分辨率 → 建屏 → load → tag。
     * 中间不调用 lv_timer_handler（DisplayTask 每帧末尾才 tick），故不会花屏。
     */
    ekeys::DisplayDriver::instance().setRotation(tetris::kPortraitRotation);
    ekeys::LvglPort::instance().setOrientation(true);

    /* 竖屏游戏页懒创建（ui_init 只建两个横屏游戏页） */
    if (ui_TetrisScreen == NULL)
    {
        ui_TetrisScreen_screen_init();
    }
    lv_scr_load(ui_TetrisScreen);
    lv_refr_now(NULL);
    ui_set_active_screen_tag(UI_SCREEN_TETRIS);

    g_active = true;
    applyGameLedPalette();

    g_core.reset(millis());
    refreshView();
}

extern "C" void tetrisGame_exit(void)
{
    if (!g_active)
    {
        return;
    }
    g_active = false;

    ekeys::RGBLightControl::instance().clearGamePalette();

    /* 恢复横屏：先 setRotation + setOrientation，再 load 菜单 + refr */
    ekeys::DisplayDriver::instance().setRotation(tetris::kLandscapeRotation);
    ekeys::LvglPort::instance().setOrientation(false);
    lv_scr_load(ui_GameScreen);
    lv_refr_now(NULL);
    ui_set_active_screen_tag(UI_SCREEN_GAMES);
}

extern "C" void tetrisGame_restart(void)
{
    if (!g_active)
    {
        return;
    }
    g_core.reset(millis());
    refreshView();
}

extern "C" void tetrisGame_handleInput(uint8_t action)
{
    if (!g_active)
    {
        return;
    }

    using ekeys::tetris::State;
    const State st = g_core.state();
    const uint8_t kb = ekeys::kMatrixKeyActionBase;

    /*
     * 矩阵键功能映射（2026-11-19 调整，见 docs/14 §4.4）：
     *   K8 退出 / K4 暂停 / K1 重开 / K9 左移 / K5 旋转 / K6 硬降 / K2 右移
     *   K3 / K7 / K10 / K11 暂无功能（忽略）
     * 旋钮保持不变：逆时针左移 / 顺时针右移 / 单击旋转 / 双击暂停
     */
    const uint8_t keyExit = kb + 8;     /* K8 退出 */
    const uint8_t keyPause = kb + 4;    /* K4 暂停 */
    const uint8_t keyRestart = kb + 1;  /* K1 重开 */
    const uint8_t keyLeft = kb + 9;     /* K9 左移 */
    const uint8_t keyRotate = kb + 5;   /* K5 旋转 */
    const uint8_t keyRight = kb + 2;    /* K2 右移 */
    const uint8_t keyHardDrop = kb + 6; /* K6 硬降 */

    /* ---- GAME OVER：重开（K1/旋钮单击）/ 退出（K8/旋钮双击）---- */
    if (st == State::Over)
    {
        if (action == LV_KEY_ENTER || action == keyRestart)
        {
            tetrisGame_restart();
        }
        else if (action == LV_KEY_ESC || action == keyExit)
        {
            tetrisGame_exit();
        }
        return;
    }

    /* ---- PAUSED：继续（K4/旋钮单击）/ 重开（K1）/ 退出（K8/旋钮双击）---- */
    if (st == State::Paused)
    {
        if (action == LV_KEY_ENTER || action == keyPause)
        {
            g_core.togglePause();
            refreshView();
        }
        else if (action == keyRestart)
        {
            tetrisGame_restart();
        }
        else if (action == LV_KEY_ESC || action == keyExit)
        {
            tetrisGame_exit();
        }
        return;
    }

    /* ---- PLAYING ---- */
    bool changed = false;
    if (action == LV_KEY_ENTER || action == keyRotate)
    {
        changed = g_core.rotate(1); /* 旋钮单击 / K5 旋转 */
    }
    else if (action == LV_KEY_LEFT || action == keyLeft)
    {
        changed = g_core.move(-1); /* 旋钮逆时针 / K9 左移 */
    }
    else if (action == LV_KEY_RIGHT || action == keyRight)
    {
        changed = g_core.move(1); /* 旋钮顺时针 / K2 右移 */
    }
    else if (action == LV_KEY_ESC || action == keyPause)
    {
        g_core.togglePause(); /* 旋钮双击 / K4 暂停 */
        changed = true;
    }
    else if (action == keyHardDrop)
    {
        g_core.hardDrop(); /* K6 硬降 */
        changed = true;
    }
    else if (action == keyRestart)
    {
        tetrisGame_restart(); /* K1 重开：立刻开新局 */
        return;
    }
    else if (action == keyExit)
    {
        tetrisGame_exit(); /* K8 退出 */
        return;
    }
    /* K3 / K7 / K10 / K11 暂无功能，忽略 */

    if (changed)
    {
        refreshView();
    }
}

extern "C" void tetrisGame_tick(uint16_t dt_ms)
{
    if (!g_active || g_core.state() != ekeys::tetris::State::Playing)
    {
        return;
    }
    if (g_core.step(dt_ms))
    {
        refreshView();
    }
}

extern "C" bool tetrisGame_isActive(void)
{
    return g_active;
}
