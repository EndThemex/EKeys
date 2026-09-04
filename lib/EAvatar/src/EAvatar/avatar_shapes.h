#pragma once
#include "avatar_types.h"
#include "avatar_math.h"
namespace eavatar
{
  struct ShapeProfile
  {
    float radius[PROFILE_SAMPLES];
  };
  class ShapeLibrary
  {
  public:
    static void circle(ShapeProfile &o, float scale = 1)
    {
      for (int i = 0; i < PROFILE_SAMPLES; i++)
        o.radius[i] = scale;
    }
    static void egg(ShapeProfile &o)
    {
      for (int i = 0; i < PROFILE_SAMPLES; i++)
      {
        float a = i * EA_TAU / PROFILE_SAMPLES;
        o.radius[i] = 0.92f + 0.08f * sinf(a) + 0.025f * cosf(2 * a);
      }
    }
    static void hexagon(ShapeProfile &o)
    {
      for (int i = 0; i < PROFILE_SAMPLES; i++)
      {
        float a = i * EA_TAU / PROFILE_SAMPLES;
        float c = cosf(a), s = sinf(a);
        float m = fmaxf(fabsf(c), fabsf(s));
        float hex = 0.93f / (m + 0.10f * fabsf(c * s));
        o.radius[i] = clamp(hex, 0.90f, 1.03f);
      }
    }
    static void triangle(ShapeProfile &o)
    {
      for (int i = 0; i < PROFILE_SAMPLES; i++)
      {
        float a = i * EA_TAU / PROFILE_SAMPLES;
        float top = 0.72f + 0.43f * (0.5f + 0.5f * sinf(a - EA_PI / 2));
        float side = 0.78f + 0.26f * fabsf(sinf(a));
        o.radius[i] = clamp(top * side, 0.70f, 1.12f);
      }
    }
    static void profileFor(AvatarState s, ShapeProfile &o)
    {
      switch (s)
      {
      case AvatarState::Egg:
        egg(o);
        break;
      case AvatarState::Hexagon:
        hexagon(o);
        break;
      case AvatarState::Play:
        triangle(o);
        break;
      default:
        circle(o);
        break;
      }
    }
  };
}
