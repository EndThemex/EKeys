/*
 * HwTestMenu.h
 *
 * 测试程序菜单框架：7 个测试项 + 1 个"自检完成"开机屏。
 *
 *   - 旋钮：LEFT / RIGHT 移动高亮；ENTER 进入当前测试项；ESC（双击）返回菜单
 *   - 状态机：Boot → Menu → TestN → Menu
 *   - 每个 TestN 自带 loop() 由 menu 周期性调用（10ms 节流）
 *   - 退出时 TestN::exit() 释放硬件（Mic.end / i2s_driver_uninstall 等）
 *
 * 测试项实现 TestBase 接口：
 *     enter()  - 进入（初始化硬件、画首屏）
 *     loop()   - 循环刷新（自带按键消费）
 *     exit()   - 退出（释放硬件）
 *     title()  - 菜单项标题
 */

#ifndef EKEYS_HWTEST_HW_TEST_MENU_H
#define EKEYS_HWTEST_HW_TEST_MENU_H

#include <Arduino_GFX_Library.h>
#include <stdint.h>

namespace ekeys
{
    namespace hwtest
    {

        constexpr uint8_t kTestItemCount = 5;

        // 旋钮回调 key 值（与 lvgl 8.3.11 LV_KEY_* 一致）
        constexpr uint8_t ROT_LEFT = 0x14;   // LV_KEY_LEFT  = 20
        constexpr uint8_t ROT_RIGHT = 0x13;  // LV_KEY_RIGHT = 19
        constexpr uint8_t ROT_ENTER = 0x0A;  // LV_KEY_ENTER = 10
        constexpr uint8_t ROT_ESC = 0x1B;    // LV_KEY_ESC   = 27
        // KEY_NONE 表示本轮无按键事件；tick() 仍会调用，测试项据此持续刷新
        constexpr uint8_t KEY_NONE = 0xFF;

        enum class MenuState : uint8_t
        {
            Boot = 0,
            Menu,
            Test,
        };

        class TestBase
        {
        public:
            virtual ~TestBase() = default;
            virtual const char *title() const = 0;
            virtual void enter() = 0;
            virtual void loop(uint8_t key) = 0; // SW 单/双击等通过这里通知；ROT_* 已在 menu 层
            virtual void exit() = 0;
        };

        class HwTestMenu
        {
        public:
            static HwTestMenu &instance();

            void begin(Arduino_GFX *gfx);

            // 主循环：在 main_hwtest.cpp 的 loop() 中调用
            void tick(uint8_t key);

            MenuState state() const { return state_; }
            uint8_t cursor() const { return cursor_; }

            // 由测试项在 SW 双击时调用，退出回菜单
            void requestBack() { state_ = MenuState::Menu; }

        private:
            HwTestMenu() = default;

            void drawBootScreen();
            void drawMenu();
            void drawBootToMenuHint();

            Arduino_GFX *gfx_ = nullptr;
            MenuState state_ = MenuState::Boot;

            uint8_t cursor_ = 0;          // 当前菜单高亮 0..kTestItemCount-1
            uint32_t bootEnteredMs_ = 0;
            uint32_t lastTickMs_ = 0;
            TestBase *active_ = nullptr;  // 当前活跃测试项
        };

    } // namespace hwtest
} // namespace ekeys

#endif // EKEYS_HWTEST_HW_TEST_MENU_H