#include "KeyEvent.h"

namespace ekeys {

String KeyEvent::toString() const {
    char buf[64];
    const char* typeStr = "?";
    switch (type) {
        case KeyEventType::Press:         typeStr = "PRESS"; break;
        case KeyEventType::Release:       typeStr = "RELEASE"; break;
        case KeyEventType::EncoderRotate: typeStr = "ENC_ROT"; break;
        case KeyEventType::EncoderClick:  typeStr = "ENC_CLK"; break;
    }
    if (type == KeyEventType::EncoderRotate) {
        snprintf(buf, sizeof(buf), "[%lu ms] %s delta=%d",
                 (unsigned long)timestamp_ms, typeStr, encoderDelta);
    } else {
        snprintf(buf, sizeof(buf), "[%lu ms] %s key=%u",
                 (unsigned long)timestamp_ms, typeStr, keyId);
    }
    return String(buf);
}

}  // namespace ekeys
