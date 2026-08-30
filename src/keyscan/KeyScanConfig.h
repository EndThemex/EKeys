#pragma once
#include <Arduino.h>

namespace ekeys {

// 矩阵规模：EKeys 用 3×4 布局（FunModularKeyboard 是 5×4，本项目只用前 3 行）
constexpr uint8_t KEY_MATRIX_ROWS = 3;
constexpr uint8_t KEY_MATRIX_COLS = 4;
constexpr uint8_t KEY_NUM         = KEY_MATRIX_ROWS * KEY_MATRIX_COLS;

// 消抖 / 调度
constexpr uint8_t DEBOUNCE_MS      = 5;
constexpr uint8_t SCAN_INTERVAL_MS = 1;
constexpr uint8_t ROW_SETTLE_US    = 5;

// 编码器 / 特殊
constexpr uint8_t ROTARY_KEY_ID    = 0xFE;
constexpr uint8_t INVALID_KEY_ID   = 0xFF;

// 一次性配置：行/列 GPIO、编码器、键位映射
// 引脚核对自 FunModularKeyboard/src/MatrixScanner.h
struct KeyScanConfig {
    // 矩阵行（输出）：扫描时拉低
    // 取自 FunModularKeyboard 的前 3 行（IO14/IO33 在 EKeys 未使用）
    const uint8_t rowPins[KEY_MATRIX_ROWS] = { 48, 10, 47 };

    // 矩阵列（输入 + 上拉）
    const uint8_t colPins[KEY_MATRIX_COLS] = { 35, 34, 7, 13 };

    // EC11 编码器（与矩阵 GPIO 不冲突）
    // 引脚与 FunModularKeyboard/src/RotaryEncoder.h 完全一致
    const int8_t encoderAPin  = 5;    // ENCODER_CLK
    const int8_t encoderBPin  = 21;   // ENCODER_DT
    const int8_t encoderSWPin = 9;    // ENCODER_SW

    // keyId → (row, col) 映射；keyId=0 表示该物理位置无键
    // ROW0..ROW2 × COL0..COL3 = keyId 1..12 连续编号
    const uint8_t keyMap[KEY_MATRIX_ROWS][KEY_MATRIX_COLS] = {
        // COL0  COL1  COL2  COL3
        {   1,    2,    3,    4 },   // ROW0
        {   5,    6,    7,    8 },   // ROW1
        {   9,   10,   11,   12 },   // ROW2
    };
};

}  // namespace ekeys
