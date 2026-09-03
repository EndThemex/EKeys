---
name: eavatar-lvgl
description: 当需要实现角色状态动画时
---

# EAvatar-LVGL Skill

## Overview

EAvatar-LVGL is an embedded Avatar Engine designed for:

- ESP32-S3
- LVGL 9.x
- 428×124 wide LCD
- 30 FPS animation
- Resource-constrained embedded systems

Its visual design is strongly inspired by the animation language of Bloub:

- parametric blob body
- radial shape profiles
- smooth morphing
- capsule-shaped eyes
- gaze movement
- asymmetric blinking
- breathing
- subtle idle motion
- state-driven expressions

The implementation is independent and must not copy Bloub source code or assets.

The primary objective is:

> Create a visually expressive, lightweight, continuously animated AI Avatar suitable for a physical ESP32-S3 device.

---

# 1. Core Architecture

The architecture is:

```text
Application
    │
    ├── Microphone
    ├── AI / LLM
    ├── TTS
    ├── Buttons
    └── Rotary Encoder
             │
             ▼
      Avatar State Machine
             │
             ▼
       EAvatar Engine
             │
             ▼
        AvatarFrame
             │
             ▼
        LVGL Renderer
             │
             ▼
        428 × 124 LCD
```

Do not put application logic directly inside the renderer.

The recommended separation is:

```text
AI application
      ↓
AvatarEvent / AvatarState
      ↓
AvatarEngine
      ↓
AvatarFrame
      ↓
LVGL
```

---

# 2. Basic Usage

Initialize LVGL first.

```cpp
#include <lvgl.h>
#include "avatar/eavatar.h"

using namespace eavatar;

EAvatar avatar;

void setup()
{
    lv_init();

    // Initialize the 428x124 LCD and LVGL display here.

    avatar.begin(
        lv_screen_active(),
        428,
        124
    );

    avatar.event(AvatarEvent::WakeUp);
}
```

The display driver is intentionally independent from EAvatar.

EAvatar must not assume a specific LCD controller.

---

# 3. Main API

## Initialization

```cpp
avatar.begin(parent, width, height);
```

Example:

```cpp
avatar.begin(
    lv_screen_active(),
    428,
    124
);
```

---

## Update

The LVGL timer normally updates the Avatar automatically.

If manual updating is required:

```cpp
avatar.update();
```

Do not create a high-frequency FreeRTOS task only for the Avatar.

Prefer:

```text
LVGL timer
    ↓
Avatar update
    ↓
invalidate
    ↓
LVGL render
```

Target:

```text
30 FPS
≈ 33 ms/frame
```

60 FPS should only be used when the display and rendering pipeline can sustain it.

---

# 4. Avatar Events

Use events to describe semantic changes.

```cpp
avatar.event(AvatarEvent::WakeUp);
avatar.event(AvatarEvent::StartListening);
avatar.event(AvatarEvent::StopListening);
avatar.event(AvatarEvent::StartThinking);
avatar.event(AvatarEvent::StartSpeaking);
avatar.event(AvatarEvent::Success);
avatar.event(AvatarEvent::Error);
avatar.event(AvatarEvent::Sleep);
avatar.event(AvatarEvent::UserInteraction);
```

Recommended mapping:

| Event | Visual meaning |
|---|---|
| WakeUp | Avatar wakes |
| StartListening | User is speaking |
| StopListening | Listening finished |
| StartThinking | AI is processing |
| StartSpeaking | TTS playback |
| Success | Successful operation |
| Error | Error state |
| Sleep | Device sleeping |
| UserInteraction | User touched/pressed device |

Prefer semantic events over directly manipulating geometry.

Bad:

```cpp
avatar.setEyeSize(...);
avatar.setBodyScale(...);
avatar.setRotation(...);
```

Good:

```cpp
avatar.event(AvatarEvent::StartThinking);
```

The engine should decide how Thinking looks.

---

# 5. Expressions

Available expressions:

