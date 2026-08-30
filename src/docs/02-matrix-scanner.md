# MatrixScanner — 键盘矩阵扫描与消抖

> 文件：`src/keyscan/MatrixScanner.h` / `src/keyscan/MatrixScanner.cpp`
> 依赖：无（仅 Arduino.h + freertos 时间 API）

---

## 1. 硬件拓扑（引脚核对自 FunModularKeyboard）

```
         COL0(GPIO35)  COL1(GPIO34)  COL2(GPIO7)  COL3(GPIO13)
         ──────────────────────────────────────────────────
ROW0(GPIO48) │ KEY1     KEY2        KEY3        KEY4
ROW1(GPIO10) │ KEY5     KEY6        KEY7        KEY8
ROW2(GPIO47) │ KEY9     KEY10       KEY11       KEY12
```

> 矩阵 GPIO 与 EC11 编码器（CLK=5, DT=21, SW=9）**完全无冲突**，可独立扫描。
> 后两行（IO33 / IO14）在 EKeys 项目中未使用；FunModularKeyboard 中分别用作第 4、5 行。

---

## 2. 扫描时序

| 阶段 | 动作 |
|---|---|
| 1 | 把所有 ROW 设为 `INPUT_PULLUP`（常态高 = 未按下） |
| 2 | 依次把每一行驱动为 `OUTPUT_LOW`，其它行保持 `INPUT_PULLUP` |
| 3 | 读 4 个 COL：低电平 = 该 (row, col) 键按下 |
| 4 | 切换下一行；轮询完所有 11 个键大概 11×短暂延时 ≈ < 1 ms |
| 5 | 所有行恢复 `INPUT_PULLUP`，让 EC11 等其它外设接管 |

> 频率：默认 1 ms / 次 tick（在 `KeyScanManager` 中调度），每次 tick 内：
> - 完整扫描一遍 3 行
> - 把每个键的当前电平喂进消抖状态机
> - 状态机判断后输出 0/1 = 稳定态

---

## 3. 消抖状态机

```
                  ┌───────────────┐
        ┌────────►│     IDLE      │  raw=1
        │         └───┬───────────┘
        │             │ raw=0 (首次检测到按下)
        │             ▼
        │         ┌───────────────┐
        │         │ DEBOUNCE_PRESS│  <5ms 持续 raw=0
        │         └───────┬───────┘
        │                 │ 持续 raw=0 ≥5ms
        │                 ▼
        │         ┌───────────────┐
        │         │   PRESSED     │  raw=1 (释放)
        │         └───────┬───────┘
        │                 ▼
        │         ┌───────────────┐
        └─────────┤DEBOUNCE_RELEASE│ <5ms 持续 raw=1
   raw=1 重新开始  └───────────────┘
```

每个按键独立维护一个 `KeyDebounceState`：
- `state`：`IDLE / DEBOUNCE_PRESS / PRESSED / DEBOUNCE_RELEASE`
- `enter_ms`：进入当前状态的时刻

**输出（stable_state_）**：
- 仅在 `state == PRESSED` 时为 1，否则 0

**事件（仅边沿触发）**：
- `state` 由 `DEBOUNCE_RELEASE → IDLE` 时上报 `RELEASE` 事件
- `state` 由 `DEBOUNCE_PRESS → PRESSED` 时上报 `PRESS` 事件
- 事件附带 `timestamp_ms = millis()`

---

## 4. 关键 API

```cpp
class MatrixScanner : public IKeySource {
public:
    explicit MatrixScanner(const KeyScanConfig& cfg);

    // IKeySource
    void begin() override;                  // 配置 GPIO
    void poll(KeyEventList& out) override;  // 单次扫描 + 消抖推进

    // 便捷查询
    uint32_t stableMask() const;           // 11-bit 稳定状态掩码
    bool     isPressed(uint8_t keyId) const;  // keyId = 1..11
};
```

- `keyId`：1-based，**与 netlist KEY1..KEY11 一一对应**，与列号无关。
- `keyId → (row, col)` 映射由 `KeyScanConfig.h` 中的 `kKeyMap[11][2]` 提供。

---

## 5. 状态机伪代码（核心）

```cpp
void MatrixScanner::poll(KeyEventList& out) {
    const uint32_t now = millis();
    for (uint8_t row = 0; row < 3; ++row) {
        driveRowLow(row);
        delayMicroseconds(5);               // 信号稳定
        for (uint8_t col = 0; col < 4; ++col) {
            const bool raw = (digitalRead(colPins_[col]) == LOW);
            const uint8_t keyId = keyMap_[row * 4 + col];
            if (keyId == 0) continue;       // 该物理位置无键
            advanceStateMachine(keyId, raw, now, out);
        }
        releaseRow(row);
    }
}

void MatrixScanner::advanceStateMachine(uint8_t keyId, bool raw,
                                        uint32_t now, KeyEventList& out) {
    auto& s = states_[keyId - 1];
    switch (s.state) {
        case IDLE:
            if (!raw) { s.state = DEBOUNCE_PRESS; s.enter_ms = now; }
            break;
        case DEBOUNCE_PRESS:
            if (raw) { s.state = IDLE; }
            else if (now - s.enter_ms >= DEBOUNCE_MS) {
                s.state = PRESSED;
                setStableBit(keyId, true);
                out.push_back({KeyEventType::Press, keyId, now});
            }
            break;
        case PRESSED:
            if (raw) { s.state = DEBOUNCE_RELEASE; s.enter_ms = now; }
            break;
        case DEBOUNCE_RELEASE:
            if (!raw) { s.state = PRESSED; s.enter_ms = now; }
            else if (now - s.enter_ms >= DEBOUNCE_MS) {
                s.state = IDLE;
                setStableBit(keyId, false);
                out.push_back({KeyEventType::Release, keyId, now});
            }
            break;
    }
}
```

---

## 6. 可调参数

| 参数 | 默认 | 位置 | 说明 |
|---|---|---|---|
| `DEBOUNCE_MS` | 5 | `KeyScanConfig.h` | 抖动窗口，机械轴 5ms 足够 |
| `SCAN_INTERVAL_MS` | 1 | `KeyScanManager` | 扫描调用周期 |
| `ROW_SETTLE_US` | 5 | `MatrixScanner.cpp` | 行拉低后稳定时间 |
| `KEY_NUM` | 11 | `KeyScanConfig.h` | 总键数，影响位掩码宽度 |

---

## 7. 与 FunModularKeyboard 的差异

| 项 | 旧实现 | 新实现 |
|---|---|---|
| 任务模型 | MainTask 里直接调用 `scanner_.scan()` | 独立 `KeyScanManager` 任务统一调度 |
| 接口 | 返回 `stable_state_` 位掩码 | 返回**事件流**，调用方拿到边沿 |
| GPIO 释放 | 不释放 | 扫描完所有行后释放，回退到高阻 |
| 编码器冲突 | 旋钮走独立 GPIO | 与 COL1/COL2 复用，扫描期间互斥 |
| 单元测试 | 难以隔离 | `IKeySource` 接口可注入 mock |
