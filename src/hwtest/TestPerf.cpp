/*
 * TestPerf.cpp
 *
 * T1 屏幕刷新测试实现。
 */

#include "TestPerf.h"

#include <Arduino_GFX_Library.h>

#include "HwTestMenu.h"
#include "HwTestUI.h"
#include "display/DisplayDriver.h"
#include "logging/LogManager.h"

namespace ekeys
{
    namespace hwtest
    {

        enum class PerfPhase : uint8_t
        {
            Idle = 0,
            FillScreen,        // 100 帧红蓝交替
            FillRectArea,      // 300 帧 1/8 区域
            DrawBitmap,        // 100 帧整屏位图
            Done,
        };

        // 1/8 屏幕区域：宽 1/4 高 1/2 ≈ 107×71
        constexpr int16_t kRectW = kScreenW / 4;
        constexpr int16_t kRectH = kScreenH / 2;

        constexpr uint16_t kFillFrameCount = 100;
        constexpr uint16_t kRectFrameCount = 300;
        constexpr uint16_t kBitmapFrameCount = 100;

        struct TestPerf : public TestBase
        {
            Arduino_GFX *gfx = nullptr;
            PerfPhase phase = PerfPhase::Idle;
            uint16_t *bitmap = nullptr;

            // 结果
            float fps_fill = 0.0f;
            float fps_rect = 0.0f;
            float fps_bitmap = 0.0f;

            const char *title() const override { return "T1 Refresh"; }

            void enter() override
            {
                gfx = DisplayDriver::instance().gfx();
                phase = PerfPhase::Idle;
                drawIdle();
                // 预分配 PSRAM 位图（避免运行时分配抖动）
                size_t bytes = (size_t)kScreenW * kScreenH * sizeof(uint16_t);
                if (bitmap == nullptr)
                {
                    bitmap = (uint16_t *)ps_malloc(bytes);
                }
                LOG_INFO("T2", "enter (bitmap %u bytes %s)",
                         (unsigned)bytes, bitmap ? "ok" : "NULL");
            }

            void loop(uint8_t key) override
            {
                if (key == ROT_ENTER)
                {
                    advance();
                }
            }

            void exit() override
            {
                LOG_INFO("T2", "exit (fps fill=%.1f rect=%.1f bmp=%.1f)",
                         fps_fill, fps_rect, fps_bitmap);
                if (bitmap != nullptr)
                {
                    free(bitmap);
                    bitmap = nullptr;
                }
                gfx = nullptr;
            }

            void advance()
            {
                if (phase == PerfPhase::Idle)
                {
                    phase = PerfPhase::FillScreen;
                    runFillScreen();
                    phase = PerfPhase::FillRectArea;
                    runFillRect();
                    phase = PerfPhase::DrawBitmap;
                    runDrawBitmap();
                    phase = PerfPhase::Done;
                    drawDone();
                }
                else if (phase == PerfPhase::Done)
                {
                    phase = PerfPhase::Idle;
                    drawIdle();
                }
            }

            void runFillScreen()
            {
                if (gfx == nullptr)
                    return;
                drawHeader("1/3 fillScreen x100");
                uint32_t t0 = millis();
                for (uint16_t i = 0; i < kFillFrameCount; ++i)
                {
                    gfx->fillScreen((i & 1) ? RGB565_RED : RGB565_BLUE);
                }
                uint32_t dt = millis() - t0;
                fps_fill = (dt > 0) ? (1000.0f * kFillFrameCount / dt) : 0.0f;
                LOG_INFO("T2", "fillScreen %u frames in %u ms -> %.1f FPS",
                         kFillFrameCount, (unsigned)dt, fps_fill);
                drawResultLine(1, "fill", fps_fill, dt, kFillFrameCount);
            }

            void runFillRect()
            {
                if (gfx == nullptr)
                    return;
                drawHeader("2/3 fillRect 1/4x1/2 x300");
                int16_t rx = (kScreenW - kRectW) / 2;
                int16_t ry = (kScreenH - kRectH) / 2;
                uint32_t t0 = millis();
                for (uint16_t i = 0; i < kRectFrameCount; ++i)
                {
                    gfx->fillRect(rx, ry, kRectW, kRectH,
                                  (i & 1) ? RGB565_GREEN : RGB565_BLACK);
                }
                uint32_t dt = millis() - t0;
                fps_rect = (dt > 0) ? (1000.0f * kRectFrameCount / dt) : 0.0f;
                LOG_INFO("T2", "fillRect %u frames in %u ms -> %.1f FPS",
                         kRectFrameCount, (unsigned)dt, fps_rect);
                drawResultLine(2, "rect", fps_rect, dt, kRectFrameCount);
            }

