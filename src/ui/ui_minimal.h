/*
 * ui_minimal.h
 *
 * 阶段 01 / 02 期间使用的最小主屏。
 * 把 src/main.cpp 中原 create_ui() 整体搬到这里。
 *
 * 阶段 05 由 SquareLine Studio 生成的 ui.cpp 接管，
 * 本文件保留作为设备首屏示例（FEATURE_DOC §17 占位）。
 */

#ifndef EKEYS_UI_UI_MINIMAL_H
#define EKEYS_UI_UI_MINIMAL_H

namespace ekeys {

class ui_minimal {
public:
    /*
     * 在当前 LVGL 主屏（lv_scr_act()）上画一个 demo UI：
     *
     *   - 标题"Nike"（NV3007）
     *   - 分辨率"428 x 142"
     *   - 状态"ESP32-S3 + LVGL"
     *   - 右侧"TEST"按钮
     */
    static void create();
};

}  // namespace ekeys

#endif  // EKEYS_UI_UI_MINIMAL_H
