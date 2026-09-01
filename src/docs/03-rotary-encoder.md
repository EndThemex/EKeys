# RotaryEncoder — EC11 旋转编码器扫描

> 文件：`src/keyscan/RotaryEncoder.h` / `src/keyscan/RotaryEncoder.cpp`

---

## 1. 硬件

EC11(U8) 连接到：

| 信号 | 引脚 | 备注 |
|---|---|---|
| CLK (A 相) | GPIO5 | ENCODER_CLK |
| DT  (B 相) | GPIO21 | ENCODER_DT |
| SW  (按键) | GPIO9 | ENCODER_SW |

> 编码器 GPIO 与矩阵 GPIO **完全无冲突**，可独立处理。
> 引脚定义参考 [FunModularKeyboard/src/RotaryEncoder.h](file:///D:/workspace/zheteng/ESP_Projects/FunModularKeyboard/src/RotaryEncoder.h)。

---

## 2. 触发策略

旋转：用 **ESP32Encoder（PCNT 硬件计数器）半正交模式**，每对 A/B 边沿计 1 格，
并提供 1023 大小的数字滤波。一步变化对应 1 个 `EncoderRotate` 事件。

按键：用 **OneButton** 库识别单击 / 双击 / 长按。

| 按键动作 | encoderDelta | UI 显示 | 用途 |
|---|---|---|---|
| 单击 | 1 | `ENC: click` | **进入/确认**（路由到当前页 `onConfirm()`，与 KEY2 同义） |
| 双击 | 2 | `ENC: double` | 暂未路由到 UI（仅 BLE 上报 Esc） |
| 长按起始 | 3 | `ENC: long` | 暂未路由到 UI（仅 BLE 上报 Tab） |

---

## 3. 关键 API

```cpp
class RotaryEncoder : public IKeySource {
public:
    RotaryEncoder() = default;

    // IKeySource
    void begin() override;                  // 配置 PCNT + OneButton
    void poll(KeyEventList& out) override;  // tick + 取出累计事件

    // OneButton tick —— 由 poll() 内部调用，也可单独调用以让 click 计时生效
    void tick();
};
```

事件结构（统一用 `KeyEvent`）：

```cpp
enum class KeyEventType {
    Press, Release,
    EncoderRotate,   // encoderDelta = ±1
    EncoderClick,    // encoderDelta = 1 (click) / 2 (double) / 3 (long)
};

struct KeyEvent {
    KeyEventType type;
    uint8_t      keyId;       // ROTARY_KEY_ID (= 0xFE) 区分于 1..12 物理键
    int8_t       encoderDelta;
    uint32_t     timestamp_ms;
};
```

---

## 4. 互斥与同步

- `RotaryEncoder` 的 A/B 引脚 (5, 21) 与矩阵 GPIO 完全不交集；
  矩阵扫描时不需要释放任何编码器引脚。
- 按钮事件通过 `volatile bool g_clickFlag / g_doubleFlag / g_longPressFlag`
  在 OneButton 回调（task 上下文）置位，在 `poll()`（scan task 上下文）读取。
  因为三个标志位互斥，bool 读/写就是原子操作，无需额外同步原语。

---

## 5. 调试

- `Serial.println("[Encoder] click")` 等仅用于硬件验证。
- 后续若需要清屏时，可在 loop 中加一个超时清除（例如 500 ms 无事件后恢复 `--`）。
