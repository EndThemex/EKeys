/*
 * InputRouter.cpp
 *
 * 见 InputRouter.h / docs/13-input-router-design.md。
 */

#include "input/InputRouter.h"

namespace ekeys
{
    void InputRouter::begin()
    {
        reset();
    }

    void InputRouter::reset()
    {
        for (uint8_t i = 0; i <= kInputKeyCount; ++i)
        {
            pressed_owner_[i] = InputOwner::None;
        }
        last_screen_ = UI_SCREEN_UNKNOWN;
    }

    bool InputRouter::updateScreen(ui_screen_tag_t screen)
    {
        if (screen == last_screen_)
        {
            return false;
        }
        last_screen_ = screen;
        return true;
    }

    MatrixRoute InputRouter::routeFor(ui_screen_tag_t screen)
    {
        switch (screen)
        {
        case UI_SCREEN_HA:
        case UI_SCREEN_HA_SECONDARY:
            /*
             * 不发 HID。ui_action 必须保留 true：
             * DisplayTask 依赖 ActionInput 触发 bumpActivity()，否则 HA 屏
             * 5s 自动回主页会在用户按 HA 键时误触发。
             */
            return {InputOwner::Ha, false, true};

        case UI_SCREEN_SETTING_SECONDARY:
        case UI_SCREEN_AUDIO_SECONDARY:
            /* 只给 LVGL：设置焦点 / 音效板触发 */
            return {InputOwner::Ui, false, true};

        case UI_SCREEN_GAME:
            /* 前台玩法：GameHost 独占，不投 ActionInput、不进 HID */
            return {InputOwner::Foreground, false, false};

        default:
            /* 现有 13 屏的 HID 主路径（含 UNKNOWN 启动首帧） */
            return {InputOwner::Keymap, true, true};
        }
    }

    bool InputRouter::isForeground(ui_screen_tag_t screen)
    {
        return routeFor(screen).owner == InputOwner::Foreground;
    }

    MatrixRoute InputRouter::onPress(uint8_t key, ui_screen_tag_t screen)
    {
        const MatrixRoute route = routeFor(screen);
        if (key >= 1 && key <= kInputKeyCount)
        {
            pressed_owner_[key] = route.owner;
        }
        return route;
    }

    MatrixRoute InputRouter::onRelease(uint8_t key, ui_screen_tag_t screen)
    {
        MatrixRoute route = routeFor(screen);
        route.fun_prescan = false;
        route.ui_action = false; /* release 不投 UI */

        if (key >= 1 && key <= kInputKeyCount)
        {
            if (pressed_owner_[key] != InputOwner::None)
            {
                route.owner = pressed_owner_[key];
            }
            pressed_owner_[key] = InputOwner::None;
        }
        return route;
    }

} // namespace ekeys
