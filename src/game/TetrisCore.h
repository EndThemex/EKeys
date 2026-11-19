/*
 * TetrisCore.h
 *
 * 纯 C++ 俄罗斯方块逻辑（docs/14 §5）：7-bag、SRS 旋转踢墙、幽灵块、
 * Hold、软/硬降、锁定延迟、消行计分、等级加速、暂停/结束。
 * 不依赖 Arduino / LVGL，便于独立校验；UI 只通过只读视图渲染。
 */
#ifndef EKEYS_GAME_TETRIS_CORE_H
#define EKEYS_GAME_TETRIS_CORE_H

#include <stdint.h>

#include "TetrisConfig.h"

namespace ekeys
{
    namespace tetris
    {

        enum class Piece : uint8_t
        {
            I = 0,
            O = 1,
            T = 2,
            S = 3,
            Z = 4,
            J = 5,
            L = 6,
            Count = 7
        };

        enum class State : uint8_t
        {
            Playing = 0,
            Paused = 1,
            Over = 2
        };

        class Core
        {
        public:
            Core();

            /* seed 用 millis()，0 时用固定种子便于确定性测试 */
            void reset(uint32_t seed);

            /* 重力 / 锁定推进；返回 true 表示画面需要重绘 */
            bool step(uint16_t dt_ms);

            /* 左右移动（-1 左 / +1 右），成功返回 true */
            bool move(int8_t dx);

            /* 旋转：dir=+1 顺时针 / -1 逆时针；SRS 踢墙，成功返回 true */
            bool rotate(int8_t dir);

            /* 软降一格并计 1 分（手动点按）；长按由 setSoftDropping 控制重力 */
            void softDrop();
            void setSoftDropping(bool on) { soft_dropping_ = on; }

            /* 硬降到底，每格 2 分，立即锁定 */
            void hardDrop();

            /* Hold：每块限一次，落定后恢复 */
            void hold();

            void togglePause();

            /* ---- 只读视图（UI 绘制用） ---- */
            /* 0=空 1..7=Piece+1 8=幽灵；含固定块、当前块、幽灵块 */
            uint8_t cellAt(uint8_t x, uint8_t y) const;
            Piece activePiece() const { return cur_; }
            uint8_t activeRotation() const { return rot_; }
            int8_t activeX() const { return x_; }
            int8_t activeY() const { return y_; }
            uint32_t score() const { return score_; }
            uint8_t level() const { return level_; }
            uint32_t lines() const { return lines_; }
            State state() const { return state_; }
            Piece nextAt(uint8_t i) const; /* i=0..2 */
            int16_t combo() const { return combo_; }
            bool hasHold() const { return has_hold_; }
            bool holdUsed() const { return hold_used_; }
            Piece holdPiece() const { return hold_; }

        private:
            struct offsets_t
            {
                int8_t x, y;
            };

            /* 形状与踢墙表（文件内静态常量，见 TetrisCore.cpp） */
            static const offsets_t kCells[7][4][4];
            static const offsets_t kJlstzKick[8][5];
            static const offsets_t kIKick[8][5];

            uint32_t rndNext();
            void refillBag();
            Piece takeFromBag();
            void fillQueue();
            Piece takeNext(); /* 取 queue_[0] 并前移补位，作为下一个待生成块 */
            void spawn(Piece p);
            bool fits(Piece p, uint8_t rot, int8_t px, int8_t py) const;
            bool moveDownOne(bool score_it);
            void updateGhost();
            void lockPiece();
            void clearLines();
            void gameOver();

            uint8_t grid_[kRows][kCols];

            Piece cur_;
            uint8_t rot_;
            int8_t x_, y_;
            int8_t ghost_y_;

            uint32_t rng_;
            Piece bag_[7];
            uint8_t bag_idx_;
            Piece queue_[5];

            Piece hold_;
            bool has_hold_, hold_used_;

            uint32_t score_, lines_;
            uint8_t level_;
            int16_t combo_; /* -1 = 无连击；连续消行递增，未消行行重置为 -1 */

            State state_;
            bool soft_dropping_;
            uint16_t gravity_ms_;
            uint16_t lock_ms_;
            uint8_t lock_resets_;
        };

    } // namespace tetris
} // namespace ekeys

#endif // EKEYS_GAME_TETRIS_CORE_H
