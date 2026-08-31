# 阶段 07 — 占位项补齐

> 状态：未开始
> 关联章节：[`FEATURE_DOC.md §17`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.5 / §3.11 / §3.14 / §3.6`](../ARCHITECTURE.md)

## 目标

补齐 `FEATURE_DOC §17` 列出的所有"未完成项"，使代码与功能文档 1:1 对应，不再保留"仅日志警告"或"代码注释保留"的占位行为。

## 范围

| 项                                        | 目标位置                                                |
| ----------------------------------------- | ------------------------------------------------------- |
| 2.4G 无线键盘模式                         | `src/output/Wireless24GKeyboardImpl.cpp`                |
| 麦克风频谱显示                            | `src/audio/AudioAnalyzer.cpp` + DisplayTask 调度         |
| OTA 升级                                  | `src/upgrade/Upgrade.cpp`                               |
| `CMD_CONF_VERSION_SET (0x02)`             | `src/protocol/commands/cmd_firmware.cpp`                |
| `CMD_DEVICE_INFO_SET (0x04)`              | `src/protocol/commands/cmd_device_info.cpp`             |
| WiFi STA 自动重连精细策略                 | `src/network/WiFiManager.cpp::processWiFiReconnect`     |

## 前置条件

- 阶段 06 全部完成。
- `platformio.ini` 已含 OTA 所需的双 app 分区（`partitions-16MB.csv`）。

## 任务清单

- [ ] **7.1 `Wireless24GKeyboardImpl`**：定义 `IRadio24G` 抽象（nRF24L01+ / 其他模块由硬件决定）；本期仅提供接口与日志告警，硬件未到位时仍允许打印 `2.4G not implemented`。
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

- _暂无_

## 备注

- 2.4G 硬件未到位前，可在 `Wireless24GKeyboardImpl` 内部保留 `#ifdef BOARD_HAS_24G` 守卫。
- OTA 升级必须先验证签名 / checksum，否则禁止覆盖当前固件。
- 阶段 07 完成后，`FEATURE_DOC.md` / `ARCHITECTURE.md` 都应同步标注"全部功能已实现"或保留明确未实现项的硬约束。