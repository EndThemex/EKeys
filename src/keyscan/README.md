# keyscan — 按键扫描子系统

> 本目录包含 ESP32-S3 上 11 键矩阵 + EC11 编码器的扫描、事件化、派发代码。

---

## 文件清单（设计稿）

```
keyscan/
├── README.md                ← 本文件
├── KeyScanConfig.h          ← GPIO 与键位映射常量
├── KeyEvent.h               ← 事件结构 + 队列消息类型
├── IKeySource.h             ← 抽象事件源接口
├── MatrixScanner.h
├── MatrixScanner.cpp
├── RotaryEncoder.h
├── RotaryEncoder.cpp
├── KeyEventDispatcher.h
├── KeyEventDispatcher.cpp
├── KeyScanManager.h
└── KeyScanManager.cpp
```

---

## 设计文档

详细的架构与契约请见 [`../docs/`](../docs/) 目录：

| 文档 | 内容 |
|---|---|
| [01-keyscan-overview.md](../docs/01-keyscan-overview.md) | 整体架构与数据流 |
| [02-matrix-scanner.md](../docs/02-matrix-scanner.md) | `MatrixScanner` 模块设计 |
| [03-rotary-encoder.md](../docs/03-rotary-encoder.md) | EC11 编码器扫描 |
| [04-key-event-dispatcher.md](../docs/04-key-event-dispatcher.md) | 事件结构与派发 |
| [05-keyscan-manager.md](../docs/05-keyscan-manager.md) | 顶层 FreeRTOS 任务 |
| [06-interface-contract.md](../docs/06-interface-contract.md) | 对外接口契约与配置 |

---

## 当前实现状态

| 模块 | 设计 | 代码 | 测试 |
|---|---|---|---|
| `KeyScanConfig.h` | ✅ | ⏳ | — |
| `KeyEvent.h` | ✅ | ⏳ | — |
| `IKeySource.h` | ✅ | ⏳ | — |
| `MatrixScanner` | ✅ | ⏳ | ⏳ |
| `RotaryEncoder` | ✅ | ⏳ | ⏳ |
| `KeyEventDispatcher` | ✅ | ⏳ | ⏳ |
| `KeyScanManager` | ✅ | ⏳ | ⏳ |

> 本次重构只产出**设计文档**；代码进入下一阶段统一提交。