```cpp
AvatarExpression::Neutral
AvatarExpression::Attentive
AvatarExpression::Surprised
AvatarExpression::Happy
AvatarExpression::Angry
AvatarExpression::Sad
AvatarExpression::Sleepy
AvatarExpression::Confused
```

Example:

```cpp
avatar.setExpression(
    AvatarExpression::Happy
);
```

Expressions should be combined with states.

For example:

```cpp
avatar.event(AvatarEvent::StartSpeaking);
avatar.setExpression(AvatarExpression::Happy);
```

Do not create a separate state for every combination.

Prefer:

```text
State × Expression
```

instead of:

```text
HappySpeaking
HappyThinking
SadSpeaking
SadThinking
...
```

This prevents state explosion.

---

# 6. Gaze

Gaze is normalized:

```cpp
avatar.setGaze(x, y);
```

Recommended range:

```text
x = -1.0 ~ +1.0
y = -1.0 ~ +1.0
```

Example:

```cpp
avatar.setGaze(
    0.35f,
    -0.15f
);
```

Interpretation:

```text
           -Y
            ↑
            │
      left  │  right
        -X  ●  +X
            │
            ↓
           +Y
```

Do not instantly jump between gaze positions.

The Avatar Engine should interpolate movement.

For AI interaction:

```text
User voice detected
        ↓
Look toward center
        ↓
Listening
        ↓
Thinking
        ↓
Speaking
```

---

# 7. Speaking Animation

The device has a microphone and speaker.

Use the audio amplitude to drive:

```cpp
avatar.setSpeakingLevel(level);
```

where:

```text
level = 0.0 ~ 1.0
```

Example:

```cpp
float rms = calculateAudioRMS();

float level = normalizeRMS(rms);

avatar.setSpeakingLevel(level);
```

Do not map raw microphone samples directly to visual geometry.

Use:

```text
audio samples
     ↓
RMS / envelope
     ↓
low-pass filter
     ↓
attack/release
     ↓
0~1 speaking level
     ↓
Avatar
```

Recommended smoothing:

```text
Attack: 30~80 ms
Release: 100~250 ms
```

This prevents visual jitter.

---

# 8. Listening Animation

When the microphone detects speech:

```cpp
avatar.event(
    AvatarEvent::StartListening
);
```

Recommended visual behaviour:

```text
Idle
 │
 ├── gaze becomes attentive
 ├── eyes become slightly larger
 ├── body remains relatively stable
 └── blink frequency decreases slightly
```

Listening should feel attentive, not hyperactive.

---

# 9. Thinking Animation

Use:

```cpp
avatar.event(
    AvatarEvent::StartThinking
);
```

Thinking should NOT simply rotate the entire body.

Recommended behaviour:

```text
Body
 └── subtle breathing

Eyes
 ├── gaze drift
 ├── occasional directional movement
 └── normal blinking

Accent
 └── subtle movement
```

The perceived "life" should primarily come from the eyes.

Avoid large body animations during Thinking.

---

# 10. Idle Animation

Idle is the most important state.

A good idle Avatar should never look completely static.

Use:

```text
Body
 └── ±0.5~1% scale movement

Gaze
 └── very slow random drift

Blink
 └── random interval

Eye geometry
 └── tiny natural variation
```

The movement should be almost imperceptible.

The goal is:

> The user should feel that the Avatar is alive without consciously noticing the animation.

Do NOT use:

```cpp
sin(time * 10)
```

for obvious continuous shaking.

Prefer several low-frequency signals:

```text
breathing
+
gaze drift
+
blink
+
micro movement
```

---

# 11. Blink

Blink is a first-class animation.

Recommended duration:

```text
≈ 180 ms
```

Use asymmetric timing:

```text
Close
   ↓
Fast

Closed
   ↓
Short hold

Open
   ↓
Slightly slower
```

Avoid:

```text
open → closed → open
```

with constant linear speed.

Use easing.

Recommended:

```cpp
easeInOutCubic()
```

or equivalent.

---

# 12. Double Blink

Occasionally generate:

```text
Blink
  ↓
short pause
  ↓
Blink
```

