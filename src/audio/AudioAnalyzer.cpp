/*
 * AudioAnalyzer.cpp
 *
 * 见 AudioAnalyzer.h。arduinoFFT@2.0.4 模板 API
 * （ArduinoFFT<float> / compute(FFTDirection::Forward) / complexToMagnitude）。
 */

#include "AudioAnalyzer.h"

#include <Arduino.h>
#include <arduinoFFT.h>

#include "logging/LogManager.h"

namespace ekeys
{

  namespace
  {
    /*
     * 逐段非对称平滑（A2：原分散在 RGBLightControl 三个灯效里）。
     * 系数按 FFT hop=256 @16kHz（约 62.5fps）标定：把原效果层 30ms 帧下的
     * 0.55/0.13 按 1-(1-k)^(16/30) 换算，得到等价系数 0.35/0.072，
     * 起跳 ~2~3 帧到顶、回落约 1s 拖尾的观感与原实现保持一致。
     */
    constexpr float kSmoothAttack = 0.35f;
    constexpr float kSmoothDecay = 0.072f;
    constexpr float kSmoothEpsilon = 0.004f;

    /* 单段电平的一步非对称平滑 + 残留截断 */
    float smoothBand(float current, float target)
    {
      const float k = (target > current) ? kSmoothAttack : kSmoothDecay;
      float value = current + (target - current) * k;
      if (value < kSmoothEpsilon)
      {
        return 0.0f;
      }
      if (value > 1.0f)
      {
        return 1.0f;
      }
      return value;
    }
  } // namespace

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
    v_real_ = static_cast<float *>(ps_malloc(sizeof(float) * kFftSize));
    v_imag_ = static_cast<float *>(ps_malloc(sizeof(float) * kFftSize));
    window_ = static_cast<float *>(ps_malloc(sizeof(float) * kFftSize));
    if (v_real_ == nullptr || v_imag_ == nullptr || window_ == nullptr)
    {
      LOG_ERROR("ANALYZER", "ps_malloc failed");
      free(v_real_);
      free(v_imag_);
      free(window_);
      v_real_ = v_imag_ = window_ = nullptr;
      return false;
    }
    /* 汉宁窗只算一次：原实现每帧 512 次 cos()，在 16ms 周期里开销明显 */
    for (size_t i = 0; i < kFftSize; ++i)
    {
      window_[i] = static_cast<float>(
          0.5 * (1.0 - cos(2.0 * PI * i / (kFftSize - 1))));
    }
    for (size_t b = 0; b < kBandCount; ++b)
    {
      smooth_[b] = 0.0f;
    }
    peak_track_ = 0.0f;
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
    free(window_);
    v_real_ = nullptr;
    v_imag_ = nullptr;
    window_ = nullptr;
    inited_ = false;
    /* 复位长时峰值跟踪：重新启用（如离开拾音灯效后回来）时增益重新收敛，
     * 避免沿用上一段会话的高增益把安静片段放大 */
    peak_track_ = 0.0f;
    for (size_t b = 0; b < kBandCount; ++b)
    {
      smooth_[b] = 0.0f;
    }
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
     * 调用方（DisplayTask 频谱）每 16ms 一次，少算一帧不影响视觉。
     * 平滑状态仍按回落方向推进，避免读取超时造成电平瞬断。 */
    if (count == 0)
    {
      for (size_t b = 0; b < kBandCount; ++b)
      {
        smooth_[b] = smoothBand(smooth_[b], 0.0f);
        out_bands[b] = smooth_[b];
      }
      return;
    }
    if (count > kFftSize)
    {
      count = kFftSize;
    }

    /* DC 去除 + 预计算汉宁窗 + 补零 */
    float mean = 0;
    for (size_t i = 0; i < count; ++i)
    {
      mean += samples[i];
    }
    mean = (count > 0) ? mean / static_cast<float>(count) : 0;

    for (size_t i = 0; i < kFftSize; ++i)
    {
      if (i < count)
      {
        v_real_[i] = (samples[i] - mean) * window_[i];
      }
      else
      {
        v_real_[i] = 0.0f;
      }
      v_imag_[i] = 0.0f;
    }

    /*
     * 幅度谱 0..N/2 均分 16 段（跳过 DC），每段取峰值。
     * 窗口已在上面乘过，无需再调用 windowing()。
     *
     * B4 修复：原经验系数 8000 未上机校准；改为按本帧整体峰值自适应归一化，
     * 避免高频段贴地、满量程过载。两方向都偏向合理的视觉效果。
     */
    ArduinoFFT<float> fft(v_real_, v_imag_,
                          static_cast<uint_fast16_t>(kFftSize),
                          static_cast<float>(kSampleRate));
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
    constexpr float kPinkComp[kBandCount] = {
        1.70f, 1.71f, 1.73f, 1.78f, 1.68f, 1.56f, 1.55f, 1.63f,
        1.79f, 1.62f, 1.80f, 2.06f, 2.47f, 3.35f, 6.83f, 9.55f};
    float peaks[kBandCount];
    float frame_peak = 0;
    for (size_t b = 0; b < kBandCount; ++b)
    {
      float peak = 0;
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
    constexpr float kPeakTrackRise = 0.25f;
    constexpr float kPeakTrackDecay = 0.01f; /* 回落系数（≈100 帧 ≈ 3s） */
    constexpr float kAbsNoiseFloor = 8000.0f;
    const float track_k = (frame_peak > peak_track_) ? kPeakTrackRise
                                                     : kPeakTrackDecay;
    peak_track_ += (frame_peak - peak_track_) * track_k;
    const float denom = (peak_track_ > kAbsNoiseFloor) ? peak_track_
                                                       : kAbsNoiseFloor;
    for (size_t b = 0; b < kBandCount; ++b)
    {
      float v = peaks[b] / denom;
      if (v < 0.0f)
      {
        v = 0.0f;
      }
      else if (v > 1.0f)
      {
        v = 1.0f;
      }
      /* A2：平滑在分析层统一完成，效果层直接消费 */
      smooth_[b] = smoothBand(smooth_[b], v);
      out_bands[b] = smooth_[b];
    }
  }

} // namespace ekeys
