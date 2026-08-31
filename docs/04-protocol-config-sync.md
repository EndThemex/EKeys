# 阶段 04 — 私有协议与桌面 App 同步

> 状态：未开始
> 关联章节：[`FEATURE_DOC.md §5`](../FEATURE_DOC.md) / [`§6`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.6`](../ARCHITECTURE.md)

## 目标

实现私有协议链路：USB CDC Serial（暂不接 TCP）+ `CommandRegistry` + `cmd_config.cpp`，让桌面 App 能通过 `CMD_CONFIG_GET` / `CMD_CONFIG_SET` 双向同步 `DeviceSettings`。本阶段只覆盖 `§6` 字段集合。

## 范围

1. 引入 `src/protocol/` 与 `src/protocol/commands/`。
2. 引入 `ArduinoJson@7.x` 作为 JSON 行解析依赖（见任务 4.1 决策）。
3. 注册命令：`0x07 CMD_CONFIG_GET` / `0x08 CMD_CONFIG_SET`（其它命令在阶段 05/06 补齐）。
4. 桌面 App 通过 USB CDC 串口发送 JSON 行；主控回复 JSON 行。

## 前置条件

- 阶段 03 已完成，`DeviceSettings` 与 `Configuration::mutex_` 可用。
- 阶段 02 已完成，`DisplayMessage::SETTING_UPDATE` 通道可用。

## 任务清单

- [ ] **4.1 `lib_deps` 决策**：使用 `ArduinoJson@7.x`，避免自行手写解析；记录在 `变更记录`。
- [ ] **4.2 `src/protocol/CommandRegistry.h/.cpp`**：实现 `std::array<Entry,64>` + 临界区注册 / 分发；空命令走 `LOG_WARNING`。
- [ ] **4.3 `src/protocol/SerialProtocol.h/.cpp`**：封装 `Serial` 流式 JSON 行收发；解析一行后调用 `CommandRegistry::dispatch(cmd, payload)`；心跳 `0x0a` 自处理。
- [ ] **4.4 `src/protocol/commands/cmd_config.cpp`**：实现 `0x07 / 0x08` 两个 handler；读取 / 写入 `DeviceSettings` 字段；变更后向 `displayQueue_` 投递 `SETTING_UPDATE`。
- [ ] **4.5 `src/config/parseConfigSetCommand.cpp`**：把 `DeviceSettings` 字段原子写入逻辑集中在该文件；变更记录字段新旧值。
- [ ] **4.6 `src/protocol/registration.cpp`**：`registerAllCommandHandlers()` 仅注册 `cmd_config`；后续阶段扩展。
- [ ] **4.7 `src/app/AppContext`**：持有 `SerialProtocol` 与 `CommandRegistry`；`MainTask::loop()` 周期 `SerialProtocol::poll()`。
- [ ] **4.8 `src/tasks/MainTask`**：接入 `work_mode` 变更时调用 `KeyboardFactory::recreate()`；`wifi_switch` 变更时调用 `network::WiFiManager::schedule()`。
- [ ] **4.9 桌面 App 联调**：以桌面 App 或 Python 脚本发送 `CMD_CONFIG_GET` / `CMD_CONFIG_SET`，验证字段双向同步。
- [ ] **4.10 自检记录**：记录 USB CDC 波特率、心跳间隔、JSON 字段命名约定。

## 验收标准

- 桌面 App 发送 `CMD_CONFIG_SET` 修改 `tft_brightness=50` 后，屏幕亮度立即变化。
- 修改 `work_mode` 后日志输出新模式；当前仅 USB 工作。
- `Configuration::mutex_` 不被协议层绕过：所有 `DeviceSettings` 读写都通过 `Configuration` API。
- `CommandRegistry::dispatch` 对未注册命令返回 `LOG_WARNING`，不崩溃。

## 变更记录

- _暂无_

## 备注

- 本阶段暂不接 TCP 与 UDP 自动发现；TCP 在阶段 06 与 WiFi 一起接入。
- `CMD_CONFIG_SET` 字段命名采用 `lower_snake_case`（与 `FEATURE_DOC §6` 保持一致）。
- 桌面 App 暂未提供，本阶段可用 `python -m serial.tools.miniterm` 手动发送 JSON 行验证。
