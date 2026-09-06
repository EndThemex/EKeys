/*
 * HwTestUI.cpp
 *
 * 见 HwTestUI.h。
 */

#include "HwTestUI.h"

#include <Arduino.h>
#include <stdarg.h>

namespace ekeys
{
    namespace hwtest
    {

        void clearScreen(Arduino_GFX *gfx, uint16_t bg)
        {
            if (gfx == nullptr)
            {
                return;
            }
            gfx->fillScreen(bg);
        }

        void drawTitle(Arduino_GFX *gfx, const char *title, const char *hint)
        {
            if (gfx == nullptr)
            {
                return;
            }
            gfx->fillRect(0, 0, kScreenW, 14, COLOR_DIM);
            gfx->setTextColor(COLOR_BG);
            gfx->setTextSize(1);
            gfx->setCursor(2, 4);
            gfx->print(title);
            if (hint != nullptr)
            {
                int16_t tw = strlen(hint) * 6;
                gfx->setCursor(kScreenW - tw - 2, 4);
                gfx->print(hint);
            }
            gfx->setTextColor(COLOR_FG);
        }

        void drawText(Arduino_GFX *gfx, int16_t x, int16_t y, const char *text,
                      uint16_t color, uint8_t size)
        {
            if (gfx == nullptr || text == nullptr)
            {
                return;
            }
            gfx->setTextSize(size);
            gfx->setTextColor(color);
            gfx->setCursor(x, y);
            gfx->print(text);
            gfx->setTextSize(1);
        }

        void drawTextLarge(Arduino_GFX *gfx, int16_t x, int16_t y, const char *text,
                           uint16_t color, uint8_t size)
        {
            drawText(gfx, x, y, text, color, size);
        }

        void drawFmt(Arduino_GFX *gfx, int16_t x, int16_t y, uint16_t color,
                     uint8_t size, const char *fmt, ...)
        {
            if (gfx == nullptr || fmt == nullptr)
            {
                return;
            }
            char buf[128];
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(buf, sizeof(buf), fmt, ap);
            va_end(ap);
            gfx->setTextSize(size);
            gfx->setTextColor(color);
            gfx->setCursor(x, y);
            gfx->print(buf);
            gfx->setTextSize(1);
        }

        void drawLevelBar(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t level_milli, uint16_t color, uint16_t bg)
        {
            if (gfx == nullptr)
            {
                return;
            }
            gfx->fillRect(x, y, w, h, bg);
            if (level_milli > 1000)
            {
                level_milli = 1000;
            }
            int16_t fill = (int16_t)((int32_t)w * level_milli / 1000);
            if (fill > 0)
            {
                gfx->fillRect(x, y, fill, h, color);
            }
            gfx->drawRect(x, y, w, h, COLOR_FG);
        }

        void drawFrame(Arduino_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t color)
        {
            if (gfx == nullptr)
            {
                return;
            }
            gfx->drawRect(x, y, w, h, color);
        }

    } // namespace hwtest
} // namespace ekeys