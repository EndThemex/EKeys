/*
 * InputRouter.h
 *
 * 矩阵键路由（docs/13-input-router-design.md §3.3）。
 *
 * 职责只有两件事：
 *   1. 屏幕 → MatrixRoute（唯一事实源）；
 *   2. 按下时记录 owner，释放时按按下时的 owner 返回。
 *
 * 不执行 HID / HA / UI 动作，避免把 KeyResolver、协议层拖进入口。
 */

#ifndef EKEYS_INPUT_INPUT_ROUTER_H
#define EKEYS_INPUT_INPUT_ROUTER_H

#include "input/InputRoute.h"
#include "ui/ui_screen_tag.h"

namespace ekeys
{
    class InputRouter
    {
    public:
        void begin();
        void reset();

        /*
         * 返回 true 表示屏幕发生变化。
         * MainTask 只在进出 Foreground 时用它触发 keyboard_->releaseAll()。
         */
        bool updateScreen(ui_screen_tag_t screen);
        ui_screen_tag_t screen() const { return last_screen_; }

        /* 纯策略：当前屏幕下矩阵键由谁消费 */
        static MatrixRoute routeFor(ui_screen_tag_t screen);
        static bool isForeground(ui_screen_tag_t screen);

        /*
         * 按下：返回路由并记录 owner；
         * 释放：用按下时记录的 owner，跨屏不变（docs/13 §3.5）。
         */
        MatrixRoute onPress(uint8_t key, ui_screen_tag_t screen);
        MatrixRoute onRelease(uint8_t key, ui_screen_tag_t screen);

    private:
        InputOwner pressed_owner_[kInputKeyCount + 1]{};
        ui_screen_tag_t last_screen_{UI_SCREEN_UNKNOWN};
    };

} // namespace ekeys

#endif // EKEYS_INPUT_INPUT_ROUTER_H
