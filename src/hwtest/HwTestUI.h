/*
 * HwTestUI.h
 *
 * 测试程序公共绘制助手（直接使用 Arduino_GFX API，不依赖 LVGL）。
 * 提供：清屏、标题栏、文本行、级别条、状态提示。
 *
 * 设计原则：
 *   - 屏幕方向 rotation=1 → 视窗 428×142
 *   - 字体使用 GFX 默认字体（5×7）+ setTextSize() 放大
 *   - 颜色直接 RGB565 常量
 */

#ifndef EKEYS_HWTEST_HW_TEST_UI_H
#define EKEYS_HWTEST_HW_TEST_UI_H

#include <Arduino_GFX_Library.h>
#include <stdint.h>

namespace ekeys
{
    namespace hwtest
    {

        // 屏幕视窗尺寸（rotation=1）
        constexpr int16_t kScreenW = 428;
        constexpr int16_t kScreenH = 142;

        // 颜色辅助
        constexpr uint16_t COLOR_BG = RGB565(0, 0, 0);
        constexpr uint16_t COLOR_FG = RGB565(255, 255, 255);
        constexpr uint16_t COLOR_DIM = RGB565(120, 120, 120);
        constexpr uint16_t COLOR_ACCENT = RGB565(0, 200, 255);
        constexpr uint16_t COLOR_WARN = RGB565(255, 160, 0);
        constexpr uint16_t COLOR_OK = RGB565(0, 220, 80);
        constexpr uint16_t COLOR_HIGHLIGHT = RGB565(255, 220, 0);

        // 清屏
        void clearScreen(Arduino_GFX *gfx, uint16_t bg = COLOR_BG);

        // 顶部标题栏（占 14 px 高）
        void drawTitle(Arduino_GFX *gfx, const char *title, const char *hint = nullptr);

        // 在指定位置写一行小字（size=1）
        void drawText(Arduino_GFX *gfx, int16_t x, int16_t y, const char *text,
                      uint16_t color = COLOR_FG, uint8_t size = 1);

        // 写一行大字
        void drawTextLarge(Arduino_GFX *gfx, int16_t x, int16_t y, const char *text,
                           uint16_t color = COLOR_FG, uint8_t size = 2);

        // 写格式化数字
        void drawFmt(Arduino_GFX *gfx, int16_t x, int16_t y, uint16_t color,
                     uint8_t size, const char *fmt, ...);

        // 级别条 (x,y,w,h)：level 0~1000 映射 0~w
        void drawLevelBar(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t level_milli, uint16_t color = COLOR_OK,
                          uint16_t bg = COLOR_DIM);

        // 画矩形边框
        void drawFrame(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t color);

    } // namespace hwtest
} // namespace ekeys

#endif // EKEYS_HWTEST_HW_TEST_UI_H