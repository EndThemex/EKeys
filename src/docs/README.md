# EKeys 重构设计文档索引

> 本目录存放 EKeys 项目整体重构的设计文档。
> 当前阶段：按键扫描子系统（v0.1 设计稿）

---

## 总览

- [01-keyscan-overview.md](01-keyscan-overview.md) — **从这里开始**
- [02-matrix-scanner.md](02-matrix-scanner.md) — 矩阵扫描与消抖
- [03-rotary-encoder.md](03-rotary-encoder.md) — EC11 旋转编码器
- [04-key-event-dispatcher.md](04-key-event-dispatcher.md) — 事件结构与派发
- [05-keyscan-manager.md](05-keyscan-manager.md) — 顶层 FreeRTOS 任务
- [06-interface-contract.md](06-interface-contract.md) — 对外接口契约与配置
- [07-pinout.md](07-pinout.md) — 引脚分配表
- [08-menu-highlight-jitter.md](08-menu-highlight-jitter.md) — MenuPage 高亮条"断层"问题排查与修复
- [09-ble-keyboard.md](09-ble-keyboard.md) — BLE 键盘输出与 KeyMap 配置
- [10-input-mapping-rule.md](10-input-mapping-rule.md) — 交互输入分配规则（按键 / 旋钮 / PageKind；§10.1 工程陷阱：inline 变量陷阱）

## 关键决策一览

| 决策点     | 选择                     | 理由                                |
| ---------- | ------------------------ | ----------------------------------- |
| 扫描周期   | 1 ms                     | 5 ms 消抖要求至少 1 ms 一次采样     |
| 任务优先级 | priority=2               | 高于 MainTask=1，确保不丢边沿       |
| 任务核心   | Core 0                   | 与 LVGL/Main 任务分开，避免互相阻塞 |
| 消抖       | 软件状态机               | 简单、跨 FreeRTOS tick 鲁棒         |
| GPIO 复用  | 矩阵扫描结束后释放为高阻 | 避免编码器读到矩阵拉低的电平        |
| 事件传递   | 回调 + 队列双通道        | UI 同步反应；MainTask 异步消费      |
| 编码器     | 不使用 ESP32Encoder PCNT | PCNT 在 GPIO 复用下需要独占引脚     |
| 日志       | 暂用 Serial.printf       | LogManager 第二阶段引入             |

---

## 参考实现

旧项目 `FunModularKeyboard` 的相关文件：

- [MatrixScanner.h](file:///D:/workspace/zheteng/ESP_Projects/FunModularKeyboard/src/MatrixScanner.h)
- [MatrixScanner.cpp](file:///D:/workspace/zheteng/ESP_Projects/FunModularKeyboard/src/MatrixScanner.cpp)
- [RotaryEncoder.h](file:///D:/workspace/zheteng/ESP_Projects/FunModularKeyboard/src/RotaryEncoder.h)
- [MainTask.cpp](file:///D:/workspace/zheteng/ESP_Projects/FunModularKeyboard/src/MainTask.cpp)

差异与改进点见各模块文档 § 与 FunModularKeyboard 的差异。
