#include "AudioAnalyzer.h"
#include <math.h>
#include <string.h>

namespace ekeys
{

    AudioAnalyzer::AudioAnalyzer(uint32_t sampleRate)
        : sampleRate_(sampleRate),
          fft_(vReal_, vImag_, AUDIO_FFT_SIZE, sampleRate)
    {
        memset(bands_, 0, sizeof(bands_));
    }

    void AudioAnalyzer::begin()
    {
        memset(vReal_, 0, sizeof(vReal_));
        memset(vImag_, 0, sizeof(vImag_));
    }

    void AudioAnalyzer::process(const int32_t *samples, size_t count)
    {
        if (samples == nullptr || count == 0)
            return;

        const size_t n = (count < AUDIO_FFT_SIZE) ? count : AUDIO_FFT_SIZE;
        for (size_t i = 0; i < n; ++i)
        {
            /* 24-bit 左对齐到 32-bit slot，右移 8 位得到等效 int16。
             * 对带符号数必须用算术右移以保留符号位。 */
            vReal_[i] = static_cast<float>(samples[i] >> 8);
            vImag_[i] = 0.0f;
        }
        for (size_t i = n; i < AUDIO_FFT_SIZE; ++i)
        {
            vReal_[i] = 0.0f;
            vImag_[i] = 0.0f;
        }

        removeDC();
        applyWindow();
        computeFFT();
        computeBands();
    }

    void AudioAnalyzer::removeDC()
    {
        float mean = 0.0f;
        for (int i = 0; i < AUDIO_FFT_SIZE; ++i)
            mean += vReal_[i];
        mean /= AUDIO_FFT_SIZE;
        for (int i = 0; i < AUDIO_FFT_SIZE; ++i)
            vReal_[i] -= mean;
    }

    void AudioAnalyzer::applyWindow()
    {
        /* Hanning 窗 */
        for (int i = 0; i < AUDIO_FFT_SIZE; ++i)
        {
            vReal_[i] *= 0.5f * (1.0f - cosf(2.0f * (float)PI * i / (AUDIO_FFT_SIZE - 1)));
        }
    }

    void AudioAnalyzer::computeFFT()
    {
        fft_.compute(FFT_FORWARD);
        fft_.complexToMagnitude();
    }

    void AudioAnalyzer::computeBands()
    {
        const int binsPerBand = (AUDIO_FFT_SIZE / 2) / AUDIO_BANDS;
        for (int i = 0; i < AUDIO_BANDS; ++i)
        {
            float sum = 0.0f;
            const int start = i * binsPerBand;
            const int end = start + binsPerBand;
            for (int j = start; j < end; ++j)
                sum += vReal_[j];
            bands_[i] = (sum / binsPerBand) * 0.4f;
        }

        /* 低频段常常因为 DC 残留 / 1/f 噪声而虚高，做一点衰减 */
        bands_[0] *= 0.1f;
        bands_[1] *= 0.1f;
        bands_[2] *= 0.1f;
    }

    const float *AudioAnalyzer::getBands() const
    {
        return bands_;
    }

} // namespace ekeys