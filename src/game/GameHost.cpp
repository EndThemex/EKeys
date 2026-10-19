/*
 * GameHost.cpp
 *
 * 见 GameHost.h。第一版占位：只显示最近按键。
 */

#include "game/GameHost.h"

#include <stdio.h>

#include "logging/LogManager.h"
#include "input/ForegroundKeyQueue.h"
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
        ui_GameScreen_set_text("GAME\nPress any key");
        LOG_INFO("GAME", "enter");
    }

    void GameHost::exit()
    {
        if (!active_)
        {
            return;
        }
        active_ = false;
        LOG_INFO("GAME", "exit");
    }

    void GameHost::onKey(uint8_t key, bool pressed, uint32_t timestamp_ms)
    {
        if (!active_)
        {
            return;
        }

        char text[48];
        snprintf(text, sizeof(text), "KEY %u %s\n%lu ms",
                 static_cast<unsigned>(key),
                 pressed ? "DOWN" : "UP",
                 static_cast<unsigned long>(timestamp_ms));
        ui_GameScreen_set_text(text);
    }

    void GameHost::tick(uint32_t dt_ms)
    {
        (void)dt_ms;
    }

} // namespace ekeys
