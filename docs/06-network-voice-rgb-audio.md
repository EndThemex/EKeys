# 阶段 06 — 网络 / 语音 / RGB / 音频

> 状态：未开始
> 关联章节：[`FEATURE_DOC.md §7 / §9 / §10 / §11`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.8 / §3.10 / §3.11`](../ARCHITECTURE.md)

## 目标

按阶段 04 已建立的协议链路，把 `work_mode = BLUETOOTH_KEYBOARD_MODE`、WiFi STA、NTP、TCP 控制通道、语音 ASR、RGB 灯光、本地/远程音频全部接入。完成后应可"语音按键 → ASR → 桌面 App 收到文本"端到端跑通。

## 范围

1. `src/network/`：WiFi STA、TCP、UDP 自动发现、NTP。
2. `src/output/`：补充 `BLEKeyboardImpl`，切换时释放经典蓝牙内存。
3. `src/audio/`：Speaker + Mic + AudioAnalyzer（占位）。
4. `src/voice/`：百度短语音 ASR + token 缓存。
5. `src/rgb/`：WS2812B 驱动 + 动画 + 点击高亮。
6. 协议层：补齐 `cmd_music / cmd_pc_status / cmd_profile / cmd_keymap / cmd_firmware / cmd_device_info` handler。

## 前置条件

- 阶段 04 完成；`CMD_CONFIG_SET` 已能切换 `work_mode`。
- 阶段 05 完成；UI 屏可显示状态条。

## 任务清单

### 网络

- [ ] **6.1 `src/network/WiFiManager.h/.cpp`**：实现 `ConnectToWiFi / scheduleWiFiConnectAttempt / stopWiFiReconnect / processWiFiReconnect`；BLE 模式下禁止开启。
- [ ] **6.2 `src/network/NtpSync.h/.cpp`**：WiFi 连上后调用 `SyncTimeFromNTP()`（GMT+8）。
- [ ] **6.3 `src/network/TcpChannel.h/.cpp`** + **`DiscoveryService.h/.cpp`**：UDP 30001 自动发现 + TCP 30000 控制通道，复用 `SerialProtocol`。
- [ ] **6.4 `src/network/NetDiagnostics.h/.cpp`**：RSSI / IP 收集，供 HA 屏使用。

### 键盘输出

- [ ] **6.5 `src/output/BLEKeyboardImpl.h/.cpp`**：基于 `t-vk/ESP32 BLE Keyboard`；`begin()` 时调用 `esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)`。
- [ ] **6.6 `src/output/KeyboardFactory`**：根据 `work_mode` 创建 USB / BLE 实例；BLE 模式失败时回落到 USB 并报警。

### 音频

- [ ] **6.7 `src/audio/Speaker.h/.cpp`**：MAX98357A，封装 `PlayRemoteAudio / PlayLocalAudio / Pause / Resume / Stop`；音量 `SetVolume(0~21)`。
- [ ] **6.8 `src/audio/Mic.h/.cpp`**：ICS43434 数字 MEMS 麦克风，引脚 `BCLK=IO10 / WS=IO12 / SCK=IO13 / SDOUT=IO11`；16kHz / 512 samples，提供 `Read()` 阻塞读。
- [ ] **6.9 `src/audio/AudioAnalyzer.h/.cpp`**：FFT_SIZE=512 / BANDS=16；本阶段只编译，不在 `MainTask` 调度。

### 语音

- [ ] **6.10 `src/voice/VoiceRecognizer.h/.cpp`**：百度短语音 ASR REST，`dev_pid=1537` 默认；token 缓存。
- [ ] **6.11 `src/voice/VoiceConfig.h`** + **`AsrTokenCache.h/.cpp`**：凭证字段与 token 自动刷新。
- [ ] **6.12 `src/keymap/KeyEventDispatcher`**：命中 `KEY_FUNCTION_ASR` 时调用 `VoiceRecognizer::startCapture()`；松开后 `finishCapture()`。
- [ ] **6.13 协议命令 `0x0c CMD_VOICE_TEXT`**：识别完成后通过 `SerialProtocol::send()` 上报桌面 App。

### RGB

- [ ] **6.14 `src/rgb/RGBDriver.h/.cpp`**：WS2812B GRB 顺序；`LED_PWR_CTRL` 上电默认拉高。
- [ ] **6.15 `src/rgb/RGBLightControl.h/.cpp`**：`RGBMode` 枚举 + 动画循环；DisplayTask 驱动 tick。
- [ ] **6.16 `src/rgb/ClickHighlight.h/.cpp`**：`RGB_CLICK_MODE` 三种点击高亮。

### 协议补齐

- [ ] **6.17 `cmd_music.cpp`**：`0x0e CMD_MUSIC_STATUS`（App→主控）/ `0x0f CMD_MUSIC_CONTROL`（主控→App）。
- [ ] **6.18 `cmd_pc_status.cpp`**：`0x0d CMD_PC_STATUS`。
- [ ] **6.19 `cmd_profile.cpp`**：`0x10 CMD_PROFILE_STATE` / `0x11 CMD_PROFILE_ICON_SET`。
- [ ] **6.20 `cmd_keymap.cpp`**：`0x05 CMD_KEYMAP_GET` / `0x06 CMD_KEYMAP_SET`。
- [ ] **6.21 `cmd_firmware.cpp`**：`0x01 CMD_CONF_VERSION_GET` / `0x0b CMD_FIRMWARE_INFO`。
- [ ] **6.22 `cmd_device_info.cpp`**：`0x03 CMD_DEVICE_INFO_GET`。
- [ ] **6.23 `registration.cpp`**：把 6.17\~6.22 全部 `registerCmd` 进来。

### 验证

- [ ] **6.24 端到端联调**：USB 模式 + WiFi + 语音键 → ASR → 桌面 App 显示识别文本。
- [ ] **6.25 自检记录**：记录 BLE / WiFi 互斥切换的实测耗时、ASR 平均响应时间。

## 验收标准

- USB 模式：按 `KEY_FUNCTION_ASR` 触发录音，松开后桌面 App 收到文本（与 FEATURE_DOC §11.3 一致，仅在 `WIRED_KEYBOARD_MODE + WiFi已连 + 语音启用` 时启用）。
- BLE 模式：语音链路整体跳过（`LOG_INFO` 提示"语音仅 USB 模式可用"），按键 HID 行为不受影响。
- USB 模式：WiFi 开启后 NTP 时间 5s 内同步；状态条 WiFi 图标切换。
- RGB：默认彩虹动画；按下任意键时该 LED 点亮，松开熄灭（`CLICK_SINGLE_COLOR_MODE`）。
- 音频：本地 `data/audio/coin2.wav` 启动播放一次；远程 URL 调用 `PlayRemoteAudio` 可播放。
- 协议：桌面 App 修改 `work_mode` 后键盘实例实时重建。

## 变更记录

- _暂无_

## 备注

- BLE 与 WiFi 共存时 BLE 内存紧张，所有非必要缓冲（PSRAM 分配）需明确规划。
- 语音仅在 `WIRED_KEYBOARD_MODE + WiFi已连 + 语音启用` 时启用；BLE / 2.4G 模式直接 `LOG_INFO` 跳过（不影响按键 HID 与协议层）。
- 音乐屏进入会 `VoiceRecognizer::suspend()`，离开自动 `resume()`。
- 阶段 06 不补齐 2.4G / 频谱 / OTA；这些放到阶段 07。
