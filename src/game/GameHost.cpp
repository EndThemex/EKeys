/*
 * GameHost.cpp
 *
 * 见 GameHost.h。当前实现 Simon Says（docs/13 Step 4）。
 */

#include "game/GameHost.h"

#include <stdio.h>

#include "logging/LogManager.h" /* 已带入 Arduino.h（random 底层走 esp_random 硬件 RNG） */
#include "input/ForegroundKeyQueue.h"
#include "input/InputRoute.h"
#include "ui/game/ui_GameScreen.h"

namespace ekeys
{
    GameHost &GameHost::instance()
    {
        static GameHost inst;
        return inst;
    }

    lv_obj_t *GameHost::screen()
    {
        if (screen_ == nullptr)
        {
            screen_ = ui_GameScreen_create();
        }
        return screen_;
    }

    void GameHost::enter(ui_screen_tag_t from)
    {
        (void)from;
        /* 清掉上次退出后残留的 release，避免新一轮游戏收到陈旧边沿 */
        ForegroundKeyQueue::instance().clear();
        active_ = true;
        screen();

        state_ = SimonState::Ready;
        seq_len_ = 0;
        input_pos_ = 0;
        state_ms_ = 0;
        ui_GameScreen_set_all_cells(false);
        ui_GameScreen_set_text("SIMON\nPress any key");
        LOG_INFO("GAME", "enter");
    }

    void GameHost::exit()
    {
        if (!active_)
        {
            return;
        }
        active_ = false;
        ui_GameScreen_set_all_cells(false);
        LOG_INFO("GAME", "exit, best=%u", static_cast<unsigned>(best_));
    }

    void GameHost::onKey(uint8_t key, bool pressed, uint32_t timestamp_ms)
    {
        (void)timestamp_ms;
        if (!active_ || !pressed)
        {
            return; /* 只消费按下边沿，release 忽略 */
        }
        if (key < 1 || key > kInputKeyCount)
        {
            return;
        }

        switch (state_)
        {
        case SimonState::Ready:
            startGame();
            break;
        case SimonState::WaitingInput:
            handleInput(key);
            break;
        case SimonState::GameOver:
            /* 结算后短暂锁定，防止手滑立刻误开新局 */
            if (state_ms_ >= kGameOverLockoutMs)
            {
                startGame();
            }
            break;
        case SimonState::Showing:
            break; /* 回放中忽略按键 */
        }
    }

    void GameHost::startGame()
    {
        seq_len_ = 0;
        startRound();
    }

    void GameHost::startRound()
    {
        if (seq_len_ >= kSimonMaxSeq)
        {
            endGame(true);
            return;
        }

        /* Arduino random() 在 ESP32 核心底层走 esp_random 硬件 RNG；映射到 1~11 */
        seq_[seq_len_++] = static_cast<uint8_t>(random(1, kInputKeyCount + 1));

        show_idx_ = 0;
        show_on_ = false;
        phase_ms_ = 0;
        state_ = SimonState::Showing;
        ui_GameScreen_set_all_cells(false);
        refreshStatusText();
    }

    void GameHost::beginShowStep(uint8_t idx)
    {
        show_idx_ = idx;
        show_on_ = true;
        phase_ms_ = 0;
        ui_GameScreen_set_cell(seq_[idx], true);
    }

    void GameHost::tick(uint32_t dt_ms)
    {
        if (!active_)
        {
            return;
        }

        switch (state_)
        {
        case SimonState::Showing:
            tickShowing(dt_ms);
            break;
        case SimonState::WaitingInput:
            state_ms_ += dt_ms;
            if (state_ms_ >= kInputTimeoutMs)
            {
                endGame(false); /* 跟按超时判负 */
            }
            break;
        case SimonState::GameOver:
            state_ms_ += dt_ms;
            break;
        case SimonState::Ready:
            break;
        }
    }

    void GameHost::tickShowing(uint32_t dt_ms)
    {
        phase_ms_ += dt_ms;
        if (show_on_)
        {
            if (phase_ms_ >= kShowOnMs)
            {
                ui_GameScreen_set_cell(seq_[show_idx_], false);
                show_on_ = false;
                phase_ms_ = 0;
            }
        }
        else if (phase_ms_ >= kShowGapMs)
        {
            if (show_idx_ + 1 >= seq_len_)
            {
                /* 回放结束 → 交给玩家 */
                state_ = SimonState::WaitingInput;
                state_ms_ = 0;
                input_pos_ = 0;
                refreshStatusText();
            }
            else
            {
                beginShowStep(static_cast<uint8_t>(show_idx_ + 1));
            }
        }
    }

    void GameHost::handleInput(uint8_t key)
    {
        if (key != seq_[input_pos_])
        {
            endGame(false);
            return;
        }

        /* 正确：点亮对应格，作为即时反馈，回合边界统一熄灭 */
        ui_GameScreen_set_cell(key, true);
        ++input_pos_;
        if (input_pos_ >= seq_len_)
        {
            /* 完成一轮：成绩 = 已完成轮数（含本轮） */
            if (seq_len_ > best_)
            {
                best_ = seq_len_;
            }
            startRound();
        }
        else
        {
            refreshStatusText();
        }
    }

    void GameHost::endGame(bool win)
    {
        /* 成绩 = 已完成轮数：失败时当前轮未完成不计，通关时全部计入 */
        last_score_ = win ? seq_len_ : static_cast<uint8_t>(seq_len_ > 0 ? seq_len_ - 1 : 0);
        if (last_score_ > best_)
        {
            best_ = last_score_;
        }
        ui_GameScreen_set_all_cells(false);
        state_ = SimonState::GameOver;
        state_ms_ = 0;
        refreshStatusText();
        LOG_INFO("GAME", "%s, score=%u best=%u", win ? "WIN" : "FAIL",
                 static_cast<unsigned>(last_score_),
                 static_cast<unsigned>(best_));
    }

    void GameHost::refreshStatusText()
    {
        char text[64];
        switch (state_)
        {
        case SimonState::Showing:
            snprintf(text, sizeof(text), "ROUND %u\nWATCH",
                     static_cast<unsigned>(seq_len_));
            break;
        case SimonState::WaitingInput:
            snprintf(text, sizeof(text), "ROUND %u\nREPEAT %u/%u",
                     static_cast<unsigned>(seq_len_),
                     static_cast<unsigned>(input_pos_),
                     static_cast<unsigned>(seq_len_));
            break;
        case SimonState::GameOver:
            snprintf(text, sizeof(text), "SCORE %u  BEST %u\nPress any key",
                     static_cast<unsigned>(last_score_),
                     static_cast<unsigned>(best_));
            break;
        case SimonState::Ready:
        default:
            snprintf(text, sizeof(text), "SIMON\nPress any key");
            break;
        }
        ui_GameScreen_set_text(text);
    }

} // namespace ekeys
