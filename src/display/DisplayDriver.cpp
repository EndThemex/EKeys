/*
 * DisplayDriver.cpp
 *
 * 把原 src/main.cpp 中 NV3007 + Arduino_ESP32SPI 创建与初始化
 * 全部封装在此，与 main.cpp 解耦。
 *
 * 屏幕配置：
 *
 *     引脚统一取自 hardware/PinMap.h（LCD_SDA=40, LCD_SCL=41,
 *     LCD_DC=42, LCD_CS=2, LCD_RST=硬件拉低无引脚, LCD_BL=1）
 *
 *     TFT_WIDTH  = 142
 *     TFT_HEIGHT = 428
 *     rotation   = 1
 *     IPS        = false
 *
 *     column offset 1 = 12, row offset 1 = 0
 *     column offset 2 = 14, row offset 2 = 0
 */

#include "DisplayDriver.h"

#include "hardware/PinMap.h"

namespace ekeys
{

    DisplayDriver::DisplayDriver()
        : bus_(nullptr), gfx_(nullptr)
    {
    }

    DisplayDriver &DisplayDriver::instance()
    {
        static DisplayDriver inst;
        return inst;
    }

    bool DisplayDriver::begin(uint32_t spi_hz)
    {
        if (bus_ == nullptr)
        {
            bus_ = new Arduino_ESP32SPI(
                kPinLcdDc,
                kPinLcdCs,
                kPinLcdSclk,
                kPinLcdMosi,
                GFX_NOT_DEFINED);
        }
        if (gfx_ == nullptr)
        {
            gfx_ = new Arduino_NV3007(
                bus_,
                kPinLcdRst,
                1,     // rotation
                false, // IPS
                142,   // TFT_WIDTH
                428,   // TFT_HEIGHT
                12,    // column offset 1
                0,     // row offset 1
                14,    // column offset 2
                0,     // row offset 2
                nv3007_279_init_operations,
                sizeof(nv3007_279_init_operations));
        }
        return gfx_->begin(spi_hz);
    }

    void DisplayDriver::fillScreen(uint16_t color)
    {
        if (gfx_)
        {
            gfx_->fillScreen(color);
        }
    }

    void DisplayDriver::draw16bitRGBBitmap(int32_t x, int32_t y, uint16_t *bitmap,
                                           int32_t w, int32_t h)
    {
        if (gfx_)
        {
            gfx_->draw16bitRGBBitmap(x, y, bitmap, w, h);
        }
    }

} // namespace ekeys
