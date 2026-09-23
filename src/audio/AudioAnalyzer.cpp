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
    /* 复位长时峰值跟踪：重新启用（如离开拾音灯效后回来）时增益重新收敛，
     * 避免沿用上一段会话的高增益把安静片段放大 */
    peak_track_ = 0.0;
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
    /*
     * 粉噪补偿表（取自 WLED usermods/audioreactive 的 fftResultPink[16]）：
     * 音乐频谱能量随频率自然滚降（约 -6dB/oct），不做补偿时高频段/高频列
     * 长期贴地、只有低频在动。表中系数按段递增放大（最高 9.55×）。
     * 注意：本机 16 段是等带宽（各 ~469Hz），与 WLED 的分段区间不完全一致，
     * 上机后可按实测观感再调这 16 个系数。
     */
    constexpr double kPinkComp[kBandCount] = {
        1.70, 1.71, 1.73, 1.78, 1.68, 1.56, 1.55, 1.63,
        1.79, 1.62, 1.80, 2.06, 2.47, 3.35, 6.83, 9.55};
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
      peaks[b] = peak * kPinkComp[b];
      if (peaks[b] > frame_peak)
      {
        frame_peak = peaks[b];
      }
    }
    /*
     * 长时峰值归一化（LedFx melbank 的 mel_gain 思路：ExpFilter 上升 0.99 /
     * 回落 0.01，即瞬时跟上、之后极慢回落）。
     *
     * B4 修复（2026-09-23 二次修订）：原实现用"本帧峰值 × 0.25"逐帧归一化，
     * 任何达到帧峰值 25% 的频段都会被钳到 1.0 —— 一个瞬间尖峰就把整片频段
     * 推满，段间/列间层次被抹平（RGB 侧只能靠静噪门限和回落掩盖）。
     * 改为跟踪长时峰值（约 3s 量级回落）后相除：短促尖峰不再拉满全场，
     * 持续响度才逐步推高增益，动态范围回到 0~1 的可用区间。
     *
     * 底噪兜底 kAbsNoiseFloor：长时峰值低于它时按门限值归一，避免安静时
     * 把环境底噪放大成满量程（初值按 ICS43434 -26dBFS@94dB SPL 估算，
     * 普通室内底噪帧峰值约 300~3000，正常说话/音乐 3 万+）。
     */
    /* 上升系数：约 3~5 帧（60~100ms）跟上。原 0.99 瞬时跟上会把鼓点瞬态
     * 当帧归一化掉（最强频段永远 ≈1.0），动态在源头被压平、灯效无跳动；
     * 放缓后瞬态先冲满再被归一化压回，节拍起伏回到 0~1 区间。
     * 回落系数（≈100 帧 ≈ 3s）保持不变。 */
    constexpr double kPeakTrackRise = 0.25;
    constexpr double kPeakTrackDecay = 0.01; /* 回落系数（≈100 帧 ≈ 3s） */
    constexpr double kAbsNoiseFloor = 8000.0;
    const double track_k = (frame_peak > peak_track_) ? kPeakTrackRise
                                                      : kPeakTrackDecay;
    peak_track_ += (frame_peak - peak_track_) * track_k;
    const double denom = (peak_track_ > kAbsNoiseFloor) ? peak_track_
                                                        : kAbsNoiseFloor;
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
