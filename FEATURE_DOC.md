# FunModularKeyboard 固件功能文档

> 适用代码版本：ESP32-S3-WROOM-1（N16R8，16MB Flash / 8MB PSRAM / OPI PSRAM），Arduino 框架，PlatformIO 6.3.2

---

## 1. 项目概览

### 1.1 硬件平台

- **MCU**：ESP32-S3-WROOM-1 模组（N16R8 = 16MB Flash / 8MB Octal PSRAM，board = `esp32-s3-devkitc-1`），PSRAM 已启用
- **USB**：TinyUSB CDC + HID（USB 键盘 + 消费控制 + 串口）
- **无线**：Wi-Fi STA、蓝牙 BLE（`t-vk/ESP32 BLE Keyboard`）
- **存储**：SPIFFS 文件系统（分区 896KB）+ NVS
- **显示**：TFT_eSPI + LVGL 8.3.11（SquareLine Studio 生成 UI）
- **音频**：ESP32-audioI2S（MAX98357 喇叭，INMP441 麦克风）

### 1.2 软件架构

- **双 FreeRTOS 任务**：
  - [MainTask](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/MainTask.h) —— 主任务（Core 1，栈 12288）：按键扫描、WiFi/BLE、I2C、协议收发、语音触发
  - [DisplayTask](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/DisplayTask.h) —— 显示任务（Core 0）：LVGL tick、RGB LED、频谱动画、消息处理
- **任务间通信**：`xQueue`（`DisplayMessage` 队列，长度 10）
- **协议命令分发**：[CommandRegistry](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/CommandRegistry.h) 单例 + [protocol::commands](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/registration.cpp) 模块化注册
- **事件总线**：[EventBus](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/services/EventBus.h)（Phase 1 基础设施，订阅/发布同步分发）
- **持久化**：[Configuration](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/Configuration.h) + [KeymapRepository](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/services/KeymapRepository.h) + [ConfigStore](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/services/ConfigStore.h)，基于 `SimpleIni` 读写 SPIFFS

### 1.3 文件 / 目录结构

| 目录                                                                                                              | 说明                                                                     |
| ----------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| [src/](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src)                              | 主源码                                                                   |
| [src/protocol/](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol)            | 私有协议（CommandRegistry + 各命令模块）                                 |
| [src/services/](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/services)            | 持久化服务（ConfigStore / KeymapRepository / EventBus / DeviceSettings） |
| [src/utils/](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/utils)                  | 公共 POD 类型（event_types / keymap_types）                              |
| [src/ui/](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/ui)                        | SquareLine Studio 生成的 LVGL UI                                         |
| [src/modules/](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/modules)              | I2C 主控（外挂 ModA / ModB 模块）                                        |
| [data/](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/data)                            | SPIFFS 资源：`config.ini`、8 套 `keymap*.ini`、音频 WAV                  |
| [include/](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/include)                      | LVGL 覆盖配置 `lv_conf_local.h`                                          |
| [partitions-4MB.csv](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/partitions-4MB.csv) | 分区表：app0 3MB / spiffs 896KB / nvs 16KB                               |

---

## 2. 输入层（按键与扫描）

### 2.1 矩阵扫描 \[MatrixScanner]

- 物理矩阵 3 行 × 4 列 = 12 个位置
- 第一行（ROW0）只有 3 个按键（COL3 位置空置），其余两行各 4 个按键，**实际物理按键共 11 个**（应用键 ID 1\~11）
- 行引脚：`{46, 39, 38}`；列引脚：`{16, 17, 18, 8}`
- 每键独立消抖状态机：`IDLE → DEBOUNCE_PRESS → PRESSED → DEBOUNCE_RELEASE`
- 消抖时间：`DEBOUNCE_TIME_MS = 10`
- 提供 API：`scan()` / `getStableState()` / `getPressedKeys()` / `getReleasedKeys()`

### 2.2 旋钮（板载 EC11）\[RotaryEncoder]

