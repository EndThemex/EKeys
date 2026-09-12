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
 * 缓冲区行数：kLvglBufferLines = 16（单缓冲约 13.7 KB，见 init 内注释）。
 */

#include "LvglPort.h"

#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

#include "DisplayDriver.h"
#include "hardware/PinMap.h"
#include "logging/LogManager.h"

namespace ekeys {

constexpr uint16_t kScreenWidth       = 428;
constexpr uint16_t kScreenHeight      = 142;
constexpr uint8_t  kLvglBufferLines   = 16;

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

    /*
     * 逐 chunk 让出：16 行缓冲把整屏刷新切成 9 个 chunk，在每个 flush 后
     * 让出一个 tick 给 IDLE0。若某次 lv_timer_handler / lv_refr_now 的渲染
     * 总耗时 ≥5s（不管慢在哪个环节），IDLE0 也能在每个 chunk 间被调度，
     * 不会触发 task_wdt abort（2026-09-12 进入音乐二级页 WDT，两轮缓冲
     * 位置修复均无效——两次崩溃栈逐帧相同，证明瓶颈与缓冲所在 RAM 无关）。
     * 单帧代价：全屏 +9 tick，局部刷新 +1 tick，可忽略。
     */
    vTaskDelay(1);
}

/*
 * 渲染耗时哨兵（刷新周期级）：LVGL 每完成一个刷新周期回调一次，
 * time_ms 为该周期纯渲染耗时（不含 flush 之后的等待），px_num 为刷新像素数。
 * 相比 tick 里的 lv_timer_handler 整体计时，能区分"渲染慢"（有本日志且
 * px_num 大）与"渲染挂死"（WDT abort 前无本日志）。
 */
void my_monitor_cb(lv_disp_drv_t *disp_drv, uint32_t time_ms, uint32_t px_num)
{
    LV_UNUSED(disp_drv);
    static uint32_t s_last_log_ms = 0;
    if (time_ms > 100 && (uint32_t)(millis() - s_last_log_ms) >= 2000) {
        s_last_log_ms = millis();
        LOG_WARNING("DISP", "refr cost %ums, %u px", (unsigned)time_ms, (unsigned)px_num);
    }
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

    /*
     * 2026-09-12 修订：绘制缓冲迁回内部 RAM（单缓冲）。
     *   - 2026-09-08 曾整体迁到 PSRAM 给 ASR TLS 让内存，但 PSRAM 上做
     *     LVGL 读改写混合 + DMA 取数极慢，整屏一帧渲染被拉长到 ≥5s，
     *     DisplayTask 单次不还 CPU 的渲染把 IDLE0 饿死 5s → task_wdt abort
     *     （进入音乐二级页必崩，vTaskDelay(1) 节律让出也救不了单次调用）。
     *   - my_disp_flush 是同步 flush（flush_ready 紧跟阻塞写），双缓冲无收益，
     *     改单缓冲再省一半内部 RAM。
     *   - 16 行 ×428 ×2B ≈ 13.7KB；ASR TLS 握手需 ~35KB 内部 RAM，仍留足余量。
     *   - 内部分配失败才退回 PSRAM / malloc（慢但可用）。
     */
    if (buf1 == nullptr) {
        const size_t buf_bytes =
            static_cast<size_t>(kScreenWidth) * kLvglBufferLines * sizeof(lv_color_t);
        buf1 = static_cast<lv_color_t *>(
            heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (buf1 == nullptr) {
            buf1 = static_cast<lv_color_t *>(ps_malloc(buf_bytes));
        }
        if (buf1 == nullptr) {
            buf1 = static_cast<lv_color_t *>(malloc(buf_bytes));
        }
        buf2 = nullptr;
    }

    lv_disp_draw_buf_init(&draw_buf_, buf1, buf2,
                          kScreenWidth * kLvglBufferLines);

    lv_disp_drv_init(&disp_drv_);
    disp_drv_.hor_res  = kScreenWidth;
    disp_drv_.ver_res  = kScreenHeight;
    disp_drv_.draw_buf = &draw_buf_;
    disp_drv_.flush_cb = my_disp_flush;
    disp_drv_.monitor_cb = my_monitor_cb;

    lv_disp_drv_register(&disp_drv_);
    inited_ = true;
}

void LvglPort::tick(uint32_t elapsed_ms)
{
    if (!inited_) {
        return;
    }
    lv_tick_inc(elapsed_ms);
    const uint32_t t0 = millis();
    lv_timer_handler();
    /* 渲染耗时哨兵：单轮 lv_timer_handler >100ms 说明渲染过重
     * （2026-09-12 WDT 崩溃排查用），2s 节流防刷屏 */
    static uint32_t s_last_warn_ms = 0;
    const uint32_t cost = millis() - t0;
    if (cost > 100 && (uint32_t)(millis() - s_last_warn_ms) >= 2000) {
        s_last_warn_ms = millis();
        LOG_WARNING("DISP", "lv_timer_handler cost %ums", (unsigned)cost);
    }
}

}  // namespace ekeys
