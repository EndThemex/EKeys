/*
 * ClickHighlight.h
 *
 * 按键点击高亮（FEATURE_DOC §9 RGB_CLICK_MODE，阶段 06 任务 6.16）。
 *
 *   - CLICK_NONE_COLOR_MODE  不响应
 *   - CLICK_SINGLE_COLOR_MODE 按下点亮对应 LED、抬起熄灭
 *   - CLICK_WARE_COLOR_MODE   保留（行为同 SINGLE，含相邻 LED 微亮）
 */

#ifndef EKEYS_RGB_CLICK_HIGHLIGHT_H
#define EKEYS_RGB_CLICK_HIGHLIGHT_H

#include <stdint.h>

#include "config/DeviceSettings.h"

namespace ekeys {

class ClickHighlight {
public:
    ClickHighlight() = delete;

    /* 设置变更时同步模式 */
    static void applySettings(const DeviceSettings &snap);

    /*
     * 按键边沿（键 ID 1~11）。由 KeyEventDispatcher 调用。
     * 内部写入 RGBLightControl 的 highlight override。
     */
    static void onKeyEdge(uint8_t key_id, bool pressed);

private:
    enum ClickMode : uint8_t {
        CLICK_NONE_COLOR_MODE = 0,
        CLICK_SINGLE_COLOR_MODE = 1,
        CLICK_WARE_COLOR_MODE = 2,
    };

    static ClickMode mode_;
};

}  // namespace ekeys

#endif  // EKEYS_RGB_CLICK_HIGHLIGHT_H
