/*
 * AudioAnalyzer.cpp
 *
 * 见 AudioAnalyzer.h。arduinoFFT@2.0.4 模板 API
 * （ArduinoFFT<double> / compute(FFTDirection::Forward) / complexToMagnitude）。
 */

#include "AudioAnalyzer.h"

#include <Arduino.h>
#include <arduinoFFT.h>

#include "logging/LogManager.h"

namespace ekeys
{

  AudioAnalyzer &AudioAnalyzer::instance()
  {
    static AudioAnalyzer inst;
    return inst;
  }

  bool AudioAnalyzer::begin()
  {
    if (inited_)
    {
      return true;
    }
    v_real_ = static_cast<double *>(ps_malloc(sizeof(double) * kFftSize));
    v_imag_ = static_cast<double *>(ps_malloc(sizeof(double) * kFftSize));
    if (v_real_ == nullptr || v_imag_ == nullptr)
    {
      LOG_ERROR("ANALYZER", "ps_malloc failed");
      free(v_real_);
      free(v_imag_);
      v_real_ = v_imag_ = nullptr;
      return false;
    }
    inited_ = true;
    return true;
  }

  void AudioAnalyzer::end()
  {
    if (!inited_)
    {
      return;
    }
    free(v_real_);
    free(v_imag_);
    v_real_ = nullptr;
    v_imag_ = nullptr;
    inited_ = false;
    LOG_INFO("ANALYZER", "psram buffers released");
  }

  void AudioAnalyzer::process(const int16_t *samples, size_t count,
                              float *out_bands, size_t band_cap)
  {
    if (!inited_ || out_bands == nullptr || band_cap < kBandCount)
    {
      return;
    }
    /* C5 修复：count=0 时跳过 FFT，避免空帧白算 ~5ms CPU。
     * 调用方（DisplayTask 频谱）每 20ms 一次，少算一帧不影响视觉。 */
    if (count == 0)
    {
      for (size_t b = 0; b < kBandCount; ++b)
      {
        out_bands[b] = 0.0f;
      }
      return;
    }
    if (count > kFftSize)
    {
      count = kFftSize;
    }

    /* DC 去除 + 汉宁窗 + 补零 */
    double mean = 0;
    for (size_t i = 0; i < count; ++i)
    {
      mean += samples[i];
    }
    mean = (count > 0) ? mean / static_cast<double>(count) : 0;

    for (size_t i = 0; i < kFftSize; ++i)
    {
      if (i < count)
      {
        const double window = 0.5 * (1.0 - cos(2.0 * PI * i / (kFftSize - 1)));
        v_real_[i] = static_cast<float>((samples[i] - mean) * window);
      }
      else
      {
        v_real_[i] = 0.0f;
      }
      v_imag_[i] = 0.0f;
    }

    /*
     * 幅度谱 0..N/2 均分 16 段（跳过 DC），每段取峰值。
     * 已手工加汉宁窗，无需再调用 windowing()（Rectangle 窗为无操作）。
     *
     * B4 修复：原经验系数 8000 未上机校准；改为按本帧整体峰值自适应归一化，
     * 避免高频段贴地、满量程过载。两方向都偏向合理的视觉效果。
     */
    ArduinoFFT<double> fft(v_real_, v_imag_,
                           static_cast<uint_fast16_t>(kFftSize),
                           static_cast<double>(kSampleRate));
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();

    const size_t usable = kFftSize / 2 - 1;
    const size_t per_band = usable / kBandCount;
    double peaks[kBandCount];
    double frame_peak = 0;
    for (size_t b = 0; b < kBandCount; ++b)
    {
      double peak = 0;
      for (size_t i = 1 + b * per_band; i < 1 + (b + 1) * per_band; ++i)
      {
        if (v_real_[i] > peak)
        {
          peak = v_real_[i];
        }
      }
      peaks[b] = peak;
      if (peak > frame_peak)
      {
        frame_peak = peak;
      }
    }
    /*
     * 自适应归一化：以本帧最大 bin 的 1/4 作为分母。
     * - floor=1.0 防止静默帧除零
     * - 每段都按 frame_peak 归一化并裁到 0~1
     */
    const double denom = (frame_peak > 4.0) ? (frame_peak * 0.25) : 1.0;
    for (size_t b = 0; b < kBandCount; ++b)
    {
      float v = static_cast<float>(peaks[b] / denom);
      if (v < 0.0f)
      {
        v = 0.0f;
      }
      else if (v > 1.0f)
      {
        v = 1.0f;
      }
      out_bands[b] = v;
    }
  }

} // namespace ekeys
