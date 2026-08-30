#pragma once
#include <Arduino.h>
#include <vector>
#include "KeyScanConfig.h"

namespace ekeys {

enum class KeyEventType : uint8_t {
    Press         = 1,
    Release       = 2,
    EncoderRotate = 3,
    EncoderClick  = 4,
};

struct KeyEvent {
    KeyEventType type;
    uint8_t      keyId;
    int8_t       encoderDelta;   // 仅 EncoderRotate
    uint32_t     timestamp_ms;

    String toString() const;
};

// 显式布局稳定（值类型，用于队列传递）
static_assert(sizeof(KeyEvent) == 8, "KeyEvent ABI must stay 8 bytes");

using KeyEventList = std::vector<KeyEvent>;

}  // namespace ekeys
