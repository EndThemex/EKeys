#pragma once
#include <Arduino.h>

namespace ekeys
{

    // 矩阵规模：EKeys 用 3×4 布局（FunModularKeyboard 是 5×4，本项目只用前 3 行）
    constexpr uint8_t KEY_MATRIX_ROWS = 3;
    constexpr uint8_t KEY_MATRIX_COLS = 4;
    constexpr uint8_t KEY_NUM = KEY_MATRIX_ROWS * KEY_MATRIX_COLS;

    // 消抖 / 调度
    constexpr uint8_t DEBOUNCE_MS = 5;
    constexpr uint8_t SCAN_INTERVAL_MS = 1;
    constexpr uint8_t ROW_SETTLE_US = 5;

    /* ---- 旋钮滤波参数 ----
     *
     * 旋钮抖动由两个独立源头叠加：
     *   (a) 接触抖动：PCNT 计数器在静止时偶尔 ±1 跳动（短尖峰，<1ms）。
     *   (b) 慢速来回：人手在临界档位上前后微动 → PCNT 累计 ±1 互相抵消。
     *
     * 默认值针对 EC11 + 240MHz + 1ms tick 调过，覆盖大多数手感区间。
     * 如果换编码器或更换主频，需要按下面方法重测：
     *   1) 把 EKEYS_DEBUG_DIAG=1 编译运行；
     *   2) 让 Serial 输出 PCNT 原始值（暂时取消 setFilter 屏蔽）；
     *   3) 看静止时的最大抖动幅度 → 调 ENC_BURST_THRESHOLD；
     *   4) 看慢转 1 步需要的最小时间 → 调 ENC_MIN_STEP_MS。 */
    constexpr uint8_t ENC_BURST_THRESHOLD = 2; // 同向连击 ≥ 此值才视为"真旋转"
    constexpr uint16_t ENC_MIN_STEP_MS = 8;    // 任意两次旋转事件间隔 < 此值视为同一动作（合并）
    constexpr uint16_t ENC_QUIET_MS = 6;       // 反向抖动静默期（>DEBOUNCE_MS，覆盖慢来回）

    // 编码器 / 特殊
    constexpr uint8_t ROTARY_KEY_ID = 0xFE;
    constexpr uint8_t INVALID_KEY_ID = 0xFF;

    // 一次性配置：行/列 GPIO、编码器、键位映射
    // 引脚核对自 FunModularKeyboard/src/MatrixScanner.h
    struct KeyScanConfig
    {
        // 矩阵行（输出）：扫描时拉低
        // 取自 FunModularKeyboard 的前 3 行（IO14/IO33 在 EKeys 未使用）
        const uint8_t rowPins[KEY_MATRIX_ROWS] = {48, 10, 47};

        // 矩阵列（输入 + 上拉）
        const uint8_t colPins[KEY_MATRIX_COLS] = {35, 34, 7, 13};

        // EC11 编码器（与矩阵 GPIO 不冲突）
        // 引脚与 FunModularKeyboard/src/RotaryEncoder.h 完全一致
        const int8_t encoderAPin = 5;  // ENCODER_CLK
        const int8_t encoderBPin = 21; // ENCODER_DT
        const int8_t encoderSWPin = 9; // ENCODER_SW

        // keyId → (row, col) 映射；keyId=0 表示该物理位置无键
        // 实际硬件布局（梯形）：ROW0=2 键、ROW1=3 键、ROW2=4 键
        // keyId 连续 1..9，未用位置填 0 跳过扫描。
        const uint8_t keyMap[KEY_MATRIX_ROWS][KEY_MATRIX_COLS] = {
            // COL0  COL1  COL2  COL3
            {1, 2, 0, 0}, // ROW0: 2 keys (COL0, COL1)
            {0, 3, 4, 5}, // ROW1: 3 keys (COL1, COL2, COL3)
            {6, 7, 8, 9}, // ROW2: 4 keys (all COLs)
        };
    };

} // namespace ekeys
