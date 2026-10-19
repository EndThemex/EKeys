/*
 * InputRoute.h
 *
 * 矩阵键路由类型（docs/13-input-router-design.md §3.1）。
 *
 * 纯 POD，不依赖 Arduino / LVGL，便于 host 单测。
 */

#ifndef EKEYS_INPUT_INPUT_ROUTE_H
#define EKEYS_INPUT_INPUT_ROUTE_H

#include <stdint.h>

namespace ekeys
{
    /*
     * 与 MatrixScanner::kMatrixKeyCount 对齐；
     * MainTask.cpp 用 static_assert 兜底，避免两边漂移。
     */
    inline constexpr uint8_t kInputKeyCount = 11;

    enum class InputOwner : uint8_t
    {
        None = 0,
        Keymap,     /* KeyEventDispatcher + KeyResolver → HID */
        Ui,         /* 只投 ActionInput → LVGL */
        Ha,         /* HaApiService binary_sensor */
        Foreground, /* 前台玩法（GameHost） */
    };

    struct MatrixRoute
    {
        InputOwner owner{InputOwner::Keymap};
        bool fun_prescan{true}; /* 只对 Keymap 有意义 */
        bool ui_action{true};   /* 是否投 kMatrixKeyActionBase + key_id */
    };

} // namespace ekeys

#endif // EKEYS_INPUT_INPUT_ROUTE_H
