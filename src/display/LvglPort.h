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
     *     （11 屏 UI 由 DisplayTask::run() 内 ui_init() 创建）
     */
    void init();

    /*
     * 由 Arduino loop() / DisplayTask 调用。
     */
    void tick(uint32_t elapsed_ms);

    /*
     * 开机画面（黑底 + 橙色大字 EKeys）：init() 之后立即调用，
     * 同步渲染一帧上屏，覆盖 SPIFFS 挂载 / 配置加载 / ui_init 建屏
     * 期间的黑屏窗口。主 UI 由 DisplayTask::run() 内 ui_init() 接管后
     * 调 clearSplash() 销毁，回收 LVGL 池内存。
     */
    void showSplash();
    void clearSplash();

    /*
     * 切换 LVGL 逻辑分辨率（docs/14 竖屏俄罗斯方块）：
     *   portrait=true  → 142×428；false → 428×142（默认横屏）。
     * 绘制缓冲无需重分配（16 行 × 142 < 现有 428×16 容量）；
     * 调用方须先 DisplayDriver::setRotation()，再调本函数，最后 lv_scr_load + lv_refr_now。
     */
    void setOrientation(bool portrait);

    /* 当前是否竖屏（供 UI 布局/自检读取） */
    bool isPortrait() const { return portrait_; }

private:
    LvglPort();
    LvglPort(const LvglPort &) = delete;
    LvglPort &operator=(const LvglPort &) = delete;

    lv_disp_draw_buf_t draw_buf_;
    lv_disp_drv_t      disp_drv_;
    bool               inited_;
    bool               portrait_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_DISPLAY_LVGL_PORT_H
