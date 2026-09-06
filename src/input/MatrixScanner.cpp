/*
 * MatrixScanner.cpp
 *
 * 矩阵扫描实现（FEATURE_DOC §2.1）：
 *
 *   行引脚（输出，扫描驱动）：{46, 39, 38}
 *   列引脚（输入，内部上拉）：{16, 17, 18, 8}
 *
 * 扫描方式：依次将每个行拉低，读 4 个列脚的电平。
 * 每列 LOW 表示该位置的按键被按下。
 *
 * 二极管方向：每个按键串有二极管，正极在列侧（电流只能 列→行）。
 * 因此行脚做驱动拉低、列脚做上拉读取：按下时电流从列上拉经二极管、
 * 开关流向被拉低的行脚，列脚被拉到约一个二极管压降（< VIL）读为 LOW。
 * 若方向配反（行→列），所有按键都会读不到——2026-09 实测踩坑记录。
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
        raw_[i] = false;
    }
}

void MatrixScanner::begin()
{
    for (uint8_t r = 0; r < kMatrixRowCount; ++r) {
        pinMode(kRowPins[r], OUTPUT);
        digitalWrite(kRowPins[r], HIGH);
    }
    for (uint8_t c = 0; c < kMatrixColCount; ++c) {
        pinMode(kColPins[c], INPUT_PULLUP);
    }
}

void MatrixScanner::setDebugSlowScan(bool slow)
{
    debug_slow_scan_ = slow;
}

bool MatrixScanner::readMatrixCell(uint8_t row, uint8_t col) const
{
    digitalWrite(kRowPins[row], LOW);
    if (debug_slow_scan_) {
        delay(300); // 调试探针：让万用表能看到行脉冲与按键拉低
    } else {
        delayMicroseconds(5);
    }
    bool pressed = (digitalRead(kColPins[col]) == LOW);
    digitalWrite(kRowPins[row], HIGH);
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
        raw_[keyId] = pressed_now;

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

bool MatrixScanner::getRawState(uint8_t keyId) const
{
    if (keyId < 1 || keyId > kMatrixKeyCount) {
        return false;
    }
    return raw_[keyId];
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
