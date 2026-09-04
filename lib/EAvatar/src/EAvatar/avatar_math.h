#pragma once
#include <math.h>
#include <stdint.h>
namespace eavatar
{
  /* 注意：不要命名为 PI / TAU —— Arduino.h 已经 #define 了它们，
   * 宏展开后 constexpr 声明会变成 "constexpr float 3.14... = ..."，
   * 编译报 "expected unqualified-id before numeric constant"。
   * 用 EA_PI / EA_TAU 前缀避开宏冲突。 */
  constexpr float EA_PI = 3.14159265358979323846f;
  constexpr float EA_TAU = 6.28318530717958647692f;
  constexpr int PROFILE_SAMPLES = 64;
  inline float clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
  inline float clamp01(float v) { return clamp(v, 0, 1); }
  inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
  inline float easeOutCubic(float t)
  {
    t = clamp01(t);
    float u = 1 - t;
    return 1 - u * u * u;
  }
  inline float easeInOutCubic(float t)
  {
    t = clamp01(t);
    return t < .5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2;
  }
  inline float smoothstep(float t)
  {
    t = clamp01(t);
    return t * t * (3 - 2 * t);
  }
  inline float fract(float x) { return x - floorf(x); }
  inline float hash01(uint32_t x)
  {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return (x & 0x00ffffff) / 16777215.0f;
  }
  inline float noise(float t, float period, float phase)
  {
    float x = t / period + phase;
    int32_t i = (int32_t)floorf(x);
    float f = fract(x);
    float a = hash01((uint32_t)i + 0x51ed);
    float b = hash01((uint32_t)i + 1u + 0x51ed);
    return lerp(a, b, smoothstep(f)) * 2.0f - 1.0f;
  }
}