Do not make double blink frequent.

A useful heuristic:

```text
Normal blink:
~90%

Double blink:
~10%
```

The exact probability can be tuned.

---

# 13. Blob Body

The body is represented as a radial profile.

Conceptually:

```text
              P0
              │
        P63 ──┼── P1
              │
              P32
```

There are:

```cpp
64
```

samples.

Each sample represents the radius at:

```cpp
angle = i * 2π / 64
```

The body is then reconstructed from these points.

Do not use only:

```cpp
lv_obj_set_style_radius(...)
```

because this cannot reproduce the intended organic deformation.

---

# 14. Shape Morphing

The body should transition continuously:

```text
Shape A
   ↓
 interpolation
   ↓
Shape B
```

For each radial sample:

```cpp
radius[i] =
    lerp(
        source[i],
        target[i],
        t
    );
```

Use easing for `t`.

Example:

```cpp
float k = easeOutCubic(t);
```

The body must never abruptly replace its shape.

Bad:

```cpp
currentShape = targetShape;
```

Good:

```cpp
for (int i = 0; i < 64; ++i)
{
    current[i] =
        lerp(
            from[i],
            target[i],
            progress
        );
}
```

---

# 15. Eye Geometry

Eyes should be treated independently from the body.

Each eye contains:

```text
width
height
open
tilt
alpha
x
y
angle
```

Do not use one shared eye transform.

This allows:

```text
left eye ≠ right eye
```

which is important for:

- Wink
- Confused
- Angry
- Sleepy
- Looking around

---

# 16. Eye Projection

Gaze should influence:

```text
eye position
eye width
eye rotation
eye perspective
```

A simple model:

```text
gaze
 ↓
yaw / pitch
 ↓
sphere projection
 ↓
eye transform
```

Avoid simply doing:

```cpp
eye.x += gazeX;
eye.y += gazeY;
```

because this looks like a 2D sprite.

The intended visual effect should suggest that the eyes are attached to a rounded 3D surface.

---

# 17. 428×124 Layout

The target display is:

```text
428 × 124
```

The Avatar should be designed specifically for this aspect ratio.

Recommended coordinate system:

```text
        428 px
┌───────────────────────────────┐
│                               │
│              ●                │
│                               │
└───────────────────────────────┘
              124 px
```

Default center:

```cpp
cx = 214;
cy = 62;
```

Do not simply render a 1:1 desktop version and scale it down.

The eye spacing, body radius and vertical position should be tuned for the low-height display.

---

# 18. Rendering Strategy

Prefer:

```text
Parametric Geometry
       ↓
LVGL Vector Drawing
       ↓
Display
```

Avoid:

```text
SVG
 ↓
parser
 ↓
large intermediate buffers
 ↓
LVGL
```

Avoid frame-by-frame bitmap generation.

The engine should generate geometry directly.

---

# 19. Memory Rules

The Avatar Engine runs on ESP32-S3.

Avoid per-frame:

```cpp
malloc()
new
std::vector growth
String concatenation
dynamic SVG parsing
large temporary buffers
```

Prefer:

```cpp
static arrays
fixed-size structures
precomputed LUT
stack-local small objects
```

The 64-point profile is intentionally small enough for MCU use.

---

# 20. Performance Optimization

If rendering becomes expensive, optimize in this order:

### Level 1

Reduce FPS:

```text
60 → 30 FPS
```

### Level 2

Use lower vector quality.

### Level 3

Precompute:

```text
sin()
cos()
```

using lookup tables.

### Level 4

Cache static geometry.

### Level 5

Reduce the number of LVGL vector operations.

Do not prematurely optimize the animation mathematics.

---

# 21. AI Device State Machine

For an AI keyboard/device, use this recommended flow:

