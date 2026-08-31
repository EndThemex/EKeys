/*
 * DisplayDriver.h
 *
 * 把当前 src/main.cpp 中已有的 NV3007 初始化代码迁出。
 * 提供 begin() / fillScreen() / draw16bitRGBBitmap()。
 */

#ifndef EKEYS_DISPLAY_DISPLAY_DRIVER_H
#define EKEYS_DISPLAY_DISPLAY_DRIVER_H

#include <Arduino_GFX_Library.h>
#include <stdint.h>

namespace ekeys {

class DisplayDriver {
public:
    static DisplayDriver &instance();

    bool begin(uint32_t spi_hz = 10000000);

    Arduino_GFX *gfx() const { return gfx_; }

    void fillScreen(uint16_t color);
    void draw16bitRGBBitmap(int32_t x, int32_t y, uint16_t *bitmap,
                            int32_t w, int32_t h);

private:
    DisplayDriver();
    DisplayDriver(const DisplayDriver &) = delete;
    DisplayDriver &operator=(const DisplayDriver &) = delete;

    Arduino_DataBus *bus_;
    Arduino_GFX      *gfx_;
};

}  // namespace ekeys

#endif  // EKEYS_DISPLAY_DISPLAY_DRIVER_H