- 引脚：`CLK=6, DT=7, SW=5`（使用 `ESP32Encoder` PCNT + `OneButton`）
- 旋转阈值：`ENCODER_STEP_THRESOLD=2`，旋转超时 `ROTATION_TIMEOUT=500ms`
- 按键事件：`Click` / `DoubleClick`
- 用途：**仅用于本机屏幕导航**（不进键映射）：
  - 在音乐二级屏 → 上一首 / 下一首 / 播放暂停
  - 在键映射二级屏 → 切换 Profile
  - 在设置二级屏 → 上 / 下导航
  - 其他屏 → `LV_KEY_LEFT/RIGHT/ENTER`

### 2.3 外挂模块（I2C）\[I2CMasterController]

- I2C 端口：SDA=GPIO15, SCL=GPIO8, 频率 100kHz
- 固定从机地址：
  - **ModA** = `0x06`（旋钮 + 滑动电位器模块）
  - **ModB** = `0x08`（机械旋钮模块）
- 协议帧格式（ModA 响应示例）：
  ```
  [I2C_RESPONSE]MODA:Slider1:[min][max][val],Slider2:[min][max][val],
                Knob1:[status][press],Knob2:..,Knob3:..,index=N/over
  ```
- ModA 旋钮：`status` 0=idle/1=左/2=右；`press` 0=idle/1=按下
- ModA 滑动电位器：值变化累计超过阈值（`max(12, range/30)`）触发一次“左/右”事件
- ModB：解析 `MODB:Position=N`，比较前后差值生成 `MODB_KNOB_LEFT/RIGHT`
- 主循环每 3s 扫描设备在线状态、每 30ms 发送 `GETDATA` 拉取一次数据

### 2.4 特殊输入（virtual input id）

[Configuration](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/Configuration.h) 中 `CONFIG_SPECIAL_INPUT_NUM = 15`，已注册的 input id 包含：

- 旋钮：`KNOB1_LEFT/RIGHT/CLICK`、`KNOB2_*`、`KNOB3_*`、`MODB_KNOB_LEFT/RIGHT`
- 滑动：`SLIDER1_LEFT/RIGHT`、`SLIDER2_LEFT/RIGHT`
- 语音触发：`KEY_FUNCTION_ASR`（可由键映射设为任意应用键，默认为应用键 11）

---

## 3. 键映射与配置

### 3.1 键位模型 \[KeyMapping]

每个物理键 / 特殊输入映射支持三种内容（互斥）：

- `function_key`：单个功能字符串（如 `KEY_FUNCTION_ASR`、`MEDIA_PLAY`）
- `normal_key[]`：普通键序列（最多 6 个，支持 `+` 分隔）
- `macros_key[]`：宏键序列（最多 5 个，先压后弹）

支持的键名表（节选，见 `MainTask::keyMapTable`）：

