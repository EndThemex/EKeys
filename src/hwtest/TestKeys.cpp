/*
 * TestKeys.cpp
 *
 * T2 矩阵按键测试实现。
 *
 * 注：外部已经 5ms 节流调用 matrix.scan()，本测试项只在 loop 中消费本轮
 *     getPressedKeys/getReleasedKeys（每轮一次）。
 */

#include "TestKeys.h"

#include <Arduino_GFX_Library.h>

#include "HwTestGlobals.h"
#include "HwTestMenu.h"
#include "HwTestUI.h"
#include "display/DisplayDriver.h"
#include "input/MatrixScanner.h"
#include "logging/LogManager.h"
#include "rgb/RGBDriver.h"

namespace ekeys
{
    namespace hwtest
    {

        // 网格几何参数
        constexpr int16_t kGridX = 8;
        constexpr int16_t kGridY = 20;
        constexpr int16_t kCellW = 100;
        constexpr int16_t kCellH = 32;
        constexpr int16_t kGridGap = 4;

        struct TestKeys : public TestBase
        {
            Arduino_GFX *gfx = nullptr;
            MatrixScanner *scanner = nullptr;

            uint8_t pressed_buf[kMatrixKeyCount] = {0};
            uint8_t released_buf[kMatrixKeyCount] = {0};
            uint16_t press_count[kMatrixKeyCount + 1] = {0};
            bool last_state[kMatrixKeyCount + 1] = {false};
            uint32_t last_redraw_ms = 0;
            bool slow_probe = false; // 慢速扫描探针（旋钮右转切换）

            const char *title() const override { return "T2 Keys"; }

            void enter() override
            {
                gfx = DisplayDriver::instance().gfx();
                scanner = getMatrixScanner();
                memset(press_count, 0, sizeof(press_count));
                memset(last_state, 0, sizeof(last_state));
                LOG_INFO("T3", "enter");
                drawStatic();
                redrawAll();
            }

            void loop(uint8_t key) override
            {
                if (scanner == nullptr)
                    return;

                // 旋钮右转：切换慢速扫描探针（诊断模式）
                if (key == ROT_RIGHT)
                {
                    slow_probe = !slow_probe;
                    scanner->setDebugSlowScan(slow_probe);
                    LOG_INFO("T3", "slow probe %s", slow_probe ? "ON" : "OFF");
                    drawStatic();
                }

                // 消费本轮 press 事件
                uint8_t pc = 0;
                scanner->getPressedKeys(pressed_buf, pc);
                for (uint8_t i = 0; i < pc; ++i)
                {
                    uint8_t kid = pressed_buf[i];
                    press_count[kid]++;
                    LOG_INFO("T3", "press id=%u ms=%lu", kid, (unsigned long)millis());
                    // 联动 RGB：按下点白
                    RGBDriver::instance().setPixel(kid - 1, 255, 255, 255);
                    RGBDriver::instance().show();
                }

                // 消费本轮 release 事件
                uint8_t rc = 0;
                scanner->getReleasedKeys(released_buf, rc);
                for (uint8_t i = 0; i < rc; ++i)
                {
                    uint8_t kid = released_buf[i];
                    LOG_INFO("T3", "release id=%u", kid);
                    RGBDriver::instance().setPixel(kid - 1, 0, 0, 0);
                    RGBDriver::instance().show();
                }

                // 重绘稳定电平 + 计数（每 100ms 一次，避免闪烁）
                uint32_t now = millis();
                if (now - last_redraw_ms > 100)
                {
                    last_redraw_ms = now;
                    redrawAll();
                }
            }

            void exit() override
            {
                LOG_INFO("T3", "exit");
                scanner->setDebugSlowScan(false);
                RGBDriver::instance().clearAll();
                RGBDriver::instance().show();
                gfx = nullptr;
                scanner = nullptr;
            }

            void drawStatic()
            {
                if (gfx == nullptr)
                    return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T2 Keys", "press any");
                // 画 3x4 网格
                for (uint8_t r = 0; r < 3; ++r)
                {
                    for (uint8_t c = 0; c < 4; ++c)
                    {
                        int16_t x = kGridX + c * (kCellW + kGridGap);
                        int16_t y = kGridY + r * (kCellH + kGridGap);
                        gfx->drawRect(x, y, kCellW, kCellH, COLOR_DIM);
                        // ROW0 + COL3 空缺
                        if (r == 0 && c == 3)
                        {
                            gfx->setTextSize(2);
                            gfx->setTextColor(COLOR_DIM);
                            gfx->setCursor(x + kCellW / 2 - 6, y + kCellH / 2 - 8);
                            gfx->print("x");
                            continue;
                        }
                        uint8_t kid = (r == 0) ? (c + 1) : (r == 1) ? (c + 4) : (c + 8);
                        gfx->setTextSize(2);
                        gfx->setTextColor(COLOR_FG);
                        gfx->setCursor(x + 8, y + kCellH / 2 - 8);
                        gfx->printf("%u", kid);
                    }
                }
                gfx->setTextSize(1);
                gfx->setTextColor(slow_probe ? COLOR_OK : COLOR_DIM);
                gfx->setCursor(8, kScreenH - 12);
                if (slow_probe)
                {
                    gfx->print("SLOW PROBE ON - rot=off");
                }
                else
                {
                    gfx->print("rot=probe  SW dbl=back");
                }
            }

            void redrawAll()
            {
                if (gfx == nullptr || scanner == nullptr)
                    return;
                for (uint8_t r = 0; r < 3; ++r)
                {
                    for (uint8_t c = 0; c < 4; ++c)
                    {
                        if (r == 0 && c == 3)
                            continue;
                        uint8_t kid = (r == 0) ? (c + 1) : (r == 1) ? (c + 4) : (c + 8);
                        int16_t x = kGridX + c * (kCellW + kGridGap);
                        int16_t y = kGridY + r * (kCellH + kGridGap);
                        bool pressed = scanner->getStableState(kid);
                        if (pressed != last_state[kid])
                        {
                            last_state[kid] = pressed;
                        }
                        uint16_t fill = pressed ? COLOR_OK : COLOR_BG;
                        gfx->fillRect(x + 1, y + 1, kCellW - 2, kCellH - 2, fill);
                        gfx->drawRect(x, y, kCellW, kCellH,
                                      pressed ? COLOR_HIGHLIGHT : COLOR_DIM);
                        // 原始电平指示（左上角小方块）：亮 = 本轮扫描瞬时读到按下（未消抖）
                        bool raw = scanner->getRawState(kid);
                        gfx->fillRect(x + 3, y + 3, 5, 5,
                                      raw ? COLOR_HIGHLIGHT : COLOR_DIM);
                        gfx->setTextSize(2);
                        gfx->setTextColor(pressed ? COLOR_BG : COLOR_FG);
                        gfx->setCursor(x + 8, y + kCellH / 2 - 8);
                        gfx->printf("%u", kid);
                        // 计数
                        gfx->setTextSize(1);
                        gfx->setTextColor(pressed ? COLOR_BG : COLOR_DIM);
                        gfx->setCursor(x + kCellW - 18, y + 4);
                        gfx->printf("%u", press_count[kid]);
                    }
                }
            }
        };

        TestBase *createTestKeys()
        {
            return new TestKeys();
        }

    } // namespace hwtest
} // namespace ekeys