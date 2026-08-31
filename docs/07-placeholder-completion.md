# 阶段 07 — 占位项补齐

> 状态：代码完成（编译验证 / 验收清单待执行）
> 关联章节：[`FEATURE_DOC.md §17`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.5 / §3.11 / §3.14 / §3.6`](../ARCHITECTURE.md)

## 目标

补齐 `FEATURE_DOC §17` 列出的所有"未完成项"，使代码与功能文档 1:1 对应，不再保留"仅日志警告"或"代码注释保留"的占位行为。

## 范围

| 项                            | 目标位置                                            |
| ----------------------------- | --------------------------------------------------- |
| 2.4G 无线键盘模式             | `src/output/Wireless24GKeyboardImpl.cpp`            |
| 麦克风频谱显示                | `src/audio/AudioAnalyzer.cpp` + DisplayTask 调度    |
| OTA 升级                      | `src/upgrade/Upgrade.cpp`                           |
| `CMD_CONF_VERSION_SET (0x02)` | `src/protocol/commands/cmd_firmware.cpp`            |
| `CMD_DEVICE_INFO_SET (0x04)`  | `src/protocol/commands/cmd_device_info.cpp`         |
| WiFi STA 自动重连精细策略     | `src/network/WiFiManager.cpp::processWiFiReconnect` |

## 前置条件

- 阶段 06 全部完成。
- `platformio.ini` 已含 OTA 所需的双 app 分区（`partitions-16MB.csv`）。

## 任务清单

- [ ] **7.1 `Wireless24GKeyboardImpl`**（已撤销：用户决定暂不实现 2.4G，保持警告 + USB 回退）：定义 `IRadio24G` 抽象（nRF24L01+ / 其他模块由硬件决定）；本期仅提供接口与日志告警，硬件未到位时仍允许打印 `2.4G not implemented`。
- [ ] **7.2 `AudioAnalyzer` 调度**：`DisplayTask::loop()` 增加频谱屏可见性判断，进入音乐屏时调度；不在时释放 CPU。
- [ ] **7.3 `Upgrade::begin / performOta`**：基于 `HTTPUpdate` 或 `esp_https_ota`；通过 `CMD_FIRMWARE_INFO` (0x0b) 携带 URL 与 checksum。
- [ ] **7.4 `cmd_firmware.cpp`**：新增 `0x02 CMD_CONF_VERSION_SET` handler；写 `version` 字段到 `DeviceSettings`。
- [ ] **7.5 `cmd_device_info.cpp`**：新增 `0x04 CMD_DEVICE_INFO_SET` handler；写 `device_name / serial` 等字段。
- [ ] **7.6 `WiFiManager::processWiFiReconnect`**：把"15s 未恢复强制重启 WiFi"逻辑下放到该函数内；BLE 模式下整体短路。
- [ ] **7.7 全量自检**：重跑阶段 01\~06 的验收清单，确认无回归。
- [ ] **7.8 文档同步**：更新 [`../FEATURE_DOC.md`](../FEATURE_DOC.md) §17 表中各项状态；如有接口调整同步更新 [`../ARCHITECTURE.md`](../ARCHITECTURE.md)。

## 验收标准

- `CMD_CONF_VERSION_SET` 与 `CMD_DEVICE_INFO_SET` 调用不再产生 `LOG_WARNING`，而是成功回包。
- OTA：桌面 App 通过协议下发 URL 后，设备重启进入 OTA 流程，结束后自动重启进入新固件。
- 频谱屏可见时显示 16 个频段柱状图；不可见时不消耗 CPU。
- 2.4G 模式：硬件未接入时仍走安全分支（打印警告），不破坏 USB/BLE 切换流程。

## 变更记录

- 2026-08-31（用户决定追加）：**7.1 撤销**——2.4G 功能按用户决定暂不实现，已删除 `IRadio24G.h` / `Wireless24GKeyboardImpl.h/.cpp`，`KeyboardFactory::Wireless24G` 恢复为警告 + USB 回退（`work_mode=2` 配置仍兼容，选择后设备可用 USB）。7.8 文档（FEATURE_DOC §17 / ARCHITECTURE §3.5）已同步该决定。
- 2026-08-31（阶段 07 代码完成）：
  - **7.1**：新增 `src/output/IRadio24G.h`（射频抽象）与 `Wireless24GKeyboardImpl`（实现 IKeyboard，6-key 报告缓冲 + 修饰键位表）；`KeyboardFactory::Wireless24G` 分支改走该后端，`BOARD_HAS_24G` 未定义时 `begin()` 打印 `2.4G not implemented` 并回退 USB。
  - **7.2**：`DisplayTask` 新增 `updateSpectrum()`——主循环每拍判断活动屏，仅 `UI_SCREEN_MUSIC(_SECONDARY)` 可见时挂起 VoiceRecognizer、接管 Mic（I2S0）、读 512 样本喂 `AudioAnalyzer`（FFT 16 频段 0~255）并调 `ui_MusicScreen_drawAudioBandsCool` 渲染；离开音乐屏释放 Mic 并 resume 语音识别，不消耗 CPU。
  - **7.3**：新增 `src/upgrade/Upgrade.h/.cpp`——`CMD_FIRMWARE_INFO (0x0b)` 请求携带 `data.url` + `data.checksum`（固件 MD5 hex，必填）触发；回成功响应后 `performOta()` 在 MainTask 上下文流式下载写入 OTA 分区，边写边算 MD5，校验通过才 `Update.end()` 后自动重启，失败 abort 不覆盖当前固件。
  - **7.4**：`cmd_firmware.cpp` 注册 `0x02 CMD_CONF_VERSION_SET`；`data.version` 写入 `DeviceSettings.config_version` 并持久化到 config.ini `[system] config_version`。
  - **7.5**：`cmd_device_info.cpp` 注册 `0x04 CMD_DEVICE_INFO_SET`；`data.device_name` / `data.serial` 写入 `DeviceSettings.device_name` / `serial_number` 并持久化；0x03 之后优先返回持久化 device_name。
  - **7.6**：`WiFiManager` 新增 `processWiFiReconnect()`——Connected 断链给 15s 宽限等 STA 自恢复，超时强制重启射频（mode off → 重新 begin）回 WaitingRetry；`process()` 顶部 BLE 模式整体短路。
  - **7.7**：全部改动文件 clangd 诊断 0 错误；配置层 `DeviceSettings` 新增 `config_version / device_name / serial_number` 字段，`Configuration::sectionOfKey` 与 `loadGlobalSettings_locked` 同步支持。
  - **7.8**：FEATURE_DOC §17 全部改为"已实现（待验证）"；ARCHITECTURE §3.5/§3.14/§4 同步；desktop-app-protocol.md 命令表 0x01/0x02/0x03 状态更新。
  - 注：复选框待 `pio run` 编译验证 + 验收清单通过后勾选。

## 备注

- 2.4G 硬件未到位前，可在 `Wireless24GKeyboardImpl` 内部保留 `#ifdef BOARD_HAS_24G` 守卫。
- OTA 升级必须先验证签名 / checksum，否则禁止覆盖当前固件。
- 阶段 07 完成后，`FEATURE_DOC.md` / `ARCHITECTURE.md` 都应同步标注"全部功能已实现"或保留明确未实现项的硬约束。
