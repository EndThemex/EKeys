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

EC11 在每次「咔哒」时产生一个完整四步序列：

```
RA  ─┐ ┌──┐ ┌──┐ ┌──
     └─┘  └─┘  └─┘
RB  ───┐ ┌──┐ ┌──┐ ┌
       └─┘  └─┘  └─┘
```

读取策略：**双信号边沿触发**，每对边沿计 1 格。
- 上升沿 A 且 B=0 → 顺时针 +1
- 下降沿 A 且 B=1 → 顺时针 +1
- 上升沿 A 且 B=1 → 逆时针 +1
- 下降沿 A 且 B=0 → 逆时针 +1
（Gray 编码的标准解码）

按键 SW（OneButton 模式）：
- 单击：单击事件
- 双击：可选，本期不实现
- 长按 ≥ 500 ms：可选，本期不实现

---

## 3. 关键 API

```cpp
class RotaryEncoder : public IKeySource {
public:
    struct Callbacks {
        std::function<void(int8_t delta)>    onRotate; // delta = ±1
        std::function<void()>                onClick;
    };

    explicit RotaryEncoder(const KeyScanConfig& cfg);

    // IKeySource
    void begin() override;                  // 配置 GPIO + 中断
    void poll(KeyEventList& out) override;  // 在 MatrixScanner 之后调用
};
```

事件结构（统一用 `KeyEvent`）：

```cpp
enum class KeyEventType { Press, Release, EncoderRotate, EncoderClick };

struct KeyEvent {
    KeyEventType type;
    uint8_t      keyId;       // 1..11 或 ROTARY_KEY_ID
    int8_t       delta;       // 仅 EncoderRotate 用
    uint32_t     timestamp_ms;
};
```

> `ROTARY_KEY_ID = 0xFE`，避免与 1..11 物理键冲突。

---

## 4. 消抖

- 旋转：硬件 RC + 软件 1 ms 间隔足够；`MatrixScanner` 调度周期 1 ms = 天然节流
- 按键：用 `Bounce2` 或手写 5 ms 消抖（不引入 OneButton 减少依赖）

---

## 5. 与矩阵扫描的互斥

`KeyScanManager` 主循环顺序：

```
1. matrix.poll(out)         // 占用 COL0~COL3 作为输入
2. encoder.poll(out)        // COL1/COL2 此刻已被释放为高阻
3. dispatcher.dispatch(out) // 把这一轮的事件派发出去
4. vTaskDelay(pdMS(1))      // 给 EC11 物理电容充放电
```

矩阵扫描器在每次扫描完后**主动 `pinMode(COL1/COL2, INPUT)`** 还原为高阻。
此窗口内 EC11 可以安全读取引脚。

---

## 6. 测试钩子

- 不接编码器时（GPIO 高/低稳定），`poll()` 不产生任何事件。
- 强制事件注入：保留 `KeyEventDispatcher::inject()` 调试接口（仅在 `DEBUG_KEYSCAN` 宏打开时编译）。
