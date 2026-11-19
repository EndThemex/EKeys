/*
 * TetrisCore.cpp
 *
 * 纯逻辑实现（docs/14 §5）。形状 / 踢墙表采用 SRS 标准：
 *   形状用 4×4 盒内的 4 个格子坐标，旋转只换坐标不改盒大小；
 *   JLSTZ 与 I 各有独立踢墙表，索引 = (from_rot, dir)。
 * 棋盘 y 向下增长（与 LVGL 一致），SRS 表中的 y 已取反存入。
 */
#include "TetrisCore.h"

#include <string.h>

namespace ekeys
{
    namespace tetris
    {

        namespace
        {
            constexpr uint8_t kQueueLen = 5;
            constexpr uint8_t kPreviewCount = 3;
        } // namespace

        /* ---------- 形状（4×4 盒内坐标） ---------- */
        const Core::offsets_t Core::kCells[7][4][4] = {
            /* I */
            {{{0, 1}, {1, 1}, {2, 1}, {3, 1}},
             {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
             {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
             {{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
            /* O */
            {{{1, 0}, {2, 0}, {1, 1}, {2, 1}},
             {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
             {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
             {{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
            /* T */
            {{{1, 0}, {0, 1}, {1, 1}, {2, 1}},
             {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
             {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
             {{1, 0}, {0, 1}, {1, 1}, {1, 2}}},
            /* S */
            {{{1, 0}, {2, 0}, {0, 1}, {1, 1}},
             {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
             {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
             {{0, 0}, {0, 1}, {1, 1}, {1, 2}}},
            /* Z */
            {{{0, 0}, {1, 0}, {1, 1}, {2, 1}},
             {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
             {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
             {{1, 0}, {0, 1}, {1, 1}, {0, 2}}},
            /* J */
            {{{0, 0}, {0, 1}, {1, 1}, {2, 1}},
             {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
             {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
             {{1, 0}, {1, 1}, {0, 2}, {1, 2}}},
            /* L */
            {{{2, 0}, {0, 1}, {1, 1}, {2, 1}},
             {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
             {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
             {{0, 0}, {1, 0}, {1, 1}, {1, 2}}},
        };

        /*
         * 踢墙索引：0=0→R 1=R→0 2=R→2 3=2→R
         *           4=2→L 5=L→2 6=L→0 7=0→L
         * 表值 (dx, dy)，dy 已按 y 向下取反。
         */
        const Core::offsets_t Core::kJlstzKick[8][5] = {
            {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},   /* 0→R */
            {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},     /* R→0 */
            {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},     /* R→2 */
            {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},   /* 2→R */
            {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},      /* 2→L */
            {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},  /* L→2 */
            {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},  /* L→0 */
            {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},      /* 0→L */
        };

        const Core::offsets_t Core::kIKick[8][5] = {
            {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},    /* 0→R */
            {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},    /* R→0 */
            {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},    /* R→2 */
            {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},    /* 2→R */
            {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},    /* 2→L */
            {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},    /* L→2 */
            {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},    /* L→0 */
            {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}},    /* 0→L */
        };

        Core::Core()
        {
            reset(0);
        }

        uint32_t Core::rndNext()
        {
            /* xorshift32：无需 <random>，确定性、足够均匀 */
            rng_ ^= rng_ << 13;
            rng_ ^= rng_ >> 17;
            rng_ ^= rng_ << 5;
            return rng_;
        }

        void Core::refillBag()
        {
            for (uint8_t i = 0; i < 7; ++i)
            {
                bag_[i] = static_cast<Piece>(i);
            }
            /* Fisher-Yates */
            for (uint8_t i = 7; i > 1; --i)
            {
                const uint8_t j = static_cast<uint8_t>(rndNext() % i);
                const Piece t = bag_[i - 1];
                bag_[i - 1] = bag_[j];
                bag_[j] = t;
            }
            bag_idx_ = 0;
        }

        Piece Core::takeFromBag()
        {
            if (bag_idx_ >= 7)
            {
                refillBag();
            }
            return bag_[bag_idx_++];
        }

        void Core::fillQueue()
        {
            for (uint8_t i = 0; i < kQueueLen; ++i)
            {
                queue_[i] = takeFromBag();
            }
        }

        Piece Core::takeNext()
        {
            const Piece next = queue_[0];
            for (uint8_t i = 0; i + 1 < kQueueLen; ++i)
            {
                queue_[i] = queue_[i + 1];
            }
            queue_[kQueueLen - 1] = takeFromBag();
            return next;
        }

        void Core::reset(uint32_t seed)
        {
            memset(grid_, 0, sizeof(grid_));
            rng_ = (seed == 0) ? 0x9E3779B9u : seed;
            bag_idx_ = 7; /* 触发 refillBag */
            fillQueue();

            cur_ = Piece::T;
            rot_ = 0;
            x_ = 3;
            y_ = 0;
            ghost_y_ = 0;

            hold_ = Piece::I;
            has_hold_ = false;
            hold_used_ = false;

            score_ = 0;
            lines_ = 0;
            level_ = 1;
            combo_ = -1;

            state_ = State::Playing;
            soft_dropping_ = false;
            gravity_ms_ = 0;
            lock_ms_ = 0;
            lock_resets_ = 0;

            spawn(takeNext());
        }

        void Core::spawn(Piece p)
        {
            cur_ = p;
            rot_ = 0;
            x_ = 3;
            y_ = 0;
            gravity_ms_ = 0;
            lock_ms_ = 0;
            lock_resets_ = 0;
            hold_used_ = false;
            updateGhost();
            if (!fits(cur_, rot_, x_, y_))
            {
                gameOver();
            }
        }

        bool Core::fits(Piece p, uint8_t rot, int8_t px, int8_t py) const
        {
            const uint8_t pi = static_cast<uint8_t>(p);
            for (uint8_t i = 0; i < 4; ++i)
            {
                const int16_t cx = static_cast<int16_t>(px) + kCells[pi][rot][i].x;
                const int16_t cy = static_cast<int16_t>(py) + kCells[pi][rot][i].y;
                if (cx < 0 || cx >= kCols || cy >= kRows)
                {
                    return false;
                }
                if (cy >= 0 && grid_[cy][cx] != 0)
                {
                    return false;
                }
            }
            return true;
        }

        void Core::updateGhost()
        {
            ghost_y_ = y_;
            while (fits(cur_, rot_, x_, static_cast<int8_t>(ghost_y_ + 1)))
            {
                ++ghost_y_;
            }
        }

        bool Core::move(int8_t dx)
        {
            if (state_ != State::Playing || dx == 0)
            {
                return false;
            }
            const int8_t nx = static_cast<int8_t>(x_ + dx);
            if (!fits(cur_, rot_, nx, y_))
            {
                return false;
            }
            x_ = nx;
            if (lock_ms_ > 0 && lock_resets_ < kMaxLockResets)
            {
                lock_ms_ = 0;
                ++lock_resets_;
            }
            updateGhost();
            return true;
        }

        bool Core::rotate(int8_t dir)
        {
            if (state_ != State::Playing || dir == 0)
            {
                return false;
            }
            const uint8_t pi = static_cast<uint8_t>(cur_);
            const uint8_t new_rot = static_cast<uint8_t>((rot_ + (dir > 0 ? 1 : 3)) & 3);

            /* O 块形状对称，直接换旋转态即可 */
            if (cur_ == Piece::O)
            {
                rot_ = new_rot;
                return true;
            }

            /* (from_rot, dir) → 踢墙表索引 */
            uint8_t kick_idx;
            if (dir > 0)
            {
                kick_idx = static_cast<uint8_t>(rot_ * 2); /* 0→0 1→2 2→4 3→6 */
            }
            else
            {
                kick_idx = static_cast<uint8_t>(((rot_ + 3) & 3) * 2 + 1); /* 0→7 1→1 2→3 3→5 */
            }

            const offsets_t (*table)[5] = (cur_ == Piece::I) ? kIKick : kJlstzKick;
            for (uint8_t k = 0; k < 5; ++k)
            {
                const int8_t nx = static_cast<int8_t>(x_ + table[kick_idx][k].x);
                const int8_t ny = static_cast<int8_t>(y_ + table[kick_idx][k].y);
                if (fits(cur_, new_rot, nx, ny))
                {
                    x_ = nx;
                    y_ = ny;
                    rot_ = new_rot;
                    if (lock_ms_ > 0 && lock_resets_ < kMaxLockResets)
                    {
                        lock_ms_ = 0;
                        ++lock_resets_;
                    }
                    updateGhost();
                    return true;
                }
            }
            (void)pi;
            return false;
        }

        bool Core::moveDownOne(bool score_it)
        {
            if (!fits(cur_, rot_, x_, static_cast<int8_t>(y_ + 1)))
            {
                return false;
            }
            ++y_;
            if (score_it)
            {
                score_ += 1;
            }
            return true;
        }

        void Core::softDrop()
        {
            if (state_ != State::Playing)
            {
                return;
            }
            moveDownOne(true);
        }

        void Core::hardDrop()
        {
            if (state_ != State::Playing)
            {
                return;
            }
            const int8_t start_y = y_;
            while (moveDownOne(false))
            {
                /* 直落到不能落为止 */
            }
            /* ghost_y_ 是硬降前的落点，与起始 y 之差即下落格数；每格 2 分 */
            if (y_ > start_y)
            {
                score_ += static_cast<uint32_t>(y_ - start_y) * 2;
            }
            lockPiece();
        }

        void Core::hold()
        {
            if (state_ != State::Playing || hold_used_)
            {
                return;
            }
            const Piece current = cur_;
            const Piece upcoming = takeNext();

            if (!has_hold_)
            {
                has_hold_ = true;
                hold_ = current;
                spawn(upcoming);
            }
            else
            {
                const Piece swap = hold_;
                hold_ = current;
                spawn(swap);
                /* upcoming 未使用（用 hold 块替换了当前块），但它已被 takeNext
                 * 从队列取出——需放回队首，否则会丢失一个预览块。 */
                for (uint8_t i = kQueueLen - 1; i > 0; --i)
                {
                    queue_[i] = queue_[i - 1];
                }
                queue_[0] = upcoming;
            }
            hold_used_ = true;
        }

        void Core::togglePause()
        {
            if (state_ == State::Playing)
            {
                state_ = State::Paused;
            }
            else if (state_ == State::Paused)
            {
                state_ = State::Playing;
            }
        }

        bool Core::step(uint16_t dt_ms)
        {
            if (state_ != State::Playing)
            {
                return false;
            }
            bool changed = false;
            const uint16_t interval = soft_dropping_ ? kSoftDropMs
                                                     : kGravityMs[(level_ > kMaxLevel ? kMaxLevel : level_) - 1];

            gravity_ms_ += dt_ms;
            while (gravity_ms_ >= interval)
            {
                gravity_ms_ = static_cast<uint16_t>(gravity_ms_ - interval);
                if (!moveDownOne(false))
                {
                    gravity_ms_ = 0;
                    break;
                }
                changed = true;
            }

            /* 触底后启动锁定延迟；恢复悬空则清零 */
            if (!fits(cur_, rot_, x_, static_cast<int8_t>(y_ + 1)))
            {
                lock_ms_ = static_cast<uint16_t>(lock_ms_ + dt_ms);
                if (lock_ms_ >= kLockDelayMs)
                {
                    lockPiece();
                    changed = true;
                }
            }
            else
            {
                lock_ms_ = 0;
            }
            return changed;
        }

        void Core::lockPiece()
        {
            const uint8_t pi = static_cast<uint8_t>(cur_);
            bool out_of_top = false;
            for (uint8_t i = 0; i < 4; ++i)
            {
                const int16_t cx = static_cast<int16_t>(x_) + kCells[pi][rot_][i].x;
                const int16_t cy = static_cast<int16_t>(y_) + kCells[pi][rot_][i].y;
                if (cy < 0)
                {
                    out_of_top = true;
                    continue;
                }
                if (cx >= 0 && cx < kCols && cy < kRows)
                {
                    grid_[cy][cx] = static_cast<uint8_t>(pi + 1);
                }
            }
            clearLines();

            if (out_of_top)
            {
                gameOver();
                return;
            }

            spawn(takeNext());
        }

        void Core::clearLines()
        {
            uint8_t cleared = 0;
            for (int16_t row = static_cast<int16_t>(kRows) - 1; row >= 0; --row)
            {
                bool full = true;
                for (uint8_t col = 0; col < kCols; ++col)
                {
                    if (grid_[row][col] == 0)
                    {
                        full = false;
                        break;
                    }
                }
                if (!full)
                {
                    continue;
                }
                ++cleared;
                /* 上方所有行下移一行 */
                for (int16_t r = row; r > 0; --r)
                {
                    memcpy(grid_[r], grid_[r - 1], kCols);
                }
                memset(grid_[0], 0, kCols);
                ++row; /* 同一行号再检查一次（下移进来的行） */
            }

            if (cleared == 0)
            {
                combo_ = -1;
                return;
            }

            const uint32_t base = (cleared <= 4) ? kLineScore[cleared] : kLineScore[4];
            ++combo_; /* -1 → 0（首次消行） */
            const uint32_t combo_bonus = (combo_ > 0)
                                             ? static_cast<uint32_t>(50) * static_cast<uint32_t>(combo_)
                                             : 0;
            score_ += (base + combo_bonus) * level_;
            lines_ += cleared;
            const uint8_t new_level = static_cast<uint8_t>(lines_ / 10 + 1);
            level_ = (new_level > kMaxLevel) ? kMaxLevel : new_level;
        }

        void Core::gameOver()
        {
            state_ = State::Over;
        }

        uint8_t Core::cellAt(uint8_t x, uint8_t y) const
        {
            if (x >= kCols || y >= kRows)
            {
                return 0;
            }
            if (grid_[y][x] != 0)
            {
                return grid_[y][x];
            }
            const uint8_t pi = static_cast<uint8_t>(cur_);
            if (state_ != State::Over)
            {
                /* 当前块优先 */
                for (uint8_t i = 0; i < 4; ++i)
                {
                    const int16_t cx = static_cast<int16_t>(x_) + kCells[pi][rot_][i].x;
                    const int16_t cy = static_cast<int16_t>(y_) + kCells[pi][rot_][i].y;
                    if (cx == x && cy == y)
                    {
                        return static_cast<uint8_t>(pi + 1);
                    }
                }
                /* 幽灵块（不与当前块重叠时显示） */
                for (uint8_t i = 0; i < 4; ++i)
                {
                    const int16_t cx = static_cast<int16_t>(x_) + kCells[pi][rot_][i].x;
                    const int16_t cy = static_cast<int16_t>(ghost_y_) + kCells[pi][rot_][i].y;
                    if (cx == x && cy == y)
                    {
                        /* 若该位置也是当前块格子，上面已返回，这里自然是幽灵 */
                        return 8;
                    }
                }
            }
            return 0;
        }

        Piece Core::nextAt(uint8_t i) const
        {
            if (i >= kPreviewCount)
            {
                return Piece::I;
            }
            return queue_[i < kQueueLen ? i : kQueueLen - 1];
        }

    } // namespace tetris
} // namespace ekeys