```text
                ┌─────────────┐
                │    IDLE     │
                └──────┬──────┘
                       │
                  User input
                       │
                       ▼
                ┌─────────────┐
                │ LISTENING   │
                └──────┬──────┘
                       │
                  Speech done
                       │
                       ▼
                ┌─────────────┐
                │  THINKING   │
                └──────┬──────┘
                       │
                   TTS start
                       │
                       ▼
                ┌─────────────┐
                │  SPEAKING   │
                └──────┬──────┘
                       │
                   TTS done
                       │
                       ▼
                ┌─────────────┐
                │    IDLE     │
                └─────────────┘
```

Error:

```text
Any State
    ↓
 ERROR
    ↓
 short animation
    ↓
 IDLE
```

Sleep:

```text
IDLE
 ↓
SLEEP
 ↓
WAKE
 ↓
IDLE
```

---

# 22. Recommended Keyboard Integration

The physical device contains:

```text
9 buttons
1 rotary encoder
microphone
speaker
428×124 LCD
ESP32-S3
```

Recommended interaction:

```text
Rotary encoder
 ├── Rotate → UI selection
 └── Press  → confirm

Button
 ├── AI query
 ├── microphone
 ├── cancel
 └── custom action
```

Avatar response:

```text
Button pressed
      ↓
Wink / Attention
      ↓
Listening
      ↓
Thinking
      ↓
Speaking
```

This creates a much stronger physical-device experience than a static UI.

---

# 23. Example AI Integration

```cpp
void onUserPressedAI()
{
    avatar.event(
        AvatarEvent::StartListening
    );

    microphone.start();
}
```

When speech recognition finishes:

```cpp
void onSpeechFinished()
{
    microphone.stop();

    avatar.event(
        AvatarEvent::StartThinking
    );

    sendToLLM();
}
```

When TTS starts:

```cpp
void onTTSStart()
{
    avatar.event(
        AvatarEvent::StartSpeaking
    );
}
```

During playback:

```cpp
void onAudioLevel(float level)
{
    avatar.setSpeakingLevel(level);
}
```

When playback finishes:

```cpp
void onTTSFinished()
{
    avatar.event(
        AvatarEvent::WakeUp
    );

    avatar.setSpeakingLevel(0.0f);
}
```

---

# 24. Error Handling

When an AI request fails:

```cpp
avatar.event(
    AvatarEvent::Error
);
```

The Avatar should communicate failure visually.

Do not flash the entire screen.

Recommended:

```text
small body reaction
+
eye expression
+
short shake
+
return to idle
```

---

# 25. Success Feedback

After an operation succeeds:

```cpp
avatar.event(
    AvatarEvent::Success
);
```

Use:

```text
slight eye expansion
+
small body reaction
+
brief accent
+
return to idle
```

Keep it short.

---

# 26. Visual Design Rules

The Avatar should follow these rules.

### Rule 1

Do not overanimate.

### Rule 2

Eyes are more important than body movement.

### Rule 3

Idle should be subtle.

### Rule 4

State transitions should be smooth.

### Rule 5

Never abruptly replace geometry.

### Rule 6

Do not use large amounts of text around the Avatar.

### Rule 7

The Avatar should remain recognizable in every state.

### Rule 8

Avoid excessive particle effects.

### Rule 9

Do not make every action produce a dramatic animation.

### Rule 10

The device should feel like it has a personality.

---

# 27. Anti-Patterns

Avoid:

```cpp
lv_anim_t
```

for every individual geometry parameter when the engine can calculate the animation centrally.

Avoid:

```cpp
lv_obj_create()
```

for every eye/body component.

Avoid constructing dozens of LVGL objects.

Prefer one Avatar widget with internal rendering.

Avoid GIF/APNG frame animation.

Avoid large sprite sheets.

Avoid continuously changing random values.

Bad:

```cpp
x = random(-20, 20);
```

Good:

```cpp
x = noise(time);
```

---

# 28. Adding a New State

To add a state:

### Step 1

Add enum:

```cpp
enum class AvatarState : uint8_t
{
    Idle,
    Thinking,
    MyNewState
};
```

### Step 2

Add profile:

```cpp
ShapeLibrary::profileFor(...)
```

### Step 3

Add animation behaviour:

```cpp
sampleState(...)
```

### Step 4