- 字母 a\~z 与 A\~Z（ASCII 自动处理 Shift）
- 数字 0\~9（符号名 `NUM_0` 等）
- 符号：\`Space , . ; ' \[ ] \ / - = \`\`
- 控制：`Enter 0xB0 / Backspace 0xB2 / Tab 0xB3 / Esc 0xB1`
- F1\~F12
- 方向键 / 编辑键 / 锁定键 / PrintScreen / Pause / Menu
- 修饰键：`Ctrl/Shift/Alt/Win`（左 + 右）
- 解析支持 `0xNN` 十六进制与十进制字符串

### 3.2 配置持久化

- 文件：`/config.ini`（默认）+ 8 套 `keymap{N}.ini`（按 Profile 拆分）
- 解析库：`SimpleIni` 4.19
- 主循环互斥：`Configuration::mutex_`（FreeRTOS semaphore）保护读写
- 提供 API：`load()` / `SaveKeyMapping()` / `SaveSetting()` / `loadActiveProfileKeyMapping()` / `switchActiveProfile()`

### 3.3 Profile 切换

- 8 套独立配置：`CONFIG_PROFILE_COUNT = 8`
- 文件路径规则：`Configuration::getProfileConfigPath(idx)` / `getProfileIconPath(idx)`
- 显示名：`Configuration::getProfileDisplayName(idx)`（内置 8 个 LVGL 符号）
- **图标**：每个 Profile 支持自定义 48×48 PNG 图标
  - 上传接口：`CMD_PROFILE_ICON_SET`（桌面 App → 主控）
  - 校验：仅接受 48×48 PNG，base64 解码后写入 SPIFFS
  - 显示端使用 `lodepng` 解码为 RGBA（`lodepng_bridge.c`）

### 3.4 触发映射到 HID

[handleKeyEvent](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/MainTask.cpp#L602-L682) 流程：

1. `currentKeyboard_->releaseAll()` 先释放全部键
2. 遍历 11 个应用键，按键索引取出 `KeyMapping`
3. 先发 macros_key → function_key（或 normal_key 序列）
4. 触发物理按键边沿会向桌面 App 上报 `CMD_KEY_EVENT`（用于屏幕点击高亮）

---

## 4. 键盘输出（多模式）

### 4.1 三种工作模式 \[Configuration::WORK_MODE]

| 模式                              | 实现                                                                                                              | 备注                                    |
| --------------------------------- | ----------------------------------------------------------------------------------------------------------------- | --------------------------------------- |
| `WIRED_KEYBOARD_MODE` (0)         | [USBKeyboardImpl](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/USBKeyboardImpl.h) | TinyUSB HID Keyboard + Consumer Control |
| `BLUETOOTH_KEYBOARD_MODE` (1)     | [BLEKeyboardImpl](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/BLEKeyboardImpl.h) | 启用 BLE 后释放经典蓝牙内存             |
| `WIRELESS_2_4G_KEYBOARD_MODE` (2) | **未实现**（代码仅 `LOG_WARNING`）                                                                                | <br />                                  |
| `NONE_MODE`                       | 占位                                                                                                              | 启动前                                  |

切换时统一接口 [IKeyboard](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/IKeyboard.h)：`begin/press(uint8_t|String)/release/releaseAll/isConnected/send`

### 4.2 模式切换时的副作用

- 进入 BLE 模式：调用 `esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)` 释放经典蓝牙内存
- 进入 BLE 模式：自动关闭 WiFi 重连（BLE 模式下内存紧张）
- WiFi 开启时：非 BLE 模式才调度连接

---

## 5. 私有协议（与桌面 App 通信）

### 5.1 传输层 \[SerialProtocol]

- 双通道：USB CDC Serial + TCP（自动发现）
- USB CDC 115200 波特率（UART0 已不再承担日志，仅作烧录用）
- 命令按 JSON 行解析
- 响应命令约定：`cmd | 0x80` 为响应包

### 5.2 上位机自动发现（UDP）

- 端口 30001，广播 `FUNKEYBOARD_DISCOVER`
- 桌面 App 应答 `FUNKEYBOARD_HERE`
- 超时 800ms，失败则回退 IP `192.168.31.1`
- 控制通道 TCP 端口 30000

### 5.3 命令清单 \[SerialProtocol::CommandType]

| ID   | 命令                   | 来源     | 实现                                                                                                                                      |
| ---- | ---------------------- | -------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| 0x01 | `CMD_CONF_VERSION_GET` | App→主控 | [cmd_firmware.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_firmware.cpp)       |
| 0x02 | `CMD_CONF_VERSION_SET` | App→主控 | 未在源码中检索到 handler                                                                                                                  |
| 0x03 | `CMD_DEVICE_INFO_GET`  | App→主控 | [cmd_device_info.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_device_info.cpp) |
| 0x04 | `CMD_DEVICE_INFO_SET`  | App→主控 | 未在源码中检索到 handler                                                                                                                  |
| 0x05 | `CMD_KEYMAP_GET`       | App→主控 | [cmd_keymap.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_keymap.cpp)           |
| 0x06 | `CMD_KEYMAP_SET`       | App→主控 | 同上（写入 + 持久化）                                                                                                                     |
| 0x07 | `CMD_CONFIG_GET`       | App→主控 | [cmd_config.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_config.cpp)           |
| 0x08 | `CMD_CONFIG_SET`       | App→主控 | 同上（解析所有设置项）                                                                                                                    |
| 0x09 | `CMD_KEY_EVENT`        | 主控→App | 按键边沿上报                                                                                                                              |
| 0x0a | `CMD_HEARTBEAT`        | 双向     | SerialProtocol 自处理                                                                                                                     |
| 0x0b | `CMD_FIRMWARE_INFO`    | App→主控 | [cmd_firmware.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_firmware.cpp)       |
| 0x0c | `CMD_VOICE_TEXT`       | 主控→App | ASR 文本推送                                                                                                                              |
| 0x0d | `CMD_PC_STATUS`        | App→主控 | [cmd_pc_status.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_pc_status.cpp)     |
| 0x0e | `CMD_MUSIC_STATUS`     | App→主控 | [cmd_music.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_music.cpp)             |
| 0x0f | `CMD_MUSIC_CONTROL`    | 主控→App | UI 控制按钮触发                                                                                                                           |
| 0x10 | `CMD_PROFILE_STATE`    | 双向     | [cmd_profile.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_profile.cpp)         |
| 0x11 | `CMD_PROFILE_ICON_SET` | App→主控 | 同上（PNG base64 上传）                                                                                                                   |
| 0x12 | `CMD_HA_STATUS`        | 主控→App | 周期推送状态聚合                                                                                                                          |

### 5.4 注册机制

[registerAllCommandHandlers](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/registration.cpp#L20-L51) 在 `main_task.begin()` 之后统一注册：

- Phase 2.5：`cmd_config`
- Phase 2.6：`cmd_keymap` / `cmd_profile` / `cmd_pc_status` / `cmd_music`
- Phase 2.8：`cmd_device_info` / `cmd_firmware`
- 注册表采用 `std::array<Entry, 64>` + 临界区保护，零堆分配

---

## 6. 桌面 App / 主控设置（`CMD_CONFIG_SET`）

`parseConfigSetCommand` 支持原子写入以下字段：

- **WiFi / 主机连接**：`wifi_switch`、`connect_host`、`wifi_ssid`、`wifi_password`（变化时立即调度连接 / 断开）
- **工作模式**：`work_mode`（触发 `setWorkMode()` 重建键盘实例）
- **RGB LED**：`rgb_mode`、`rgb_single_colar`、`rgb_click_mode`、`rgb_brightness`
- **屏幕**：`tft_theme`、`tft_brightness`（下限 5，避免 OLED 烧屏）
- **音频**：`device_volume`（联动 `speaker_.SetVolume(volume/5)`）、`audio_enable`、`power_mode`
- **语音**：`voice_enable`、`voice_trigger_key`、`voice_max_record_ms`、`voice_auto_enter`、`voice_dev_pid`、`voice_cuid`、`voice_baidu_api_key`、`voice_baidu_secret_key`
- **PC 状态**：`pc_status_mask`
- **Profile 切换**：`active_keymap_profile`（写入后刷新 UI + 上报完整状态）

设置变更后会向 `DisplayTask` 推送 `SETTING_UPDATE`，并自动重算语音运行时。

---

## 7. 网络层

### 7.1 WiFi 管理 \[MainTask]

- 默认开启（`wifi_switch = false`）
- 触发场景：配置变更 / 启动时 `scheduleWiFiConnectAttempt()`
- 重连策略：每 5s 重试一次 (`kWifiRetryIntervalMs`)，单次超时 10s (`kWifiConnectTimeoutMs`)
- 断线检测：若 TCP 15s 仍未恢复，强制重启 WiFi
- BLE 模式自动停用 WiFi（避免内存竞争）
- API：`ConnectToWiFi()` / `scheduleWiFiConnectAttempt()` / `stopWiFiReconnect()` / `processWiFiReconnect()`

### 7.2 NTP 同步

- 服务器：`pool.ntp.org`，GMT+8
- 触发：WiFi 连上后调用 `SyncTimeFromNTP()`
- 使用 `settimeofday` 写入系统时钟（用于 UI 时间显示）

### 7.3 TCP 客户端

- 自动发现：UDP 广播（见 5.2）
- 控制台指令通过同一 `SerialProtocol` 复用（Serial / TCP 任一在线即生效）

---

## 8. 显示与 UI

### 8.1 LVGL 屏（SquareLine 生成）

定义在 [ui.h](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/ui/ui.h) 中，共 11 屏：

| Screen Tag                                              | 中文用途                                               |
| ------------------------------------------------------- | ------------------------------------------------------ |
| `UI_SCREEN_MAIN`                                        | 主屏（时间 / 状态条 / 工作模式）                       |
| `UI_SCREEN_KEYMAPPED`                                   | 键映射概览（11 应用键 + Profile 图标）                 |
| `UI_SCREEN_KEYMAPPED_SECONDARY`                         | 键映射详情（每个键的文字标签）                         |
| `UI_SCREEN_MUSIC` / `UI_SCREEN_MUSIC_SECONDARY`         | 音乐控制（标题 / 艺人 / 歌词 / 进度条）                |
| `UI_SCREEN_PC_STATUS` / `UI_SCREEN_PC_STATUS_SECONDARY` | PC 状态（CapsLock / 网络 / CPU / 内存 / 温度）         |
| `UI_SCREEN_HA` / `UI_SCREEN_HA_SECONDARY`               | HA 状态聚合（WiFi / TCP / 模块 / 语音）                |
| `UI_SCREEN_SETTING` / `UI_SCREEN_SETTING_SECONDARY`     | 设置（work mode / RGB / 屏幕 / 音量 / 电源 / Profile） |

字体：内置 BebasNeue 与自定义中文 `FontCKJGT`（多尺寸 16/24/28/32/40/48/64/80，部分尺寸因空间被 build filter 排除）。

### 8.2 状态条 \[ui_StatusBar]

- 显示工作模式（WIR / BLT / 2.4）
- 音量条
- WiFi / TCP 状态
- ModA / ModB 在线指示
- 录音中动画

### 8.3 主任务 → 显示任务消息

[DisplayMessage](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/message_types.h#L54-L77) 包含 8 种类型：

1. `ACTION_INPUT`（旋钮 / 设置键 → LVGL `LV_EVENT_KEY`）
2. `KEY_INPUT`（用于点亮按键 RGB）
3. `SETTING_UPDATE`
4. `MODULE_STATUS`
5. `ASR_RECORDING_STATE`
6. `PC_STATUS_UPDATE`
7. `HA_STATUS_UPDATE`
8. `MUSIC_PLAYER_UPDATE`
9. `KEYMAP_PROFILE_UPDATE`

### 8.4 设置 UI 反向同步

- LVGL 设置屏修改后调用 `ui_settings_request_apply()` / `ui_settings_request_save()`
- 通过临界区 `g_ui_settings_lock` 投递 `ui_settings_snapshot_t` 给 MainTask
- MainTask 通过 `consumeUiSettingsRequest()` → `applyUiSettingsSnapshot()` 写回 `DeviceSettings`

---

## 9. RGB 灯光 \[RGBLightControl]

- 硬件：WS2812B × 11 颗（GRB 顺序），信号 IO15，电源使能 IO21
- 亮度：`SetBrightness(0~100)`
- 模式（`RGB_MODE` 枚举）：
  - `RGB_NONE_MODE` 关灯
  - `RGB_SINGLE_MODE` 单色（24 色调色板索引）
  - `RGB_RAINBOW_MODE` / `RGB_RAINBOWWARE_MODE` 彩虹
  - `RGB_COLORCYCLE_MODE` 颜色循环
  - `RGB_METER_MODE`（实现存在但 mainTask 未调度）
  - `RGB_FIRE_MODE` 火焰
  - `RGB_PULSE_MODE` 呼吸
- 按键点击高亮（`RGB_CLICK_MODE`）：
  - `CLICK_NONE_COLOR_MODE` 不响应
  - `CLICK_SINGLE_COLOR_MODE` 按下点亮对应键、抬起熄灭
  - `CLICK_WARE_COLOR_MODE`（保留）
- 显示任务负责驱动 LED 动画循环（`run()` 内 tick）

---

## 10. 音频

### 10.1 喇叭 \[Speaker]

- I2S 引脚：BCLK=16, LRC=39, DOUT=38（MAX98357）
- 音量：`SetVolume(0~21)` = `device_volume / 5`
- 播放能力：
  - `PlayRemoteAudio(url)` / `PlayLocalAudio(path)`
  - 支持暂停 / 恢复 / 停止 / 进度查询
- 数据源：本地 WAV（`data/coin2.wav`、`dino_jump.wav`），启动时播放一次 `coin2.wav`

### 10.2 麦克风 \[Mic]

- I2S 引脚：BCLK=IO10, WS=IO12, SCK=IO13, SDOUT(到 ESP32)=IO11（ICS43434）
- 采样率 16kHz，缓冲 512 samples
- 当前 main 循环未启用麦克风读取（代码注释保留）
- 语音识别所需的麦克风由 [VoiceRecognizer](#111-语音识别) 自行管理（使用同一 Mic 通道）

### 10.3 频谱分析 \[AudioAnalyzer]（保留代码但未启用）

- FFT_SIZE=512，BANDS=16，使用 `arduinoFFT`
- DC 去除 + 汉宁窗 → 16 个频段
- 预留接口 `process()` / `getBands()`，可后续用于音乐频谱屏

---

## 11. 语音识别 \[VoiceRecognizer]

### 11.1 引擎与协议

- 后端：百度短语音 ASR REST API（dev_pid 默认 1537 = 普通话）
- 凭证：`voice_baidu_api_key` + `voice_baidu_secret_key`
- cuid：`FunModularKeyboard`
- token 管理：自动获取 / 缓存，失败退避

### 11.2 触发流程

1. 应用键（默认 11）按下 → `voiceTriggerBit_` 命中 → `startVoiceCapture()`
2. `Mic.Begin()` → `voiceRecognizer_.startCapture()` → 推 `ASR_RECORDING_STATE=true`
3. 主循环 `voiceRecognizer_.feedCapture()` 持续喂 PCM
4. 触发键松开 → `finishVoiceCapture()` → 调 ASR
5. 结果：
   - 优先通过 `CMD_VOICE_TEXT` 推给桌面 App（支持 UTF-8 中文）
   - 若上位机未连接且文本是 ASCII，`sendAsciiTextToHost()` 兜底 HID 注入（按字符 delay 8ms）
6. 可选 `voice_auto_enter=1` 时追加回车

### 11.3 限制

- 仅在 `WIRED_KEYBOARD_MODE` + WiFi 已连接 + 语音启用时工作
- 音乐屏 (`UI_SCREEN_MUSIC*`) 进入会 `suspend()` ASR，离开自动 `resume()`
- 录音时长限制 1000\~8000ms

---

## 12. PC 状态（来自桌面 App 上报）

`CMD_PC_STATUS`（桌面 App 主动推送）解析后填充 [PcStatusInfo](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/message_types.h#L8-L22)：

- 锁键状态：`caps_lock / num_lock / scroll_lock`
- 网络：`network_connected / network_up_kbps / network_down_kbps`
- 电源：`on_ac_power / battery_percent`
- 性能：`cpu_usage_percent / memory_usage_percent / cpu_temp_c / disk_io_percent`
- 过滤：负值字段显示 `--`

UI 展示在 `UI_SCREEN_PC_STATUS(_SECONDARY)`。

---

## 13. 音乐控制

### 13.1 上行（UI → 桌面 App）

- `CMD_MUSIC_CONTROL`（主控发送）：`previous` / `toggle` / `next`
- 触发源：
  - 二级音乐屏按钮
  - 旋钮（在音乐二级屏映射为上 / 切 / 下）
  - 应用键映射的 `MEDIA_PLAY` / `MEDIA_NEXT_TRACK` 等

### 13.2 下行（桌面 App → 主控）

- `CMD_MUSIC_STATUS`：更新 `MusicPlayerInfo`
  - 标题、艺人、播放器名、当前/下一句歌词
  - 播放状态、进度、上下首按钮可用性
- 主控本地进度：每 1000ms 自增 `current_seconds`（用于 App 离线时的本地回放）
- 超时处理：30s 未收到 MUSIC_STATUS 自动标记 `PLAYER OFFLINE`

### 13.3 ASR 与音乐的互斥

- `updateMusicUiAsrOwnership()` 自动管理录音状态机

---

## 14. HA 状态聚合屏

`SendHaStatusSnapshot()` 每 2.5s 推送一次 `HaStatusInfo`：

- WiFi（启用 / 已连 / RSSI / IP）
- 与桌面 App 的 TCP 连接状态 + endpoint
- 当前工作模式 / 语音启用与录音状态
- ModA / ModB 在线指示
- 显示在 `UI_SCREEN_HA(_SECONDARY)`

---

## 15. 日志系统 \[LogManager]

- 输出：默认 115200 串口
- 级别：DEBUG / INFO / WARNING / ERROR
- 可注册回调（用于转 SPIFFS / Web 等，当前未启用）
- 全局宏：`LOG_DEBUG(tag, fmt, ...)` 等

---

## 16. 电源与 GPIO

- 升压 5V（外挂模块供电）使能引脚：`GPIO3`（`kBoost5VEnablePin`）
- 仅在 `NORMAL_POWER_MODE` 时开启 5V；其它模式关闭以省电
- 模式定义：`NORMAL / LOW_POWER / LIGHT_POWER / DEEPSLEEP_POWER_MODE`
  - 当前仅 NORMAL 模式有实际动作，其它模式仅占位（预留）

---

## 17. 待办 / 未完成项（阶段 07 已全部补齐，联调验证待执行）

| 项                                                                                                           | 状态                              | 位置                                  |
| ------------------------------------------------------------------------------------------------------------ | --------------------------------- | ------------------------------------- |
| 2.4G 无线键盘模式                                                                                            | 已实现（硬件未到位时安全回退 USB） | `Wireless24GKeyboardImpl` + `IRadio24G` |
| 麦克风频谱显示                                                                                               | 已实现（音乐屏可见时调度）         | `DisplayTask::updateSpectrum` 与 `AudioAnalyzer` |
| OTA 升级类 [Upgrade](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/Upgrade.h) | 已实现（HTTP + MD5 校验）          | `src/upgrade/Upgrade.cpp`（0x0b 携带 url/checksum 触发） |
| CMD_CONF_VERSION_SET / CMD_DEVICE_INFO_SET                                                                   | 已注册 handler（写 DeviceSettings + INI 持久化） | `cmd_firmware.cpp` / `cmd_device_info.cpp` |
| WiFi STA 自动重连精细策略                                                                                    | 已实现（15s 宽限后强制重启；BLE 模式整体短路） | `processWiFiReconnect`                |

---

## 18. 关键路径参考

- **启动流程**：[main.cpp](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/main.cpp) → 加载 SPIFFS / Config → 启动 [MainTask](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/MainTask.cpp#L2109-L2512) 与 [DisplayTask](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/DisplayTask.cpp)
- **按键 → 桌面**：[MatrixScanner::scan](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/MatrixScanner.cpp) → MainTask 边沿识别 → HID 注入 + `CMD_KEY_EVENT` 上报
- **设置同步链路**：
  - App → [SerialProtocol](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/SerialProtocol.cpp) → [CommandRegistry](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/CommandRegistry.cpp) → [cmd_config](file:///D:/search/gitcode/FunModularKeyboard/firmware/FunModularKeyboard/src/protocol/commands/cmd_config.cpp) → MainTask → `parseConfigSetCommand` → `SendDisplaySetting` 队列 → DisplayTask → LVGL
  - 反向：LVGL → `ui_settings_request_*` → MainTask → `applyUiSettingsSnapshot`

---

文档基于当前仓库代码静态梳理，逻辑与接口如后续重构，请以代码为准。
