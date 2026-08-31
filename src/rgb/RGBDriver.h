/*
 * RGBDriver.h
 *
 * WS2812B 底层驱动（PINOUT §2.4，阶段 06 任务 6.14）。
 *
 *   - 11 颗，与 11 个物理键一一对应（下标 0~10 = 键 ID 1~11）
 *   - GRB 顺序（NEO_GRB），信号 IO15，电源使能 LED_PWR_CTRL=IO21
 *   - IO21 上电默认拉高（先供电再驱动 DIN）
 *
 * 底层库：Adafruit NeoPixel（RMT），与 LCD SPI 无冲突。
 */

#ifndef EKEYS_RGB_RGB_DRIVER_H
#define EKEYS_RGB_RGB_DRIVER_H

#include <stdint.h>

namespace ekeys {

class RGBDriver {
public:
    static RGBDriver &instance();

    RGBDriver(const RGBDriver &) = delete;
    RGBDriver &operator=(const RGBDriver &) = delete;

    void begin();

    static constexpr uint8_t kLedCount = 11;

    /* led 0~10 */
    void setPixel(uint8_t led, uint8_t r, uint8_t g, uint8_t b);
    void setAll(uint8_t r, uint8_t g, uint8_t b);
    void clearAll();
    void show();

    /* 0~100（映射 strip.setBrightness 0~255） */
    void SetBrightness(uint8_t brightness);

private:
    RGBDriver() = default;

    void *impl_ = nullptr;  // Adafruit_NeoPixel*
    bool inited_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_RGB_RGB_DRIVER_H
