/*
 * GameHost.h
 *
 * 前台玩法宿主（docs/13-input-router-design.md §3.7）。
 *
 * 第一版是占位实现：
 *   - 惰性创建 ui_GameScreen；
 *   - 显示最近一次按键，用于验证 InputRouter / ForegroundKeyQueue；
 *   - 不碰 HID / HA / 协议。
 *
 * 后续 Simon 等游戏只需替换 enter/onKey/tick 的内部实现。
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

        lv_obj_t *screen_{nullptr};
        bool active_{false};
    };

} // namespace ekeys

#endif // EKEYS_GAME_GAME_HOST_H
