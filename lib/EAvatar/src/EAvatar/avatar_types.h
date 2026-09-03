#pragma once
#include <stdint.h>

namespace eavatar {

enum class AvatarState : uint8_t {
    Idle, Thinking, Wink, Wide, Alert, Notify, Exclaim, Sleep,
    Egg, Hexagon, Play, Orbit, Burst, Comet
};

enum class AvatarExpression : uint8_t { Neutral, Attentive, Surprised, Happy, Angry, Sad, Confused, Sleepy };
enum class AvatarEvent : uint8_t { WakeUp, StartListening, StopListening, StartThinking, StartSpeaking, Success, Error, Sleep, UserInteraction };

struct EyeFrame {
    float width = 0.186f;   // body-radius units
    float height = 0.412f;
    float open = 1.0f;
    float tilt = 0.0f;
    float alpha = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
};

struct AvatarFrame {
    float body_scale = 1.0f;
    float body_rotation = 0.0f;
    float body_x = 0.0f;
    float body_y = 0.0f;
    float gaze_yaw = 28.49f;
    float gaze_pitch = 28.62f;
    float gaze_roll = -13.0f;
    float eye_split = 15.46f;
    EyeFrame eyes[2];
    float speaking_level = 0.0f;
    float glow = 0.0f;
    float shake_x = 0.0f;
    float alpha = 1.0f;
    float accent_x = 0.0f;
    float accent_y = 0.0f;
    float accent_alpha = 0.0f;
    float body_radius[64] = {};
};

}
