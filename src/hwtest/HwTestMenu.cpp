/*
 * HwTestMenu.cpp
 *
 * 见 HwTestMenu.h。
 */

#include "HwTestMenu.h"

#include "HwTestUI.h"
#include "logging/LogManager.h"

namespace ekeys
{
    namespace hwtest
    {

        // 前向声明（测试项实现见各自文件）
        class TestBase;
        TestBase *createTestPerf();
        TestBase *createTestKeys();
        TestBase *createTestRotary();
        TestBase *createTestMic();
        TestBase *createTestSpeaker();
        void destroyTest(TestBase *t);

        struct TestDescriptor
        {
            const char *label;
            TestBase *(*factory)();
        };

        static const TestDescriptor kTests[kTestItemCount] = {
            {"T1 Refresh", createTestPerf},
            {"T2 Keys", createTestKeys},
            {"T3 Rotary", createTestRotary},
            {"T4 Mic", createTestMic},
            {"T5 Speaker", createTestSpeaker},
        };

        // 释放测试项实例（原在 TestDisplay.cpp，该测试已移除，迁移至此）
        void destroyTest(TestBase *t)
        {
            delete t;
        }

        HwTestMenu &HwTestMenu::instance()
        {
            static HwTestMenu inst;
            return inst;
        }

        void HwTestMenu::begin(Arduino_GFX *gfx)
{
    gfx_ = gfx;
    state_ = MenuState::Boot;
    cursor_ = 0;
    bootEnteredMs_ = millis();
    lastTickMs_ = millis();
    drawBootScreen();
    LOG_INFO("MENU", "begin: state=Boot");
}

        void HwTestMenu::drawBootScreen()
        {
            if (gfx_ == nullptr)
            {
                return;
            }
            gfx_->fillScreen(COLOR_BG);
            drawTitle(gfx_, "HWTEST", "boot");
            drawText(gfx_, 4, 20, "EKeys Hardware Test", COLOR_ACCENT, 2);
            drawText(gfx_, 4, 44, "matrix/rotary/mic/speaker", COLOR_DIM, 1);
            drawText(gfx_, 4, 64, "Turn rotary to enter menu", COLOR_FG, 1);
            drawText(gfx_, 4, 80, "SW single = enter", COLOR_DIM, 1);
            drawText(gfx_, 4, 94, "SW double = back", COLOR_DIM, 1);
            drawFmt(gfx_, 4, 116, COLOR_OK, 1, "Uptime: %lu ms", (unsigned long)millis());
        }

        void HwTestMenu::drawMenu()
        {
            if (gfx_ == nullptr)
            {
                return;
            }
            gfx_->fillScreen(COLOR_BG);
            drawTitle(gfx_, "HWTEST - MENU", "SW=ok");
            int16_t lineH = 14;
            int16_t y0 = 18;
            for (uint8_t i = 0; i < kTestItemCount; ++i)
            {
                int16_t y = y0 + i * lineH;
                uint16_t bg = (i == cursor_) ? COLOR_ACCENT : COLOR_BG;
                uint16_t fg = (i == cursor_) ? COLOR_BG : COLOR_FG;
                if (i == cursor_)
                {
                    gfx_->fillRect(0, y - 2, kScreenW, lineH - 2, COLOR_ACCENT);
                }
                gfx_->setTextSize(1);
                gfx_->setTextColor(fg);
                gfx_->setCursor(8, y);
                gfx_->print("> ");
                gfx_->print(kTests[i].label);
            }
            gfx_->setTextSize(1);
            gfx_->setTextColor(COLOR_DIM);
            gfx_->setCursor(4, kScreenH - 12);
            gfx_->print("SW dbl = back to menu");
        }

        void HwTestMenu::drawBootToMenuHint()
        {
            // 不重绘整屏，只在屏幕底部提示一下（保持 1.5s 后跳菜单）
        }

        void HwTestMenu::tick(uint8_t key)
        {
            uint32_t now = millis();

            switch (state_)
            {
                case MenuState::Boot:
                    // 任何旋钮/按键都提前结束自检屏
                    if (key != KEY_NONE || (now - bootEnteredMs_) > 1500)
                    {
                        state_ = MenuState::Menu;
                        drawMenu();
                        LOG_INFO("MENU", "Boot -> Menu");
                    }
                    break;

                case MenuState::Menu:
                    if (key == KEY_NONE)
                        break; // 无事件，菜单保持
                    if (key == ROT_LEFT)
                    {
                        if (cursor_ == 0)
                            cursor_ = kTestItemCount - 1;
                        else
                            --cursor_;
                        drawMenu();
                    }
                    else if (key == ROT_RIGHT)
                    {
                        cursor_ = (cursor_ + 1) % kTestItemCount;
                        drawMenu();
                    }
                    else if (key == ROT_ENTER)
                    {
                        if (active_ != nullptr)
                            destroyTest(active_);
                        active_ = kTests[cursor_].factory();
                        if (active_ != nullptr)
                        {
                            state_ = MenuState::Test;
                            active_->enter();
                            LOG_INFO("MENU", "enter %s", active_->title());
                        }
                    }
                    else if (key == ROT_ESC)
                    {
                        // Menu 状态下双击：保持在菜单，但屏幕闪一下提示
                        LOG_INFO("MENU", "ESC in Menu (no-op)");
                    }
                    break;

                case MenuState::Test:
                    if (active_ != nullptr)
                    {
                        // ROT_ESC 由测试项自己处理（通过 requestBack 回调）
                        if (key == ROT_ESC)
                        {
                            active_->exit();
                            destroyTest(active_);
                            active_ = nullptr;
                            state_ = MenuState::Menu;
                            drawMenu();
                            return;
                        }
                        active_->loop(key);
                    }
                    break;
            }

            lastTickMs_ = now;
        }

    } // namespace hwtest
} // namespace ekeys