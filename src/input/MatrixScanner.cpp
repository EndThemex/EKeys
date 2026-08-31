/*
 * MatrixScanner.cpp
 *
 * 矩阵扫描实现（FEATURE_DOC §2.1）：
 *
 *   行引脚（输入，内部上拉）：{46, 39, 38}
 *   列引脚（输出）：{16, 17, 18, 8}
 *
 * 扫描方式：依次将每个列拉低，读 3 个行脚的电平。
 * 每行 LOW 表示该位置的按键被按下。
 */

#include "MatrixScanner.h"

#include "hardware/PinMap.h"

namespace ekeys {

namespace {

constexpr uint8_t kRowPins[kMatrixRowCount] = {
    kPinMatrixRow0, kPinMatrixRow1, kPinMatrixRow2
};
constexpr uint8_t kColPins[kMatrixColCount] = {
    kPinMatrixCol0, kPinMatrixCol1, kPinMatrixCol2, kPinMatrixCol3
};

}  // namespace

MatrixScanner::MatrixScanner()
    : pressedCount_(0), releasedCount_(0)
{
    for (uint8_t i = 0; i <= kMatrixKeyCount; ++i) {
        states_[i].phase = MatrixKeyState::Phase::Idle;
        states_[i].stable_pressed = false;
        states_[i].phase_started_ms = 0;
    }
}

void MatrixScanner::begin()
{
    for (uint8_t r = 0; r < kMatrixRowCount; ++r) {
        pinMode(kRowPins[r], INPUT_PULLUP);
    }
    for (uint8_t c = 0; c < kMatrixColCount; ++c) {
        pinMode(kColPins[c], OUTPUT);
        digitalWrite(kColPins[c], HIGH);
    }
}

bool MatrixScanner::readMatrixCell(uint8_t row, uint8_t col) const
{
    digitalWrite(kColPins[col], LOW);
    delayMicroseconds(5);
    bool pressed = (digitalRead(kRowPins[row]) == LOW);
    digitalWrite(kColPins[col], HIGH);
    return pressed;
}

void MatrixScanner::keyIdToRowCol(uint8_t keyId, uint8_t &row, uint8_t &col)
{
    if (keyId < 1 || keyId > kMatrixKeyCount) {
        row = 0; col = 0;
        return;
    }
    if (keyId <= 3) {
        row = 0;
        col = keyId - 1;
    } else if (keyId <= 7) {
        row = 1;
        col = keyId - 4;
    } else {
        row = 2;
        col = keyId - 8;
    }
}

void MatrixScanner::dispatchEdge(uint8_t keyId, bool pressed)
{
    MatrixKeyState &s = states_[keyId];
    s.stable_pressed = pressed;
    if (pressed) {
        if (pressedCount_ < kMatrixKeyCount) {
            pressedKeys_[pressedCount_++] = keyId;
        }
    } else {
        if (releasedCount_ < kMatrixKeyCount) {
            releasedKeys_[releasedCount_++] = keyId;
        }
    }
}

void MatrixScanner::scan()
{
    uint32_t now = millis();
    pressedCount_ = 0;
    releasedCount_ = 0;

    for (uint8_t keyId = 1; keyId <= kMatrixKeyCount; ++keyId) {
        uint8_t row;
        uint8_t col;
        keyIdToRowCol(keyId, row, col);
        bool pressed_now = readMatrixCell(row, col);

        MatrixKeyState &s = states_[keyId];
        switch (s.phase) {
            case MatrixKeyState::Phase::Idle: {
                if (pressed_now) {
                    s.phase = MatrixKeyState::Phase::DebouncePress;
                    s.phase_started_ms = now;
                }
            } break;

            case MatrixKeyState::Phase::DebouncePress: {
                if (!pressed_now) {
                    s.phase = MatrixKeyState::Phase::Idle;
                } else if ((now - s.phase_started_ms) >= kDebounceTimeMs) {
                    s.phase = MatrixKeyState::Phase::Pressed;
                    dispatchEdge(keyId, true);
                }
            } break;

            case MatrixKeyState::Phase::Pressed: {
                if (!pressed_now) {
                    s.phase = MatrixKeyState::Phase::DebounceRelease;
                    s.phase_started_ms = now;
                }
            } break;

            case MatrixKeyState::Phase::DebounceRelease: {
                if (pressed_now) {
                    s.phase = MatrixKeyState::Phase::Pressed;
                } else if ((now - s.phase_started_ms) >= kDebounceTimeMs) {
                    s.phase = MatrixKeyState::Phase::Idle;
                    dispatchEdge(keyId, false);
                }
            } break;
        }
    }
}

bool MatrixScanner::getStableState(uint8_t keyId) const
{
    if (keyId < 1 || keyId > kMatrixKeyCount) {
        return false;
    }
    return states_[keyId].stable_pressed;
}

void MatrixScanner::getPressedKeys(uint8_t out[], uint8_t &count) const
{
    count = 0;
    for (uint8_t i = 0; i < pressedCount_; ++i) {
        out[count++] = pressedKeys_[i];
    }
}

void MatrixScanner::getReleasedKeys(uint8_t out[], uint8_t &count) const
{
    count = 0;
    for (uint8_t i = 0; i < releasedCount_; ++i) {
        out[count++] = releasedKeys_[i];
    }
}

}  // namespace ekeys