Define eye configuration.

### Step 5

Define transition duration.

### Step 6

Test at:

```text
428 × 124
```

Do not optimize for desktop first.

---

# 29. Debugging

If the Avatar looks polygonal:

```text
Check:
- radial sample count
- path smoothing
- vector quality
```

If eyes look flat:

```text
Check:
- gaze projection
- depth scaling
- eye rotation
```

If Avatar looks dead:

```text
Check:
- blink
- gaze drift
- breathing
- micro movement
```

If Avatar looks nervous:

```text
Reduce:
- body movement
- gaze frequency
- blink frequency
- animation amplitude
```

If rendering is slow:

```text
Check:
- vector path count
- FPS
- allocations
- sin/cos calls
```

---

# 30. Visual Quality Priority

When implementing new features, prioritize:

```text
1. Eye quality
2. Blink
3. Gaze
4. Body silhouette
5. Morph transition
6. Idle motion
7. Expression
8. Decorative effects
```

Do not prioritize particle effects before eye behaviour is correct.

---

# 31. Recommended Development Sequence

For major changes use:

```text
Phase 1
Blob silhouette
     ↓
Phase 2
Eye geometry
     ↓
Phase 3
Gaze
     ↓
Phase 4
Blink
     ↓
Phase 5
Idle liveliness
     ↓
Phase 6
State morphing
     ↓
Phase 7
Speaking
     ↓
Phase 8
Microphone integration
     ↓
Phase 9
AI integration
```

The first five phases should be visually polished before adding complicated AI features.

---

# 32. Current EAvatar API

Current public API:

```cpp
class EAvatar
{
public:

    lv_obj_t* begin(
        lv_obj_t* parent,
        int32_t width,
        int32_t height
    );

    void update();

    void invalidate();

    void event(
        AvatarEvent event
    );

    void setState(
        AvatarState state
    );

    void setExpression(
        AvatarExpression expression
    );

    void setGaze(
        float x,
        float y
    );

    void setSpeakingLevel(
        float level
    );

    AvatarEngine& engine();
};
```

---

# 33. Example Complete Application

```cpp
#include <Arduino.h>
#include <lvgl.h>

#include "avatar/eavatar.h"

using namespace eavatar;

EAvatar avatar;

void setup()
{
    Serial.begin(115200);

    lv_init();

    // Initialize ESP32-S3 LCD here.

    avatar.begin(
        lv_screen_active(),
        428,
        124
    );

    avatar.event(
        AvatarEvent::WakeUp
    );
}

void loop()
{
    lv_timer_handler();

    // Example application logic.

    delay(5);
}
```

---

# 34. AI Assistant Instructions

When an AI agent is asked to modify EAvatar-LVGL:

1. Inspect the existing Avatar API before changing it.
2. Preserve Core/Renderer separation.
3. Do not introduce dynamic allocation into per-frame animation.
4. Prefer semantic events over direct geometry manipulation.
5. Preserve the 428×124 target.
6. Preserve 30 FPS as the baseline.
7. Prefer smooth interpolation over abrupt state changes.
8. Treat eyes as independent geometry.
9. Keep idle animation subtle.
10. Do not copy Bloub source code or assets.
11. Do not replace the parametric system with bitmap animation unless explicitly requested.
12. Do not add large LVGL object trees.
13. Test state transitions after modifying animation code.
14. Keep public API backwards compatible where practical.

---

# 35. Definition of Done

A feature is considered complete when:

```text
✓ Compiles on ESP32-S3
✓ Works with LVGL 9.x
✓ Works at 428×124
✓ Does not require a browser
✓ Does not require SVG parsing
✓ Does not allocate continuously
✓ Maintains smooth animation
✓ Does not destroy idle liveliness
✓ Eye movement remains natural
✓ State transitions are interpolated
✓ API remains understandable
```

The ultimate goal is not to reproduce a web page.

The goal is:

> Build a small embedded Avatar Engine that captures the visual and behavioural qualities of modern parametric blob avatars while being practical to run on ESP32-S3.