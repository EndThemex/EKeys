/*
 * LvglPort.cpp
 *
 * 原 src/main.cpp 中 LVGL 显示驱动 + tick / flush 回调迁到这里。
 *
 * 屏幕逻辑分辨率（旋转 1 后）：
 *
 *     width  = 428
 *     height = 142
 *
 * 缓冲区行数：kLvglBufferLines = 40（约 33 KB/个）。
 */

#include "LvglPort.h"

#include <Arduino_GFX_Library.h>

#include "DisplayDriver.h"
#include "hardware/PinMap.h"

namespace ekeys {

constexpr uint16_t kScreenWidth       = 428;
constexpr uint16_t kScreenHeight      = 142;
constexpr uint8_t  kLvglBufferLines   = 40;

namespace {

/*
 * 双缓冲原为静态数组（.bss，合计 ~67KB 内部 RAM），曾导致 ASR 的 TLS 握手
 * 分配不到内部内存（MBEDTLS_ERR_SSL_ALLOC_FAILED）。改为 PSRAM 堆分配，
 * PSRAM 不可用时退回 malloc 保底。
 */
lv_color_t *buf1 = nullptr;
lv_color_t *buf2 = nullptr;

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width  = (area->x2 - area->x1 + 1);
    uint32_t height = (area->y2 - area->y1 + 1);

    Arduino_GFX *gfx = DisplayDriver::instance().gfx();
#if LV_COLOR_16_SWAP != 0
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1,
                              (uint16_t *)&color_p->full, width, height);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1,
                            (uint16_t *)&color_p->full, width, height);
#endif

    lv_disp_flush_ready(disp_drv);
}

}  // namespace

LvglPort::LvglPort()
    : inited_(false)
{
}

LvglPort &LvglPort::instance()
{
    static LvglPort inst;
    return inst;
}

void LvglPort::init()
{
    if (inited_) {
        return;
    }
    lv_init();

    /* 双缓冲迁到 PSRAM：静态数组占 ~67KB 内部 RAM，挤掉了 TLS 握手所需内存 */
    if (buf1 == nullptr) {
        const size_t buf_bytes =
            static_cast<size_t>(kScreenWidth) * kLvglBufferLines * sizeof(lv_color_t);
        buf1 = static_cast<lv_color_t *>(ps_malloc(buf_bytes));
        buf2 = static_cast<lv_color_t *>(ps_malloc(buf_bytes));
        if (buf1 == nullptr || buf2 == nullptr) {
            /* 无 PSRAM 时退回内部堆，显示功能保持可用 */
            free(buf1);
            free(buf2);
            buf1 = static_cast<lv_color_t *>(malloc(buf_bytes));
            buf2 = static_cast<lv_color_t *>(malloc(buf_bytes));
        }
    }

    lv_disp_draw_buf_init(&draw_buf_, buf1, buf2,
                          kScreenWidth * kLvglBufferLines);

    lv_disp_drv_init(&disp_drv_);
    disp_drv_.hor_res  = kScreenWidth;
    disp_drv_.ver_res  = kScreenHeight;
    disp_drv_.draw_buf = &draw_buf_;
    disp_drv_.flush_cb = my_disp_flush;

    lv_disp_drv_register(&disp_drv_);
    inited_ = true;
}

void LvglPort::tick(uint32_t elapsed_ms)
{
    if (!inited_) {
        return;
    }
    lv_tick_inc(elapsed_ms);
    lv_timer_handler();
}

}  // namespace ekeys
