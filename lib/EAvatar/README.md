# EAvatar-LVGL83 (for EKeys)

This is the EKeys-flavored port of EAvatar-LVGL v0.2.0, adapted to use the
LVGL 8.3.11 software-renderer draw context API instead of the LVGL 9.x
`lv_vector_*` API used in the original.

## Layout

- `src/avatar_types.h` — public types (AvatarState/Expression/Event, AvatarFrame)
- `src/avatar_math.h` — math helpers (lerp, easing, hash, noise)
- `src/avatar_shapes.h` — radial shape profiles (circle/egg/hexagon/triangle)
- `src/avatar_engine.h/.cpp` — pure state machine (no LVGL dependency)
- `src/eavatar_lvgl83.h/.cpp` — LVGL 8.3.11 adapter (lv_obj + draw_ctx)

## EKeys 142×428 portrait layout

The original EAvatar v0.2.0 targets a 428×124 landscape strip. EKeys has a
142×428 portrait LCD, so the wrapper overrides:

- center: cx=214, cy=78 (slightly above mid to leave room for hint text)
- body radius: 42 px (≈ 84 px diameter, fits 142 px height with eye room)
- eye x offset: ±16 px additional push outward (default `eye_split_deg` of
  ~15° in portrait would cause the two eyes to overlap)

Everything else — 64-sample radial profile, capsule eyes, sphere-style gaze
projection, 180 ms blink, idle breathing, state morphing — is identical to
EAvatar v0.2.0.

## API

```cpp
#include "EAvatar/eavatar_lvgl83.h"

eavatar::EAvatar avatar;

avatar.begin(lv_scr_act(), 428, 142);
avatar.event(AvatarEvent::WakeUp);

// Drive from AI state:
avatar.event(AvatarEvent::StartListening);
avatar.event(AvatarEvent::StartThinking);
avatar.event(AvatarEvent::StartSpeaking);
avatar.setSpeakingLevel(0.65f);  // 0..1
```

## Memory

No per-frame heap allocation. The widget renders via `draw_ctx->draw_polygon`
into the existing LVGL display buffer, so it does not require a separate
canvas buffer.
