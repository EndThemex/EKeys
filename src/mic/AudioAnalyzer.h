#pragma once
#include <Arduino.h>
#include <ArduinoFFT.h>

#define AUDIO_FFT_SIZE 512
#define AUDIO_BANDS 16

namespace ekeys
{

    /* 简易频谱分析器：把 24-bit 左对齐的 int32_t PCM 样本 → 16 段能量带。
     *
     * 处理流程：
     *   1) 去直流 (DC removal)：减去平均值，避免 50Hz/60Hz 工频主导低频
     *   2) Hanning 窗：降低 FFT 频谱泄漏
     *   3) FFT → 复数转幅度
     *   4) 把 0..N/2 频带均分成 BANDS 段，求平均作为该段能量
     *   5) 简单增益与低频段衰减 (与 FunModularKeyboard 行为一致)
     *
     * 输入：int32_t 数组，高 24-bit 有效（左对齐），低 8-bit 为 0。
     *      内部右移 8 位得到 int16 等价值送 FFT；24->16 位动态范围损失
     *      对语音/音乐可视化无感（信噪比仍 ~96 dB）。
     *
     * getBands() 返回的数值未做 log/dB 化，调用方按需再做可视化映射。 */
    class AudioAnalyzer
    {
    public:
        explicit AudioAnalyzer(uint32_t sampleRate);

        void begin();
        void process(const int32_t *samples, size_t count);
        const float *getBands() const;

    private:
        void applyWindow();
        void removeDC();
        void computeFFT();
        void computeBands();

        uint32_t sampleRate_;

        float vReal_[AUDIO_FFT_SIZE];
        float vImag_[AUDIO_FFT_SIZE];
        float bands_[AUDIO_BANDS];

        ArduinoFFT<float> fft_;
    };

} // namespace ekeys