#pragma once
#include <Arduino.h>
#include <driver/i2s.h>

/* 与 FunModularKeyboard 的 Mic.h 引脚保持一致。
 * 适用于 INMP441 / SPH0645 / ICS-43434 等标准 I2S 数字麦克风模块。 */
#define MIC_I2S_BCLK 11
#define MIC_I2S_WS 17
#define MIC_I2S_DATA 18

#define MIC_I2S_NUM I2S_NUM_0
#define MIC_SAMPLE_RATE 16000
#define MIC_BUFFER_SAMPLES 512

/* ICS-43434 数据手册：只支持 24-bit 模式（左对齐到 32-bit slot）。
 * ESP-IDF I2S 端没有 24-bit 选项；32-bit 模式下，
 * 硬件只把 32-bit slot 左对齐的 24-bit 数据放进缓冲区高位 24 位，
 * 低 8 位为 0，所以可以放心用 int32_t 读 + 直接当 24-bit 用。 */
#define MIC_BITS_PER_SAMPLE 32

namespace ekeys
{

    /* 基于 ESP-IDF I2S 驱动的数字麦克风读取封装。
     *
     * 用法：
     *   Mic mic;
     *   if (!mic.begin()) { ... }
     *   int32_t buf[MIC_BUFFER_SAMPLES];
     *   size_t n = mic.read(buf, MIC_BUFFER_SAMPLES);
     *
     * 设计要点：
     *   - I2S_MODE_MASTER | I2S_MODE_RX：ESP32-S3 主动提供 BCLK/WS，从麦克风读数据
     *   - I2S_CHANNEL_FMT_ONLY_LEFT：单声道麦克风（INMP441 L/R 脚接地时为左声道）
     *   - 16 kHz / 32-bit slot：兼容 INMP441 (16/24-bit) 与 ICS-43434 (24-bit only)
     *   - DMA 双缓冲 (count=4, len=512/4=128)：保证 read() 不会长时间阻塞
     */
    class Mic
    {
    public:
        Mic();
        ~Mic();

        bool begin();
        /* 阻塞读取 samples_count 个 int32_t 样本，返回实际读取到的样本数。
         * 数据是 24-bit 左对齐到 32-bit slot，高 24-bit 有效，低 8-bit 为 0。
         * 失败 / 未初始化返回 0。 */
        size_t read(int32_t *buffer, size_t samples_count);
        void end();
        bool reset();

        bool isInitialized() const { return initialized_; }

    private:
        bool initialized_ = false;
        i2s_config_t i2s_config_{};
        i2s_pin_config_t pin_config_{};
    };

} // namespace ekeys