/*
 * RGBLightControl.h
 *
 * RGB 灯效控制（FEATURE_DOC §9，阶段 06 任务 6.15）。
 *
 *   - RGBMode 枚举与 DeviceSettings.rgb_mode 同序
 *   - 单色模式使用 24 色调色板（rgb_single_colar 为调色板索引）
 *   - 动画循环由 DisplayTask::run() tick 驱动（约 30ms 一帧）
 *   - 点击高亮经 ClickHighlight 写入 override，本类渲染时优先
 */

#ifndef EKEYS_RGB_RGB_LIGHT_CONTROL_H
#define EKEYS_RGB_RGB_LIGHT_CONTROL_H

#include <stdint.h>

#include "config/DeviceSettings.h"

namespace ekeys {

enum RGBMode : uint8_t {
    RGB_NONE_MODE = 0,
    RGB_SINGLE_MODE = 1,
    RGB_RAINBOW_MODE = 2,
    RGB_RAINBOWWARE_MODE = 3,
    RGB_COLORCYCLE_MODE = 4,
    RGB_METER_MODE = 5,
    RGB_FIRE_MODE = 6,
    RGB_PULSE_MODE = 7,
};

/* 24 色调色板（FEATURE_DOC §9 单色模式索引） */
struct RgbColor {
    uint8_t r, g, b;
};

constexpr RgbColor kPalette24[24] = {
    {255, 255, 255}, {255, 0, 0},     {255, 64, 0},   {255, 128, 0},
    {255, 192, 0},   {255, 255, 0},   {192, 255, 0},  {128, 255, 0},
    {64, 255, 0},    {0, 255, 0},     {0, 255, 64},   {0, 255, 128},
    {0, 255, 192},   {0, 255, 255},   {0, 192, 255},  {0, 128, 255},
    {0, 64, 255},    {0, 0, 255},     {64, 0, 255},   {128, 0, 255},
    {192, 0, 255},   {255, 0, 255},   {255, 0, 192},  {255, 0, 128},
};

class RGBLightControl {
public:
    static RGBLightControl &instance();

    RGBLightControl(const RGBLightControl &) = delete;
    RGBLightControl &operator=(const RGBLightControl &) = delete;

    /* 设置变更时调用（SETTING_UPDATE / UI 反向同步后） */
    void applySettings(const DeviceSettings &snap);

    /* DisplayTask 周期驱动动画 */
    void tick(uint32_t elapsed_ms);

    /* 点击高亮 override（ClickHighlight 写入，tick 渲染时优先） */
    void setHighlight(uint8_t led, bool active);

private:
    RGBLightControl() = default;

    void renderFrame();
    RgbColor currentSingleColor() const;
    static uint8_t hueWheel(uint16_t hue);

    RGBMode mode_ = RGB_NONE_MODE;
    uint8_t single_index_ = 0;
    uint8_t brightness_ = 25;
    uint32_t elapsed_ms_ = 0;
    bool highlight_[11] = {false};
    uint32_t last_render_ms_ = 0;
    uint8_t fire_seed_[11] = {0};
};

}  // namespace ekeys

#endif  // EKEYS_RGB_RGB_LIGHT_CONTROL_H
