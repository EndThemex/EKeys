/*
 * TestRotary.cpp
 *
 * T3 旋钮测试实现。
 *
 * ROT_LEFT / ROT_RIGHT 在主循环的 g_lastKey 中累积（每次 rotary callback
 * 会写一次 g_lastKey）。本测试项每次 loop() 只消费一次。
 */

#include "TestRotary.h"

#include <Arduino_GFX_Library.h>

#include "HwTestMenu.h"
#include "HwTestUI.h"
#include "display/DisplayDriver.h"
#include "logging/LogManager.h"

namespace ekeys
{
    namespace hwtest
    {

        struct TestRotary : public TestBase
        {
            Arduino_GFX *gfx = nullptr;
            int32_t counter = 0;
            int last_dir = 0; // -1/0/+1
            uint16_t click_count = 0;
            uint16_t dblclick_count = 0;
            uint32_t last_redraw_ms = 0;

            const char *title() const override { return "T3 Rotary"; }

            void enter() override
            {
                gfx = DisplayDriver::instance().gfx();
                counter = 0;
                click_count = 0;
                dblclick_count = 0;
                LOG_INFO("T5", "enter");
                drawStatic();
                drawValue();
            }

            void loop(uint8_t key) override
            {
                if (key == ROT_LEFT)
                {
                    counter--;
                    last_dir = -1;
                }
                else if (key == ROT_RIGHT)
                {
                    counter++;
                    last_dir = +1;
                }
                else if (key == ROT_ENTER)
                {
                    click_count++;
                    LOG_INFO("T5", "click count=%u", click_count);
                }
                else if (key == ROT_ESC)
                {
                    dblclick_count++;
                    LOG_INFO("T5", "dblclick count=%u", dblclick_count);
                }

                uint32_t now = millis();
                if (now - last_redraw_ms > 50)
                {
                    last_redraw_ms = now;
                    drawValue();
                }
            }

            void exit() override
            {
                LOG_INFO("T5", "exit (counter=%ld click=%u dbl=%u)",
                         (long)counter, click_count, dblclick_count);
                gfx = nullptr;
            }

            void drawStatic()
            {
                if (gfx == nullptr)
                    return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T3 Rotary", "rotate/SW");
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_DIM);
                gfx->setCursor(8, 20);
                gfx->print("Counter:");
                gfx->setCursor(8, 92);
                gfx->print("Click:");
                gfx->setCursor(8, 106);
                gfx->print("DblClick:");
                gfx->setCursor(8, kScreenH - 12);
                gfx->setTextColor(COLOR_DIM);
                gfx->print("SW dbl = back menu");
            }

            void drawValue()
            {
                if (gfx == nullptr)
                    return;
                // 中央大字
                gfx->setTextSize(4);
                gfx->setTextColor(COLOR_OK);
                gfx->setCursor(120, 22);
                gfx->printf("%+5ld", (long)counter);
                // 方向箭头
                gfx->setTextSize(2);
                if (last_dir > 0)
                {
                    gfx->setTextColor(COLOR_OK);
                    gfx->setCursor(380, 32);
                    gfx->print(">>");
                }
                else if (last_dir < 0)
                {
                    gfx->setTextColor(COLOR_WARN);
                    gfx->setCursor(8, 32);
                    gfx->print("<<");
                }
                else
                {
                    gfx->setTextColor(COLOR_DIM);
                    gfx->setCursor(8, 32);
                    gfx->print("--");
                }
                // 计数
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_FG);
                gfx->setCursor(80, 92);
                gfx->printf("%u", click_count);
                gfx->setCursor(80, 106);
                gfx->printf("%u", dblclick_count);
            }
        };

        TestBase *createTestRotary()
        {
            return new TestRotary();
        }

    } // namespace hwtest
} // namespace ekeys