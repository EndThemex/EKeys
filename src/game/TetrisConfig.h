/*
 * TetrisConfig.h
 *
 * 俄罗斯方块集中配置（docs/14 §8.1）：
 *   - 棋盘 / 格子尺寸、重力表、计分表
 *   - 11 色 LED / 屏幕键位图映射（K1~K11）
 *   - 竖屏 rotation 常量（当前 0，见 docs/14 §6.1）
 *
 * 纯常量头，无 Arduino / LVGL 依赖，C++ 与 C 均可 include。
 */
#ifndef EKEYS_GAME_TETRIS_CONFIG_H
#define EKEYS_GAME_TETRIS_CONFIG_H

#include <stdint.h>

#include "TetrisPalette.h"

namespace ekeys
{
    namespace tetris
    {

        /* ---------- 棋盘 ---------- */
        constexpr uint8_t kCols = 10;
        constexpr uint8_t kVisibleRows = 20;
        constexpr uint8_t kHiddenRows = 2;               /* 顶部生成缓冲 */
        constexpr uint8_t kRows = kVisibleRows + kHiddenRows; /* 22 */
        constexpr uint8_t kCellPx = 12;                   /* UI 每格像素 */

        /* ---------- 重力 / 计分 ---------- */
        constexpr uint8_t kMaxLevel = 15;
        /* 等级 1..15 的下落间隔 ms（数组下标 = level-1） */
        constexpr uint16_t kGravityMs[kMaxLevel] = {
            800, 720, 630, 550, 470, 380, 300, 220,
            160, 120, 90, 70, 55, 45, 35};
        constexpr uint16_t kSoftDropMs = 40; /* 长按 K4 的下降间隔 */
        constexpr uint16_t kLockDelayMs = 500;
        constexpr uint8_t kMaxLockResets = 15;
        constexpr uint16_t kLineScore[5] = {0, 100, 300, 500, 800}; /* ×level */

        /* ---------- 竖屏 ---------- */
        /* 2026-11-19 用户要求竖屏方向再翻转一次：由 2 改回 0（与 r=2 相差 180°） */
        constexpr uint8_t kPortraitRotation = 0;
        constexpr uint16_t kPortraitWidth = 142;
        constexpr uint16_t kPortraitHeight = 428;
        /* 横屏（现有全局默认） */
        constexpr uint8_t kLandscapeRotation = 1;
        constexpr uint16_t kLandscapeWidth = 428;
        constexpr uint16_t kLandscapeHeight = 142;

        /* ---------- 颜色 ---------- */
        struct Rgb
        {
            uint8_t r, g, b;
        };

        /* 方块调色板：索引 1=I 2=O 3=T 4=S 5=Z 6=J 7=L，8=幽灵暗色 */
        constexpr Rgb kPieceColors[9] = {
            {TETRIS_PIECE_RGB_0}, {TETRIS_PIECE_RGB_1},
            {TETRIS_PIECE_RGB_2}, {TETRIS_PIECE_RGB_3},
            {TETRIS_PIECE_RGB_4}, {TETRIS_PIECE_RGB_5},
            {TETRIS_PIECE_RGB_6}, {TETRIS_PIECE_RGB_7},
            {TETRIS_PIECE_RGB_8}};

        /*
         * 11 个物理键 LED / 屏幕键位图颜色（LED 下标 = key_id - 1 = 0..10）。
         * 颜色按物理键固定，与功能无关；功能映射见 TetrisGame.cpp::tetrisGame_handleInput
         * 与 ui_TetrisScreen.c::s_action_text（2026-11-19 调整）。
         */
        constexpr Rgb kActionColors[11] = {
            {TETRIS_ACTION_RGB_0},  /* K1  红     → 重开 */
            {TETRIS_ACTION_RGB_1},  /* K2  橙     → 右移 */
            {TETRIS_ACTION_RGB_2},  /* K3  黄     → 暂无功能 */
            {TETRIS_ACTION_RGB_3},  /* K4  绿     → 暂停 */
            {TETRIS_ACTION_RGB_4},  /* K5  青     → 旋转 */
            {TETRIS_ACTION_RGB_5},  /* K6  蓝     → 硬降 */
            {TETRIS_ACTION_RGB_6},  /* K7  紫     → 暂无功能 */
            {TETRIS_ACTION_RGB_7},  /* K8  品红   → 退出 */
            {TETRIS_ACTION_RGB_8},  /* K9  薄荷绿 → 左移 */
            {TETRIS_ACTION_RGB_9},  /* K10 琥珀   → 暂无功能 */
            {TETRIS_ACTION_RGB_10}  /* K11 青     → 暂无功能 */
        };
        constexpr uint8_t kActionLedCount = 11;

        /* RGB565 转换（LV_COLOR_16_SWAP=0，与 lv_color_make 同序） */
        inline uint16_t toRgb565(const Rgb &c)
        {
            return static_cast<uint16_t>(((c.r & 0xF8) << 8) |
                                         ((c.g & 0xFC) << 3) |
                                         (c.b >> 3));
        }

    } // namespace tetris
} // namespace ekeys

#endif // EKEYS_GAME_TETRIS_CONFIG_H
