# KeyEvent & KeyEventDispatcher — 事件数据结构与派发

> 文件：`src/keyscan/KeyEvent.h`、
> `src/keyscan/KeyEventDispatcher.h` / `src/keyscan/KeyEventDispatcher.cpp`

---

## 1. 事件数据结构

```cpp
// KeyEvent.h
#pragma once
#include <Arduino.h>
#include <vector>

enum class KeyEventType : uint8_t {
    Press              = 1,
    Release            = 2,
    EncoderRotate      = 3,
    EncoderClick       = 4,
};

struct KeyEvent {
    KeyEventType type;
    uint8_t      keyId;          // 1..11 物理键 / 0xFE 编码器
    int8_t       encoderDelta;   // 仅 EncoderRotate
    uint32_t     timestamp_ms;

    String toString() const;     // 用于日志
};

using KeyEventList = std::vector<KeyEvent>;
```

---

## 2. IKeySource 抽象

```cpp
// IKeySource.h
#pragma once
#include "KeyEvent.h"

class IKeySource {
public:
    virtual ~IKeySource() = default;
    virtual void begin() = 0;
    virtual void poll(KeyEventList& out) = 0;
};
```

---

## 3. Dispatcher 设计

### 3.1 角色

`KeyEventDispatcher` 负责把 `KeyScanManager` 收集到的 `KeyEventList` 分发给两类订阅者：

| 类型 | 用途 | 线程 |
|---|---|---|
| **回调订阅** | LVGL 焦点导航、RGB 反馈、蜂鸣器提示 | 与 Dispatcher 同任务（同步派发） |
| **队列订阅** | MainTask 消费，做键映射 + HID 输出 | 跨任务（FreeRTOS Queue） |

### 3.2 接口

```cpp
class KeyEventDispatcher {
public:
    using EventCallback = std::function<void(const KeyEvent&)>;

    // 回调订阅（同步派发）
    void subscribe(KeyEventType typeMask, EventCallback cb);

    // 队列订阅（异步派发）
    void attachQueue(QueueHandle_t queue);  // 每事件一条队列消息

    // 主任务使用
    void dispatch(const KeyEventList& events);
};
```

### 3.3 派发语义

```
for each event in events:
    if (event.type & callbackMask_) -> invoke callback
    if (queue_ != NULL)             -> xQueueSend(queue_, &event, 0)
```

> - 回调在 **KeyScanManager 任务上下文** 执行，**不要在回调里做长时间阻塞**。
> - 队列满时**丢弃**（`xQueueSend` timeout=0），并在日志里记一次 WARN，避免阻塞扫描。
> - 派发顺序按 `events` 数组顺序，即按 `MatrixScanner::poll` → `RotaryEncoder::poll` 的顺序，
>   保证「同一物理键 PRESS 先于 RELEASE」「旋转事件在键事件之后」。

---

## 4. 订阅示例

```cpp
// 在 MainTask 里：
g_dispatcher.attachQueue(mainQueue);

// 在 UI 任务里：
g_dispatcher.subscribe(
    KeyEventType::EncoderRotate | KeyEventType::EncoderClick,
    [](const KeyEvent& ev) {
        // LVGL 焦点移动
        lv_group_focus_next(g_group);
    });
```

---

## 5. 内存预算

- `KeyEvent`：`1+1+1+4 = 7` 字节，对齐后 8 字节
- 一次 `poll()` 最多产生：1 个 PRESS（同时按下所有 12 键 → 12 个） + 1 个 RELEASE（12 个）
- `KeyEventList::reserve(32)` 防止堆碎片
