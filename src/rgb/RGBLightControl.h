/*
 * RGBLightControl.h
 *
 * RGB 灯效控制（FEATURE_DOC §9，阶段 06 任务 6.15）。
 *
 *   - RGBMode 枚举与 DeviceSettings.rgb_mode 同序
 *   - 单色模式使用 24 色调色板（rgb_single_color 为调色板索引）
 *   - 动画循环由 DisplayTask::run() tick 驱动（约 30ms 一帧）
 *   - 点击高亮经 ClickHighlight 写入 override，本类渲染时优先
 */

#ifndef EKEYS_RGB_RGB_LIGHT_CONTROL_H
#define EKEYS_RGB_RGB_LIGHT_CONTROL_H

#include <stdint.h>

#include "config/DeviceSettings.h"

namespace ekeys
{

    enum RGBMode : uint8_t
    {
        RGB_NONE_MODE = 0,
        RGB_SINGLE_MODE = 1,
        RGB_RAINBOW_MODE = 2,
        RGB_RAINBOWWARE_MODE = 3,
        RGB_COLORCYCLE_MODE = 4,
        RGB_METER_MODE = 5,
        RGB_FIRE_MODE = 6,
        RGB_PULSE_MODE = 7,
        RGB_SOUND_MODE = 8,
        RGB_MATRIX_MODE = 9,
        RGB_GRADIENT_MODE = 10,
    };

    /* 24 色调色板（FEATURE_DOC §9 单色模式索引） */
    struct RgbColor
    {
        uint8_t r, g, b;
    };

    constexpr RgbColor kPalette24[24] = {
        {255, 255, 255},
        {255, 0, 0},
        {255, 64, 0},
        {255, 128, 0},
        {255, 192, 0},
        {255, 255, 0},
        {192, 255, 0},
        {128, 255, 0},
        {64, 255, 0},
        {0, 255, 0},
        {0, 255, 64},
        {0, 255, 128},
        {0, 255, 192},
        {0, 255, 255},
        {0, 192, 255},
        {0, 128, 255},
        {0, 64, 255},
        {0, 0, 255},
        {64, 0, 255},
        {128, 0, 255},
        {192, 0, 255},
        {255, 0, 255},
        {255, 0, 192},
        {255, 0, 128},
    };

    class RGBLightControl
    {
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

        /* 拾音模式：DisplayTask 频谱链路喂入 16 频段能量（0~1） */
        void setAudioBands(const float *bands, uint8_t count);

        /* 当前是否为拾音模式（DisplayTask 据此决定是否接管 Mic） */
        bool wantsMic() const
        {
            return mode_ == RGB_SOUND_MODE || mode_ == RGB_MATRIX_MODE ||
                   mode_ == RGB_GRADIENT_MODE;
        }

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
        /* 拾音模式：最近一帧 16 频段能量（0~1）与 11 灯显示电平（含回落） */
        float audio_bands_[16] = {0};
        float audio_level_[11] = {0};
        /* 矩阵律动：4 列电平（含回落），列 = 频段组，行高 = 音量 */
        float audio_col_[4] = {0};
        /* NONE 模式下上帧渲染的高亮掩码（位 i = highlight_[i]）。
         * 11 颗 LED 需 11 位，必须 uint16_t（uint8_t 会截断 LED 8~10） */
        uint16_t last_none_mask_ = 0xFFFF;
    };

} // namespace ekeys

#endif // EKEYS_RGB_RGB_LIGHT_CONTROL_H
