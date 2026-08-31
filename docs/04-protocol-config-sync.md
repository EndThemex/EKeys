# 阶段 04 — 私有协议与桌面 App 同步

> 状态：代码完成（4.9 联调待验证）
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

- [x] **4.1 `lib_deps` 决策**：使用 `ArduinoJson@7.x`，避免自行手写解析；记录在 `变更记录`。
- [x] **4.2 `src/protocol/CommandRegistry.h/.cpp`**：实现 `std::array<Entry,64>` + 临界区注册 / 分发；空命令走 `LOG_WARNING`。
- [x] **4.3 `src/protocol/SerialProtocol.h/.cpp`**：封装 `Serial` 流式 JSON 行收发；解析一行后调用 `CommandRegistry::dispatch(cmd, payload)`；心跳 `0x0a` 自处理。
- [x] **4.4 `src/protocol/commands/cmd_config.cpp`**：实现 `0x07 / 0x08` 两个 handler；读取 / 写入 `DeviceSettings` 字段；变更后向 `displayQueue_` 投递 `SETTING_UPDATE`。
- [x] **4.5 `src/config/parseConfigSetCommand.cpp`**：把 `DeviceSettings` 字段原子写入逻辑集中在该文件；变更记录字段新旧值。
- [x] **4.6 `src/protocol/registration.cpp`**：`registerAllCommandHandlers()` 仅注册 `cmd_config`；后续阶段扩展。
- [x] **4.7 `src/app/AppContext`**：持有 `SerialProtocol` 与 `CommandRegistry`；`MainTask::loop()` 周期 `SerialProtocol::poll()`。
- [x] **4.8 `src/tasks/MainTask`**：接入 `work_mode` 变更时调用 `KeyboardFactory::recreate()`；`wifi_switch` 变更时调用 `network::WiFiManager::schedule()`。
- [ ] **4.9 桌面 App 联调**：以桌面 App 或 Python 脚本发送 `CMD_CONFIG_GET` / `CMD_CONFIG_SET`，验证字段双向同步。
- [x] **4.10 自检记录**：记录 USB CDC 波特率、心跳间隔、JSON 字段命名约定。

## 验收标准

- 桌面 App 发送 `CMD_CONFIG_SET` 修改 `tft_brightness=50` 后，屏幕亮度立即变化。
- 修改 `work_mode` 后日志输出新模式；当前仅 USB 工作。
- `Configuration::mutex_` 不被协议层绕过：所有 `DeviceSettings` 读写都通过 `Configuration` API。
- `CommandRegistry::dispatch` 对未注册命令返回 `LOG_WARNING`，不崩溃。

## 变更记录

- 2026-08-31：`lib_deps` 新增 `bblanchon/ArduinoJson@^7.0.4`（v7 `JsonDocument` 动态容量，替代 v6 `DynamicJsonDocument`）。
- 2026-08-31：`Configuration` 新增 `snapshot()`（加锁复制）与 `mutateSettings()`（锁内原子修改），协议层读写 `DeviceSettings` 不再绕过 `mutex_`；持久化仍在锁外经 `saveSetting()` 逐键完成（其内部自行加锁，避免死锁）。
- 2026-08-31：`Backlight` 升级为单例 + LEDC PWM（通道 0 / 5kHz / 8-bit，默认 80%），替代原 GPIO 高低电平；`main.cpp` 改用 `Backlight::instance().begin()`。原计划阶段 05 实施，因本阶段验收标准要求 `tft_brightness` 立即生效而提前。
- 2026-08-31：`DisplayMessage` 联合体扩展 `setting` 载荷（`tft_brightness` / `tft_theme`）；`DisplayTask` 收到 `SETTING_UPDATE` 后调用 `Backlight::setDuty()`；`tft_theme` 由阶段 05 UI 屏接管。
- 2026-08-31：`KeyboardFactory::recreate()` 语义落地为 `AppContext::applyWorkMode(uint8_t)`（create + 释放旧实例 + setKeyboard）；BLE / 2.4G 后端仍回退 USB 并 WARN（阶段 06 接入）。
- 2026-08-31：`wifi_switch` / `connect_host` / `wifi_ssid` / `wifi_password` 变更仅记录日志并持久化；`network::WiFiManager::schedule()` 于阶段 06 接入 WiFi 时实现。
- 2026-08-31：`Transport.h`（ITransport）随 TCP 一起延后至阶段 06；本阶段 `SerialProtocol` 直接绑定 USB CDC，接口预留 `poll()` / `sendDocument()` 便于届时扩展双通道。
- 2026-08-31：`rgb_single_colar` 在本项目 `DeviceSettings` 中为 `uint8_t`（参考工程为颜色字符串），协议仍按数值解析。

## 备注

- 本阶段暂不接 TCP 与 UDP 自动发现；TCP 在阶段 06 与 WiFi 一起接入。
- `CMD_CONFIG_SET` 字段命名采用 `lower_snake_case`（与 `FEATURE_DOC §6` 保持一致）。
- 桌面 App 暂未提供，本阶段可用 `python -m serial.tools.miniterm` 手动发送 JSON 行验证。

### 4.10 自检记录

- 传输层：USB CDC（HW CDC，`ARDUINO_USB_MODE=1` + `ARDUINO_USB_CDC_ON_BOOT=1`），波特率 115200；`Serial` 由 `main.cpp` `Serial.begin(115200)` 初始化。
- 日志与协议共用同一 CDC 端口（LogManager 输出到 `Serial`）：日志行以 `[I]TAG: ...` 开头，协议行以 `{` 开头，桌面 App / 测试脚本必须跳过非 `{` 起始的行。
- JSON 行格式：
  - 请求：`{"cmd":<int>,"seq":<int>,"data":{...}}`（`data` 可省略）
  - 响应：`{"cmd":<cmd|0x80>,"seq":<int>,"status":0或1,"data":{...}}`；失败附加 `"error":"<msg>"`
  - 心跳 `0x0a` 自回复 `0x8a`，`data.timestamp` 为 `millis()`；无独立周期心跳（设备侧仅应答，桌面 App 侧自行定时）
  - `CMD_CONFIG_GET` 响应与 `CMD_CONFIG_SET` 成功后的主动上报均为 `cmd=0x87`（上报 seq=0），字段集合见 `FEATURE_DOC §6`，附加 `active_profile_name` / `active_profile_has_custom_icon`
- 字段命名：`lower_snake_case`，与 `DeviceSettings` 字段一一对应；未知字段忽略并 `LOG_WARNING`；`tft_brightness` 钳位 5~100；`work_mode` 合法域 0~2；`active_keymap_profile` 合法域 0~7。
- 行缓冲 2048 字节，超长行丢弃并告警；`\r` 自动剥离。
