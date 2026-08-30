# KeyScanManager — 顶层扫描任务

> 文件：`src/keyscan/KeyScanManager.h` / `src/keyscan/KeyScanManager.cpp`
> 依赖：`IKeySource`、`KeyEventDispatcher`

---

## 1. 职责

`KeyScanManager` 是一个 FreeRTOS 任务，统一调度：

1. `MatrixScanner::poll()` — 矩阵键
2. `RotaryEncoder::poll()` — 编码器
3. 把事件交给 `KeyEventDispatcher::dispatch()`

它**不持有任何业务逻辑**（键映射、HID 输出、UI 联动都不归它管）。

---

## 2. 任务结构

```cpp
class KeyScanManager {
public:
    KeyScanManager(MatrixScanner& matrix,
                   RotaryEncoder& encoder,
                   KeyEventDispatcher& dispatcher);

    void begin();   // 创建 FreeRTOS 任务
    void stop();    // 删除任务（可选）

private:
    static void taskEntry(void* arg);
    void run();

    MatrixScanner&     matrix_;
    RotaryEncoder&     encoder_;
    KeyEventDispatcher& dispatcher_;
    TaskHandle_t       taskHandle_{nullptr};
};
```

`begin()` 内部：

```cpp
xTaskCreatePinnedToCore(
    &KeyScanManager::taskEntry,
    "KeyScan",
    4096,           // 栈深度（事件回调不大，4KB 足够）
    this,
    2,              // 优先级（比 MainTask 高，确保不丢事件）
    &taskHandle_,
    0               // Core 0（与 LVGL/Main 任务分开）
);
```

> **核心选择**：放 Core 0。LVGL 当前已经在默认核跑（详见 [main.cpp](file:///D:/workspace/zheteng/ESP_Projects/EKeys/src/main.cpp)）。
> 把扫描放到 Core 0，事件入队后再被 Core X 的 MainTask 消费，**避免扫描延迟影响 UI 帧率**。

---

## 3. 主循环伪代码

```cpp
void KeyScanManager::run() {
    matrix_.begin();
    encoder_.begin();

    KeyEventList events;
    events.reserve(32);

    const TickType_t period = pdMS_TO_TICKS(SCAN_INTERVAL_MS);
    TickType_t last = xTaskGetTickCount();

    while (true) {
        events.clear();

        // 1) 矩阵扫描（占用 COL0~COL3）
        matrix_.poll(events);

        // 2) 编码器扫描（COL1/COL2 已被释放）
        encoder_.poll(events);

        // 3) 派发
        if (!events.empty()) {
            dispatcher_.dispatch(events);
        }

        // 4) 周期等待（绝对时间）
        vTaskDelayUntil(&last, period);
    }
}
```

---

## 4. 与 MainTask / DisplayTask 的边界

```
KeyScanManager (Core 0, priority 2)
    │
    │  QueueHandle_t (32 slots, sizeof(KeyEvent))
    ▼
MainTask (Core 1, priority 1)
    │  • 键映射查表
    │  • HID 发送 (USB / BLE)
    │  • RGB 点击反馈
    ▼
    用户按键 / 编码器动作发生
```

- **`KeyScanManager` 不直接调用 LVGL、IKeyboard、RGBLightControl** — 单一职责。
- **`MainTask` 从队列消费 `KeyEvent`** — 旧 `MainTask.cpp` 里已有的 `scanner_.scan()` 替换为 `xQueueReceive(queue_, &ev, ...)`。

---

## 5. 启动时序（与现有 main.cpp 集成）

`setup()` 末尾新增：

```cpp
// 1. 创建 dispatcher（全局或局部）
static KeyEventDispatcher g_dispatcher;

// 2. 订阅：UI 焦点跟随编码器
g_dispatcher.subscribe(
    static_cast<uint8_t>(KeyEventType::EncoderRotate) |
    static_cast<uint8_t>(KeyEventType::EncoderClick),
    [](const KeyEvent& ev) {
        if (ev.type == KeyEventType::EncoderRotate) {
            // 旋转 → LVGL 焦点
            lv_group_focus_next(g_ui_group);
        } else {
            // 单击 → 进入当前焦点控件
            lv_obj_send_event(lv_group_get_focused(g_ui_group),
                              LV_EVENT_CLICKED, nullptr);
        }
    });

// 3. 创建队列给 MainTask
static QueueHandle_t g_keyQueue = xQueueCreate(32, sizeof(KeyEvent));
g_dispatcher.attachQueue(g_keyQueue);

// 4. 实例化模块
static MatrixScanner     g_matrix(g_cfg);
static RotaryEncoder     g_encoder(g_cfg);
static KeyScanManager    g_scanMgr(g_matrix, g_encoder, g_dispatcher);

// 5. 启动（在 LVGL 之后）
g_scanMgr.begin();
```

`loop()` 改为空（KeyScanManager 接管节奏，MainTask 接管键映射）。

---

## 6. 失败模式

| 场景 | 行为 | 应对 |
|---|---|---|
| 队列满 | 丢事件 + WARN | 监控 `dropped_events_` 计数，> 0 触发告警 |
| 回调执行太久 | 拖累扫描周期 | 回调里**只**做 LVGL API 调用；长任务发消息给其它任务 |
| 编码器 / 矩阵 GPIO 抢占 | 读到虚假边沿 | 严格按 §3 顺序调用，且 `MatrixScanner` 每次扫描后释放 GPIO |
| `KeyEventList` 容量超 | `std::vector` 自扩容 | 预 `reserve(32)`，超过会 `LOG_WARNING` |

---

## 7. 可观测性

`KeyScanManager` 暴露以下调试钩子（仅 `DEBUG_KEYSCAN=1` 时编译）：

```cpp
struct KeyScanStats {
    uint32_t total_polls;
    uint32_t total_events;
    uint32_t dropped_queue_full;
    uint32_t longest_poll_us;
};
const KeyScanStats& stats() const;
```

通过 Serial 周期性打印或 LVGL 调试屏显示。
