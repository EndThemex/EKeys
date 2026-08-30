# 对外接口契约与配置

> 文件：`src/keyscan/KeyScanConfig.h`

---

## 1. KeyScanConfig — 单一配置源

```cpp
// KeyScanConfig.h
#pragma once
#include <Arduino.h>

namespace ekeys {

constexpr uint8_t  KEY_NUM              = 11;
constexpr uint8_t  KEY_MATRIX_ROWS      = 3;
constexpr uint8_t  KEY_MATRIX_COLS      = 4;
constexpr uint8_t  DEBOUNCE_MS          = 5;
constexpr uint8_t  SCAN_INTERVAL_MS     = 1;
constexpr uint8_t  ROW_SETTLE_US        = 5;
constexpr uint8_t  ROTARY_KEY_ID        = 0xFE;
constexpr uint8_t  INVALID_KEY_ID       = 0xFF;

struct KeyScanConfig {
    // 矩阵行 GPIO（输出）
    const uint8_t rowPins[KEY_MATRIX_ROWS]   = { 9, 10, 11 };

    // 矩阵列 GPIO（输入 / 上拉）
    const uint8_t colPins[KEY_MATRIX_COLS]   = { 6,  7,  8, 12 };

    // EC11 编码器
    const uint8_t encoderAPin                 = 5;
    const uint8_t encoderBPin                 = 7;   // 复用 COL1
    const uint8_t encoderSWPin                = 8;   // 复用 COL2

    // keyId → (row, col) 映射；keyId=0 表示该物理位置无键
    //   顺序与 netlist 中 KEY1..KEY11 一致
    const uint8_t keyMap[KEY_MATRIX_ROWS][KEY_MATRIX_COLS] = {
        // COL0  COL1  COL2  COL3
        {   1,    2,    3,   0 },   // ROW0
        {   4,    5,    6,   7 },   // ROW1
        {   8,    9,   10,  11 },   // ROW2
    };
};

// 内部使用：keyId → (row, col) 反查
inline void keyIdToRC(const KeyScanConfig& cfg, uint8_t keyId,
                      uint8_t& row, uint8_t& col) {
    for (uint8_t r = 0; r < KEY_MATRIX_ROWS; ++r) {
        for (uint8_t c = 0; c < KEY_MATRIX_COLS; ++c) {
            if (cfg.keyMap[r][c] == keyId) {
                row = r; col = c; return;
            }
        }
    }
    row = INVALID_KEY_ID; col = INVALID_KEY_ID;
}

}  // namespace ekeys
```

---

## 2. 跨任务消息契约

`KeyEvent` 通过 `QueueHandle_t` 传递，**必须是值类型**，不能含指针 / String / 引用：

```cpp
struct KeyEvent {
    KeyEventType type;          // 1 byte
    uint8_t      keyId;         // 1 byte
    int8_t       encoderDelta;  // 1 byte (仅 EncoderRotate)
    uint32_t     timestamp_ms;  // 4 bytes
    // 显式 padding + total 8 bytes
};
static_assert(sizeof(KeyEvent) == 8, "KeyEvent ABI 漂移会破坏队列");
```

队列容量：

```cpp
constexpr UBaseType_t KEY_EVENT_QUEUE_LEN = 32;
QueueHandle_t q = xQueueCreate(KEY_EVENT_QUEUE_LEN, sizeof(KeyEvent));
```

> 32 是经验值：12 键 + 12 释放 + 编码器若干事件 ≈ 28，留 4 余量。

---

## 3. 订阅者契约

### 3.1 回调订阅

```cpp
using EventCallback = std::function<void(const KeyEvent&)>;
void subscribe(uint8_t typeMask, EventCallback cb);
```

- `typeMask` 是 `KeyEventType` 的**位掩码**（注意 enum 用 uint8_t 强转）
- 回调在 **KeyScanManager 任务上下文** 调用，禁止 `delay()`、`vTaskDelay()`、长时间阻塞
- 回调内如需发消息给其它任务，请使用 `xQueueSend(..., 0)`

### 3.2 队列订阅

```cpp
void attachQueue(QueueHandle_t queue);
```

- 调用一次绑定一个队列
- 队列满 → 丢弃 + WARN，**不阻塞扫描任务**

---

## 4. ABI 稳定性

- `KeyEvent` 结构一旦发布，**只允许尾部追加字段**，且要做 `static_assert(sizeof)` 守住布局
- `KeyScanConfig` 是 `constexpr` 常量，编译期可被优化；运行时不允许修改

---

## 5. 错误码与日志约定

| Tag | 级别 | 触发 |
|---|---|---|
| `KeyScan` | INFO | `begin()` 完成、任务启动 |
| `KeyScan` | WARNING | 队列满、回调耗时过长、`KeyEventList` reserve 扩容 |
| `KeyScan` | ERROR | GPIO 配置失败、`xTaskCreate` 失败 |

> 日志使用现有 `LogManager`（如有）；本项目当前无 `LogManager`，可临时用 `Serial.printf`，第二阶段统一替换。