            void runDrawBitmap()
            {
                if (gfx == nullptr)
                    return;
                if (bitmap == nullptr)
                {
                    gfx->fillScreen(RGB565_RED);
                    gfx->setTextSize(2);
                    gfx->setTextColor(RGB565_WHITE);
                    gfx->setCursor(20, 60);
                    gfx->print("bitmap alloc fail");
                    return;
                }
                drawHeader("3/3 draw16bitRGB x100");
                // 填充位图：水平条纹
                for (int16_t y = 0; y < kScreenH; ++y)
                {
                    for (int16_t x = 0; x < kScreenW; ++x)
                    {
                        bitmap[(size_t)y * kScreenW + x] =
                            ((x / 16) ^ (y / 16)) & 1 ? RGB565_BLUE : RGB565_GREEN;
                    }
                }
                uint32_t t0 = millis();
                for (uint16_t i = 0; i < kBitmapFrameCount; ++i)
                {
                    gfx->draw16bitRGBBitmap(0, 0, bitmap, kScreenW, kScreenH);
                }
                uint32_t dt = millis() - t0;
                fps_bitmap = (dt > 0) ? (1000.0f * kBitmapFrameCount / dt) : 0.0f;
                LOG_INFO("T2", "bitmap %u frames in %u ms -> %.1f FPS",
                         kBitmapFrameCount, (unsigned)dt, fps_bitmap);
                drawResultLine(3, "bmp ", fps_bitmap, dt, kBitmapFrameCount);
            }

            void drawIdle()
            {
                if (gfx == nullptr)
                    return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T1 Refresh", "SW=run");
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_FG);
                gfx->setCursor(4, 22);
                gfx->print("Press SW to start:");
                gfx->setCursor(4, 36);
                gfx->print("  1) fillScreen x100");
                gfx->setCursor(4, 48);
                gfx->print("  2) fillRect 1/4x1/2 x300");
                gfx->setCursor(4, 60);
                gfx->print("  3) draw16bitRGB x100");
                gfx->setCursor(4, 80);
                gfx->setTextColor(COLOR_DIM);
                gfx->print("ref: 40MHz SPI full ~40 FPS");
                gfx->setCursor(4, kScreenH - 12);
                gfx->setTextColor(COLOR_FG);
                gfx->print("SW dbl = back");
            }

            void drawHeader(const char *label)
            {
                if (gfx == nullptr)
                    return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T1 Refresh", "running");
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_ACCENT);
                gfx->setCursor(4, 22);
                gfx->print(label);
            }

            void drawResultLine(uint8_t slot, const char *name,
                                float fps, uint32_t dt, uint16_t frames)
            {
                if (gfx == nullptr)
                    return;
                int16_t y = 40 + slot * 18;
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_OK);
                gfx->setCursor(8, y);
                gfx->printf("%s: %u fr / %u ms = %.1f FPS", name,
                            frames, (unsigned)dt, fps);
            }

            void drawDone()
            {
                if (gfx == nullptr)
                    return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T1 Refresh", "done");
                gfx->setTextSize(2);
                gfx->setTextColor(COLOR_OK);
                gfx->setCursor(8, 20);
                gfx->printf("%.1f FPS", fps_fill);
                gfx->setCursor(8, 44);
                gfx->printf("%.1f FPS", fps_rect);
                gfx->setCursor(8, 68);
                gfx->printf("%.1f FPS", fps_bitmap);
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_DIM);
                gfx->setCursor(8, 100);
                gfx->print("fill / rect / bmp");
                gfx->setCursor(8, 116);
                gfx->print("SW again -> rerun");
                gfx->setCursor(8, kScreenH - 12);
                gfx->setTextColor(COLOR_FG);
                gfx->print("SW dbl = back menu");
            }
        };

        TestBase *createTestPerf()
        {
            return new TestPerf();
        }

    } // namespace hwtest
} // namespace ekeys