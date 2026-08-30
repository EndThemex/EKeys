#include "MatrixScanner.h"

namespace ekeys {

MatrixScanner::MatrixScanner(const KeyScanConfig& cfg) : cfg_(cfg) {}

void MatrixScanner::initGpio() {
    // 列：输入 + 上拉
    for (uint8_t c = 0; c < KEY_MATRIX_COLS; ++c) {
        pinMode(cfg_.colPins[c], INPUT_PULLUP);
    }
    // 行：默认输入（高阻），扫描时切为输出拉低
    for (uint8_t r = 0; r < KEY_MATRIX_ROWS; ++r) {
        pinMode(cfg_.rowPins[r], INPUT);
    }
}

void MatrixScanner::begin() {
    initGpio();
}

void MatrixScanner::driveRowLow(uint8_t row) {
    const uint8_t pin = cfg_.rowPins[row];
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void MatrixScanner::releaseRow(uint8_t row) {
    const uint8_t pin = cfg_.rowPins[row];
    pinMode(pin, INPUT);  // 高阻，释放给其它外设（如 EC11）
}

void MatrixScanner::releaseAllRows() {
    for (uint8_t r = 0; r < KEY_MATRIX_ROWS; ++r) {
        releaseRow(r);
    }
}

void MatrixScanner::setStableBit(uint8_t keyId, bool pressed) {
    if (keyId == 0 || keyId > KEY_NUM) return;
    const uint32_t bit = 1UL << (keyId - 1);
    if (pressed) stableMask_ |= bit;
    else         stableMask_ &= ~bit;
}

bool MatrixScanner::isPressed(uint8_t keyId) const {
    if (keyId == 0 || keyId > KEY_NUM) return false;
    return (stableMask_ & (1UL << (keyId - 1))) != 0;
}

void MatrixScanner::advance(uint8_t keyId, bool rawPressed, uint32_t nowMs,
                            KeyEventList& out) {
    auto& s = states_[keyId - 1];
    switch (s.state) {
        case State::IDLE:
            if (rawPressed) { s.state = State::DEBOUNCE_PRESS; s.enterMs = nowMs; }
            break;
        case State::DEBOUNCE_PRESS:
            if (!rawPressed) {
                s.state = State::IDLE;
            } else if (nowMs - s.enterMs >= DEBOUNCE_MS) {
                s.state = State::PRESSED;
                setStableBit(keyId, true);
                out.push_back({KeyEventType::Press, keyId, 0, nowMs});
            }
            break;
        case State::PRESSED:
            if (!rawPressed) { s.state = State::DEBOUNCE_RELEASE; s.enterMs = nowMs; }
            break;
        case State::DEBOUNCE_RELEASE:
            if (rawPressed) {
                s.state = State::PRESSED;
                s.enterMs = nowMs;
            } else if (nowMs - s.enterMs >= DEBOUNCE_MS) {
                s.state = State::IDLE;
                setStableBit(keyId, false);
                out.push_back({KeyEventType::Release, keyId, 0, nowMs});
            }
            break;
    }
}

void MatrixScanner::poll(KeyEventList& out) {
    const uint32_t now = millis();

    // 诊断：每 500ms 输出一帧"每个 ROW 拉低后各 COL 的原始电平"。
    // 用来区分"行没拉低"和"列没接通"。节流避免淹没串口。
    static uint32_t lastDbgMs = 0;
    bool dbgThisFrame = (now - lastDbgMs) >= 500;
    if (dbgThisFrame) lastDbgMs = now;

    for (uint8_t r = 0; r < KEY_MATRIX_ROWS; ++r) {
        driveRowLow(r);
        delayMicroseconds(ROW_SETTLE_US);

        if (dbgThisFrame) {
            char line[80];
            int n = snprintf(line, sizeof(line), "[MatrixDiag] ROW%d(pin=%d) COL=",
                             r, cfg_.rowPins[r]);
            for (uint8_t c = 0; c < KEY_MATRIX_COLS; ++c) {
                n += snprintf(line + n, sizeof(line) - n, "%d:%d ",
                              cfg_.colPins[c], digitalRead(cfg_.colPins[c]));
            }
            snprintf(line + n, sizeof(line) - n, "\n");
            Serial.print(line);
        }

        for (uint8_t c = 0; c < KEY_MATRIX_COLS; ++c) {
            const uint8_t keyId = cfg_.keyMap[r][c];
            if (keyId == 0) continue;
            const bool pressed = (digitalRead(cfg_.colPins[c]) == LOW);
            advance(keyId, pressed, now, out);
        }

        releaseRow(r);
    }

    // 关键：扫描完所有行后释放为高阻，让 EC11 / 其它外设能正常读 COL
    releaseAllRows();
}

}  // namespace ekeys
