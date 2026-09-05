/*
 * AudioAnalyzer.h
 *
 * 频谱分析（FEATURE_DOC §10.3，阶段 06 任务 6.9）。
 *
 *   - FFT_SIZE=512 / BANDS=16，底层 arduinoFFT@2.0.4（ArduinoFFT<double> 模板 API）
 *   - DC 去除 + 汉宁窗 → 幅度谱 → 16 频段能量（对数压缩到 0~1）
 *   - 本阶段只编译不调度；阶段 07 频谱屏接入
 */

#ifndef EKEYS_AUDIO_AUDIO_ANALYZER_H
#define EKEYS_AUDIO_AUDIO_ANALYZER_H

#include <stddef.h>
#include <stdint.h>

namespace ekeys
{

    class AudioAnalyzer
    {
    public:
        static AudioAnalyzer &instance();

        AudioAnalyzer(const AudioAnalyzer &) = delete;
        AudioAnalyzer &operator=(const AudioAnalyzer &) = delete;

        /* 惰性分配缓冲（PSRAM），失败返回 false */
        bool begin();

        /*
         * B4 修复：释放 PSRAM 双缓冲（v_real_ / v_imag_），
         * 与 DisplayTask 停频谱路径的 Mic::end() 对称。
         */
        void end();

        /*
         * 输入一帧 PCM（最多 FFT_SIZE 样本，不足补零）。
         * 输出写入 out_bands[16]（0~1）。
         */
        void process(const int16_t *samples, size_t count,
                     float *out_bands, size_t band_cap);

        static constexpr size_t kFftSize = 512;
        static constexpr size_t kBandCount = 16;
        static constexpr uint32_t kSampleRate = 16000;

    private:
        AudioAnalyzer() = default;

        double *v_real_ = nullptr; // kFftSize（arduinoFFT 2.0.4 模板 T=double）
        double *v_imag_ = nullptr; // kFftSize
        bool inited_ = false;
    };

} // namespace ekeys

#endif // EKEYS_AUDIO_AUDIO_ANALYZER_H
