# EKeys 按键扫描子系统 — 架构总览

> 状态：设计草案 v0.1
> 适用项目：`D:\workspace\zheteng\ESP_Projects\EKeys`
> 目标 MCU：ESP32-S3-WROOM-1（Arduino + LVGL 8.3）

---

## 1. 目标

把当前散落在 `main.cpp` 中的逻辑（仅 LVGL 显示），重构为一个职责清晰、可独立替换的
**按键扫描子系统**。该子系统未来负责：

1. 读取 4 列 × 3 行 = **12 键** 的键盘矩阵（带消抖）。
2. 读取 EC11 旋转编码器（顺时针/逆时针/单击）。
3. 产生「按下了哪些键」「释放了哪些键」「编码器转了几格」的**事件流**。
4. 把事件流通过回调 / 队列投递给上层（默认 `MainTask` 处理键映射与 HID 输出）。

本次重构**只做按键扫描**这一层，键映射、HID 输出、UI 联动由后续阶段补齐。

---

## 2. 设计原则

| 原则 | 落地方式 |
|---|---|
| 单一职责 | `MatrixScanner` 只负责「轮询电平 + 消抖」，不知道什么是键映射 |
| 硬件解耦 | 行/列 GPIO 集中在 `KeyScanConfig.h`，不散落在业务代码里 |
| 线程安全 | 所有跨任务接口走 `KeyEventQueue` 或 `std::function` 回调 + 临界区 |
| 可替换 | 提供 `IKeySource` 抽象，未来可以替换成 I2C 扩展模块、`KeyMatrix` 模拟等 |
| 可测试 | `MatrixScanner` 不依赖 LVGL、不依赖 USB / BLE，纯逻辑可单元测试 |
| 可观测 | 每个事件带 `timestamp_ms`，关键状态机变化打日志 |

---

## 3. 模块划分

```
keyscan/
├── KeyScanConfig.h          # 行/列 GPIO、键数、消抖时间等常量
├── KeyEvent.h               # 事件数据结构
├── IKeySource.h             # 抽象事件源接口
├── MatrixScanner.h/.cpp     # 矩阵扫描 + 消抖状态机
├── RotaryEncoder.h/.cpp     # EC11 编码器扫描 + 单击
├── KeyEventDispatcher.h/.cpp # 事件聚合 → 回调 / 队列
├── KeyScanManager.h/.cpp    # 顶层任务：调度上述模块
└── README.md                # 本目录导航
```

层级关系：

```
        ┌──────────────────────┐
        │   KeyScanManager     │  ← FreeRTOS 任务 / 周期 tick
        └──────────┬───────────┘
                   │ 调用 IKeySource::poll()
        ┌──────────┴───────────┐
        │     IKeySource       │  ← 抽象接口
        └──────┬────────┬──────┘
               │        │
   ┌───────────┘        └────────────┐
   ▼                                  ▼
MatrixScanner                  RotaryEncoder
(12 键矩阵)                     (EC11)
```

所有源都实现同一个接口 `IKeySource::poll()`，
返回一组 `KeyEvent`。`KeyScanManager` 聚合事件后分发给 `KeyEventDispatcher`。

---

## 4. 数据流

```
GPIO 电平 ──► MatrixScanner::poll() ──┐
                                      ├─► KeyScanManager ──► KeyEventDispatcher
EC11 A/B/SW ─► RotaryEncoder::poll() ─┘                            │
                                                                     ▼
                                                       ┌─────────────┴────────────┐
                                                       ▼                          ▼
                                                 Callback 注册表              QueueHandle_t
                                                 (lvgl 焦点 / RGB 显示)       (MainTask 消费)
```

> 主任务 `MainTask` 从 `QueueHandle_t` 消费，做键映射 → HID 输出；
> UI 焦点导航通过 Callback 注册到 dispatcher，由 dispatcher 在 `loop()` 内直接同步派发。

---

## 5. 与现有代码的集成点

| 现有文件 | 需要新增 / 修改 |
|---|---|
| `src/main.cpp` | `setup()` 末尾启动 `KeyScanManager`；`loop()` 由其内部驱动 |
| `include/` | 改名为 `src/keys/` 也可；本次把 `.h` 放到 `src/keyscan/`，`include/` 保留为 PIO 默认头目录 |
| `platformio.ini` | 不需要新增依赖（无 OneButton 也可；如需 OneButton 再加） |
| `lib/` | 不需要 |

> **注意**：`include/` 当前只有 PIO 的占位 README，不要破坏它。新建 `src/keyscan/` 即可。

---

## 6. 后续阶段（不在本次重构范围）

| 阶段 | 内容 |
|---|---|
| 2 | `KeyMapping` 配置 + 8 个 Profile |
| 3 | USB HID / BLE HID 双模输出 |
| 4 | LVGL 焦点联动（旋钮导航、按键音效、RGB 反馈） |
| 5 | 长按、组合键、Tap-Hold 等增强语义 |
