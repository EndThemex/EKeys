/*
 * LvglPort.h
 *
 * 把 src/main.cpp 中 lvgl_display_init / lvgl_tick_cb / my_disp_flush
 * 迁出，封装为单独模块。
 */

#ifndef EKEYS_DISPLAY_LVGL_PORT_H
#define EKEYS_DISPLAY_LVGL_PORT_H

#include <lvgl.h>

namespace ekeys {

class LvglPort {
public:
    static LvglPort &instance();

    /*
     * 必须在 setup() 中按顺序调用：
     *
     *     DisplayDriver::instance().begin();
     *     LvglPort::instance().init();
     *     ui_minimal::create();
     */
    void init();

    /*
     * 由 Arduino loop() / DisplayTask 调用。
     */
    void tick(uint32_t elapsed_ms);

private:
    LvglPort();
    LvglPort(const LvglPort &) = delete;
    LvglPort &operator=(const LvglPort &) = delete;

    lv_disp_draw_buf_t draw_buf_;
    lv_disp_drv_t      disp_drv_;
    bool               inited_;
};

}  // namespace ekeys

#endif  // EKEYS_DISPLAY_LVGL_PORT_H
