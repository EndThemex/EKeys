/*
 * GameHost.h
 *
 * 前台玩法宿主（docs/13-input-router-design.md §3.7）。
 *
 * 当前承载 Simon Says（docs/13 Step 4）：
 *   - 11 键当游戏垫，回放序列 → 玩家跟按 → 逐轮加长；
 *   - 只读 ForegroundKeyQueue，不碰 HID / HA / 协议；
 *   - 计时全部在 tick(dt) 内推进，无阻塞等待；
 *   - 音效走 SoundService（后续阶段），当前仅屏显 + 指示条。
 *
 * 后续新游戏可在 GameHost 内加子状态机，或拆 GameBase。
 */

#ifndef EKEYS_GAME_GAME_HOST_H
#define EKEYS_GAME_GAME_HOST_H

#include <stdint.h>

#include "lvgl.h"
#include "ui/ui_screen_tag.h"

namespace ekeys
{
    class GameHost
    {
    public:
        static GameHost &instance();

        GameHost(const GameHost &) = delete;
        GameHost &operator=(const GameHost &) = delete;

        void enter(ui_screen_tag_t from);
        void exit();
        void onKey(uint8_t key, bool pressed, uint32_t timestamp_ms);
        void tick(uint32_t dt_ms);
        bool active() const { return active_; }

        /* 惰性创建并返回游戏屏；navigateNow 用它作为切屏目标 */
        lv_obj_t *screen();

    private:
        GameHost() = default;

        /* ---- Simon 状态机 ---- */
        enum class SimonState : uint8_t
        {
            Ready = 0,    /* 等待开始（任意键开局） */
            Showing,      /* 回放序列（回放中忽略按键） */
            WaitingInput, /* 玩家跟按 */
            GameOver,     /* 失败/通关，锁定一小段时间后任意键重开 */
        };

        static constexpr uint8_t kSimonMaxSeq = 32;         /* 序列上限（通关长度） */
        static constexpr uint32_t kShowOnMs = 420;          /* 回放单步点亮时长 */
        static constexpr uint32_t kShowGapMs = 160;         /* 回放步间隔 */
        static constexpr uint32_t kInputTimeoutMs = 5000;   /* 跟按总超时 */
        static constexpr uint32_t kGameOverLockoutMs = 900; /* 结算后防误触 */

        void startGame();
        void startRound();
        void beginShowStep(uint8_t idx);
        void tickShowing(uint32_t dt_ms);
        void handleInput(uint8_t key);
        void endGame(bool win);
        void refreshStatusText();

        SimonState state_{SimonState::Ready};
        uint8_t seq_[kSimonMaxSeq]{};
        uint8_t seq_len_{0};    /* 当前轮序列长度 */
        uint8_t show_idx_{0};   /* 回放步下标 */
        bool show_on_{false};   /* 回放当前步处于点亮段 */
        uint32_t phase_ms_{0};  /* 当前段已积累时长 */
        uint32_t state_ms_{0};  /* 当前状态已持续时长（超时/锁通用） */
        uint8_t input_pos_{0};  /* 玩家已正确跟按的步数 */
        uint8_t best_{0};       /* 本次开机内最好成绩（完成轮数） */
        uint8_t last_score_{0}; /* 最近一局结算成绩（endGame 时算好） */

        lv_obj_t *screen_{nullptr};
        bool active_{false};
    };

} // namespace ekeys

#endif // EKEYS_GAME_GAME_HOST_H
