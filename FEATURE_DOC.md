# EKeys 固件功能文档

> 适用代码版本：ESP32-S3-WROOM-1（N16R8，16MB Flash / 8MB PSRAM / OPI PSRAM），Arduino 框架，PlatformIO 6.x
> 仓库路径：`d:\workspace\zheteng\ESP_Projects\EKeys`

---

## 1. 项目概览

### 1.1 硬件平台

- **MCU**：ESP32-S3-WROOM-1 模组（N16R8 = 16MB Flash / 8MB Octal PSRAM，PSRAM 已启用）
- **USB**：TinyUSB CDC + HID（USB 键盘 + 消费控制 + 串口）
- **无线**：Wi-Fi STA、蓝牙 BLE（`t-vk/ESP32 BLE Keyboard`）
- **存储**：SPIFFS 文件系统（分区 3.875 MB）+ NVS
- **显示**：Arduino_GFX 驱动 NV3007（428×142 SPI LCD）+ LVGL 8.3.11（SquareLine Studio 生成 UI）
- **音频**：ESP32-audioI2S（MAX98357 喇叭，ICS43434 麦克风）

### 1.2 软件架构

- **双 FreeRTOS 任务**：
  - [MainTask](src/tasks/MainTask.h) —— 主任务（Core 1，栈 12288）：按键扫描、WiFi/BLE、I2C、协议收发、语音触发
  - [DisplayTask](src/tasks/DisplayTask.h) —— 显示任务（Core 0）：LVGL tick、RGB LED、频谱动画、消息处理
- **任务间通信**：`xQueue`（`DisplayMessage` 队列，长度 10）
- **协议命令分发**：[CommandRegistry](src/protocol/CommandRegistry.h) 单例 + [protocol::commands](src/protocol/registration.cpp) 模块化注册
- **事件总线**：[EventBus](src/services/EventBus.h)（订阅/发布同步分发）
- **持久化**：[Configuration](src/config/Configuration.h) + [KeymapRepository](src/services/KeymapRepository.h) + [ConfigStore](src/services/ConfigStore.h)，基于 `SimpleIni` 读写 SPIFFS

### 1.3 文件 / 目录结构

| 目录                            | 说明                                                                     |
| ------------------------------- | ------------------------------------------------------------------------ |
| [src/](src)                     | 主源码                                                                   |
| [src/protocol/](src/protocol)   | 私有协议（CommandRegistry + 各命令模块）                                 |
| [src/services/](src/services)   | 持久化服务（ConfigStore / KeymapRepository / EventBus）                  |
| [src/utils/](src/utils)         | 公共 POD 类型（event_types / keymap_types）                              |
| [src/ui/](src/ui)               | SquareLine Studio 生成的 LVGL UI                                         |
| [data/](data)                   | SPIFFS 资源：`config.ini`、`keymapN.ini`、`audio_pad.ini`、预置音频      |
| [include/](include)             | LVGL 覆盖配置 `lv_conf.h`                                                |

完整目录与子系统职责见 [ARCHITECTURE.md §3](ARCHITECTURE.md)。

---

## 2. 输入层（按键与扫描）

### 2.1 矩阵扫描 [MatrixScanner]

- 物理矩阵 3 行 × 4 列 = 12 个位置
- 第一行（ROW0）只有 3 个按键（COL3 位置空置），其余两行各 4 个按键，**实际物理按键共 11 个**（应用键 ID 1~11）
- 行引脚：`{46, 39, 38}`；列引脚：`{16, 17, 18, 8}`（详见 [PINOUT.md §2.5](PINOUT.md)）
- 每键独立消抖状态机：`IDLE → DEBOUNCE_PRESS → PRESSED → DEBOUNCE_RELEASE`
- 消抖时间：`DEBOUNCE_TIME_MS = 10`
- 提供 API：`scan()` / `getStableState()` / `getPressedKeys()` / `getReleasedKeys()`

### 2.2 旋钮（板载 EC11）[RotaryEncoder]

- 引脚：`CLK=6, DT=7, SW=5`（使用 `ESP32Encoder` PCNT + `OneButton`）
- **用途：仅用于本机屏幕导航**（不进键映射）：
  - 顺时针 → `LV_KEY_RIGHT`
  - 逆时针 → `LV_KEY_LEFT`
  - 单击 → `LV_KEY_ENTER`
  - 双击 → `LV_KEY_ESC`
- 完整 UI 输入语义、矩阵键 101~111 编码、DisplayTask 路由见 `.trae/rules/rules.md`。

### 2.3 外挂模块（I2C）[I2CMasterController]

- I2C 端口：SDA=GPIO47, SCL=GPIO48，频率 100kHz
- 固定从机地址：
  - **ModA** = `0x06`（旋钮 + 滑动电位器模块）
  - **ModB** = `0x08`（机械旋钮模块）
- 协议帧格式（ModA 响应示例）：
  ```
  [I2C_RESPONSE]MODA:Slider1:[min][max][val],Slider2:[min][max][val],
                Knob1:[status][press],Knob2:..,Knob3:..,index=N/over
  ```
- ModA 旋钮：`status` 0=idle/1=左/2=右；`press` 0=idle/1=按下
- 主循环每 3s 扫描设备在线状态、每 30ms 发送 `GETDATA` 拉取一次数据

### 2.4 特殊输入（virtual input id）

`Configuration::CONFIG_SPECIAL_INPUT_NUM = 15`，已注册的 input id 包含：

- 旋钮：`KNOB1_LEFT/RIGHT/CLICK`、`KNOB2_*`、`KNOB3_*`、`MODB_KNOB_LEFT/RIGHT`
- 滑动：`SLIDER1_LEFT/RIGHT`、`SLIDER2_LEFT/RIGHT`
- 语音触发：`KEY_FUNCTION_ASR`（可由键映射设为任意应用键，默认为应用键 11）

---

## 3. 键映射与配置

### 3.1 键位模型 [KeyMapping]

每个物理键 / 特殊输入映射支持四种内容（互斥 + FUN 组合层）：

- `function_key`：单个功能字符串（如 `KEY_FUNCTION_ASR`、`MEDIA_PLAY`）
- `normal_key[]`：普通键序列（最多 6 个，支持 `+` 分隔）
- `macros_key[]`：宏键序列（最多 5 个，先压后弹）
- `text`：文本注入串（≤128 字符，HID 注入，仅 ASCII）

优先级：`function > text > normal > macro`。

支持的键名表（节选）：

- 字母 a~z 与 A~Z（ASCII 自动处理 Shift）
- 数字 0~9（符号名 `NUM_0` 等）
- 控制键 / F1~F12 / 方向键 / 编辑键
- 修饰键：`Ctrl/Shift/Alt/Win`（左 + 右）
- 解析支持 `0xNN` 十六进制与十进制字符串

### 3.2 配置持久化

- 文件：`/config.ini`（默认）+ 8 套 `keymap{N}.ini`（按 Profile 拆分）+ `/audio_pad.ini`（音效板绑定）
- 解析库：`SimpleIni` 4.19
- 主循环互斥：`Configuration::mutex_`（FreeRTOS semaphore）保护读写
- 提供 API：`load()` / `SaveKeyMapping()` / `SaveSetting()` / `loadActiveProfileKeyMapping()` / `switchActiveProfile()`

### 3.3 Profile 切换

- 8 套独立配置：`CONFIG_PROFILE_COUNT = 8`
- 文件路径规则：`Configuration::getProfileConfigPath(idx)` / `getProfileIconPath(idx)`
- 显示名：`Configuration::getProfileDisplayName(idx)`（内置 8 个 LVGL 符号，可通过 `CMD_PROFILE_NAME_SET` 0x15 改写为 UTF-8 中文）
- **图标**：每个 Profile 支持自定义 48×48 PNG 图标
  - 上传接口：`CMD_PROFILE_ICON_SET`（0x11，base64 解码后写入 SPIFFS）
  - 显示端使用 `lodepng` 解码为 RGBA

### 3.4 触发映射到 HID

`handleKeyEvent` 流程：

1. `currentKeyboard_->releaseAll()` 先释放全部键
2. 遍历 11 个应用键，按键索引取出 `KeyMapping`
3. 先发 macros_key → function_key（或 normal_key 序列 / text 注入）
4. FUN 组合层（fun_key1 / fun_key2）：FUN 键先按时其它键触发 combo1/combo2，松开发；FUN 键本身不产生 HID

---

## 4. 键盘输出（多模式）

### 4.1 三种工作模式 [Configuration::WORK_MODE]

| 模式                              | 实现                                    | 备注                                     |
| --------------------------------- | --------------------------------------- | ---------------------------------------- |
| `WIRED_KEYBOARD_MODE` (0)         | [USBKeyboardImpl](src/output/USBKeyboardImpl.h) | TinyUSB HID Keyboard + Consumer Control |
| `BLUETOOTH_KEYBOARD_MODE` (1)     | [BLEKeyboardImpl](src/output/BLEKeyboardImpl.h) | 启用 BLE 后释放经典蓝牙内存              |
| `WIRELESS_2_4G_KEYBOARD_MODE` (2) | **未实现**（代码仅 `LOG_WARNING`）      | 选择后回退 USB                           |

切换时统一接口 [IKeyboard](src/output/IKeyboard.h)：`begin/press(uint8_t|String)/release/releaseAll/isConnected/send`。

### 4.2 模式切换时的副作用

- 进入 BLE 模式：调用 `esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)` 释放经典蓝牙内存
- 进入 BLE 模式：自动关闭 WiFi 重连（BLE 模式下内存紧张）
- WiFi 开启时：非 BLE 模式才调度连接

---

## 5. 私有协议（与桌面 App 通信）

### 5.1 传输层 [SerialProtocol]

- 双通道：USB CDC Serial + TCP（自动发现）
- USB CDC 115200 波特率（UART0 已不再承担日志，仅作烧录用）
- 命令按 JSON 行解析
- 响应命令约定：`cmd | 0x80` 为响应包（`CMD_PROFILE_STATE` 0x10 为当前例外）
- 行缓冲 2048 字节；TCP 在线时帧只走 TCP（串口不镜像，防心跳刷屏）

### 5.2 上位机自动发现（UDP）

- 端口 30001，广播 `FUNKEYBOARD_DISCOVER`（设备→App）
- 桌面 App 应答 `FUNKEYBOARD_HERE`
- 单轮超时 800ms，最多 3 轮，失败回退 IP `192.168.31.1`
- 控制通道 TCP 端口 30000

### 5.3 命令清单 [SerialProtocol::CommandType]

| ID       | 命令                    | 方向          | 实现                                                                                       |
| -------- | ----------------------- | ------------- | ------------------------------------------------------------------------------------------ |
| `0x01`   | `CMD_CONF_VERSION_GET`  | App→主控      | [cmd_firmware.cpp](src/protocol/commands/cmd_firmware.cpp)                                 |
| `0x02`   | `CMD_CONF_VERSION_SET`  | App→主控      | 同上                                                                                       |
| `0x03`   | `CMD_DEVICE_INFO_GET`   | App→主控      | [cmd_device_info.cpp](src/protocol/commands/cmd_device_info.cpp)                           |
| `0x04`   | `CMD_DEVICE_INFO_SET`   | App→主控      | 同上                                                                                       |
| `0x05`   | `CMD_KEYMAP_GET`        | App→主控      | [cmd_keymap.cpp](src/protocol/commands/cmd_keymap.cpp)                                     |
| `0x06`   | `CMD_KEYMAP_SET`        | App→主控      | 同上（写入 + 持久化）                                                                      |
| `0x07`   | `CMD_CONFIG_GET`        | App→主控      | [cmd_config.cpp](src/protocol/commands/cmd_config.cpp)                                     |
| `0x08`   | `CMD_CONFIG_SET`        | App→主控      | 同上（解析所有设置项）                                                                     |
| `0x09`   | `CMD_KEY_EVENT`         | 主控→App      | 当前未实现序列化发送                                                                       |
| `0x0a`   | `CMD_HEARTBEAT`         | 双向          | SerialProtocol 自处理                                                                      |
| `0x0b`   | `CMD_FIRMWARE_INFO`     | App→主控      | [cmd_firmware.cpp](src/protocol/commands/cmd_firmware.cpp)（查询 / OTA）                     |
| `0x0c`   | `CMD_VOICE_TEXT`        | 主控→App      | ASR 文本推送                                                                               |
| `0x0d`   | `CMD_PC_STATUS`         | App→主控      | [cmd_pc_status.cpp](src/protocol/commands/cmd_pc_status.cpp)                               |
| `0x0e`   | `CMD_MUSIC_STATUS`      | App→主控      | [cmd_music.cpp](src/protocol/commands/cmd_music.cpp)                                       |
| `0x0f`   | `CMD_MUSIC_CONTROL`     | 主控→App      | UI 控制按钮触发                                                                            |
| `0x10`   | `CMD_PROFILE_STATE`     | 双向          | [cmd_profile.cpp](src/protocol/commands/cmd_profile.cpp)                                   |
| `0x11`   | `CMD_PROFILE_ICON_SET`  | App→主控      | 同上（PNG base64 上传）                                                                    |
| `0x12`   | `CMD_HA_STATUS`         | 主控→App      | 当前未实现协议序列化（仅本机 HA 屏消费）                                                   |
| `0x13`   | `CMD_TIME_SET`          | App→主控      | [cmd_time.cpp](src/protocol/commands/cmd_time.cpp)（epoch + tz 写入系统时间）               |
| `0x14`   | `CMD_FIRMWARE_DOWNLOAD` | App→主控      | [cmd_firmware.cpp](src/protocol/commands/cmd_firmware.cpp)（复位进 USB 下载模式烧录）       |
| `0x15`   | `CMD_PROFILE_NAME_SET`  | App→主控      | [cmd_profile.cpp](src/protocol/commands/cmd_profile.cpp)（UTF-8 中文名称）                  |
| `0x16`   | `CMD_AUDIO_FILE`        | App→主控      | [cmd_audio.cpp](src/protocol/commands/cmd_audio.cpp)（音效文件管理，data.op 分发）          |
| `0x17`   | `CMD_AUDIO_PAD`         | App→主控      | 同上（音效板绑定与播放控制）                                                               |

### 5.4 注册机制

`registerAllCommandHandlers()` 在 `main_task.begin()` 之后统一注册：

- Phase 2.5：`cmd_config`
- Phase 2.6：`cmd_keymap` / `cmd_profile` / `cmd_pc_status` / `cmd_music`
- Phase 2.8：`cmd_device_info` / `cmd_firmware`
- Phase 09：`cmd_time` / `cmd_audio`
- 注册表采用 `std::array<Entry, 64>` + 临界区保护，零堆分配

完整协议细节、字段、报文示例见 [docs/desktop-app-protocol.md](docs/desktop-app-protocol.md)。

---

## 6. 桌面 App / 主控设置（`CMD_CONFIG_SET`）

`parseConfigSetCommand` 支持原子写入以下字段：

- **WiFi / 主机连接**：`wifi_switch`、`connect_host`、`wifi_ssid`、`wifi_password`
- **工作模式**：`work_mode`（触发 `setWorkMode()` 重建键盘实例）
- **RGB LED**：`rgb_mode`、`rgb_single_color`、`rgb_click_mode`、`rgb_brightness`
- **屏幕**：`tft_theme`、`tft_brightness`（下限 5，避免 OLED 烧屏）
- **音频**：`device_volume`（联动 `Speaker::SetVolume(volume/5)`）、`audio_enable`、`power_mode`
- **语音**：`voice_enable`、`voice_trigger_key`、`voice_max_record_ms`、`voice_auto_enter`、`voice_cuid`、`voice_tencent_secret_id`、`voice_tencent_secret_key`
- **PC 状态**：`pc_status_mask`
- **Profile 切换**：`active_keymap_profile`（写入后刷新 UI + 上报完整状态）

设置变更后会向 `DisplayTask` 推送 `SETTING_UPDATE`，并自动重算语音运行时。

---

## 7. 网络层

### 7.1 WiFi 管理 [MainTask]

- 默认关闭（`wifi_switch = false`）
- 重连策略：每 5s 重试一次 (`kWifiRetryIntervalMs`)，单次超时 10s (`kWifiConnectTimeoutMs`)
- 断链宽限：`kWifiLinkGraceMs = 15000`，超时强制重启射频
- BLE 模式自动停用 WiFi（避免内存竞争）
- API：`ConnectToWiFi()` / `scheduleWiFiConnectAttempt()` / `stopWiFiReconnect()` / `processWiFiReconnect()`

### 7.2 NTP 同步

- 服务器：`pool.ntp.org`，默认 GMT+8（POSIX TZ `CST-8`）
- 触发：WiFi 连上后调用 `SyncTimeFromNTP()`
- 使用 `settimeofday` 写入系统时钟（用于 UI 时间显示）
- 桌面 App 可用 `CMD_TIME_SET` (0x13) 主动覆盖

### 7.3 TCP 客户端

- 自动发现：UDP 广播（见 5.2）
- 控制台指令通过同一 `SerialProtocol` 复用（Serial / TCP 任一在线即生效）
- 重连状态机详见 [docs/desktop-app-protocol.md §2.2](docs/desktop-app-protocol.md)

---

## 8. 显示与 UI

### 8.1 LVGL 屏（SquareLine 生成）

定义在 [ui.h](src/ui/ui.h) 中，共 12 屏：

| Screen Tag                                              | 中文用途                                       |
| ------------------------------------------------------- | ---------------------------------------------- |
| `UI_SCREEN_MAIN`                                        | 主屏（时间 / 状态条 / 工作模式）               |
| `UI_SCREEN_KEYMAPPED`                                   | 键映射概览（11 应用键 + Profile 图标）         |
| `UI_SCREEN_KEYMAPPED_SECONDARY`                         | 键映射详情（每个键的文字标签；FUN 组合层）     |
| `UI_SCREEN_MUSIC` / `UI_SCREEN_MUSIC_SECONDARY`         | 音乐控制（标题 / 艺人 / 歌词 / 进度条）        |
| `UI_SCREEN_AUDIO` / `UI_SCREEN_AUDIO_SECONDARY`         | 音效板（11 键本地音频绑定 + 播放状态）         |
| `UI_SCREEN_PC_STATUS` / `UI_SCREEN_PC_STATUS_SECONDARY` | PC 状态（CapsLock / 网络 / CPU / 内存 / 温度） |
| `UI_SCREEN_HA` / `UI_SCREEN_HA_SECONDARY`               | HA 状态聚合（WiFi / TCP / 模块 / 语音）        |
| `UI_SCREEN_SETTING` / `UI_SCREEN_SETTING_SECONDARY`     | 设置（work mode / RGB / 屏幕 / 音量 / 电源 / Profile / FUN 键） |

导航环（旋钮 LEFT/RIGHT）：主屏 → 键映射 → 音乐 → **音效** → PC状态 → HA → 设置 → 主屏。

字体：内置 BebasNeue 与自定义中文 `FontCKJGT`（多尺寸 16/24/28/32/40/48/64/80，部分尺寸因空间被 build filter 排除）。

### 8.2 状态条 [ui_StatusBar]

- 显示工作模式（WIR / BLT / 2.4）
- 音量条
- WiFi / TCP 状态
- ModA / ModB 在线指示
- 录音中动画

### 8.3 主任务 → 显示任务消息

[DisplayMessage](src/message_types.h) 包含以下类型：

1. `ACTION_INPUT`（旋钮 / 设置键 → LVGL `LV_EVENT_KEY`）
2. `KEY_INPUT`（用于点亮按键 RGB）
3. `SETTING_UPDATE`
4. `MODULE_STATUS`
5. `ASR_RECORDING_STATE`
6. `PC_STATUS_UPDATE`
7. `HA_STATUS_UPDATE`
8. `MUSIC_PLAYER_UPDATE`
9. `KEYMAP_PROFILE_UPDATE`（含 `is_preview` 字段，详见 §8.5）
10. `AUDIO_PAD_UPDATE`

### 8.4 设置 UI 反向同步

- LVGL 设置屏修改后调用 `ui_settings_request_apply()` / `ui_settings_request_save()`
- 通过临界区 `g_ui_settings_lock` 投递 `ui_settings_snapshot_t` 给 MainTask
- MainTask 通过 `consumeUiSettingsRequest()` → `applyUiSettingsSnapshot()` 写回 `DeviceSettings`

### 8.5 键映射二级页三段式交互

KEYMAPPED_SECONDARY 屏的 Profile 切换改为预览→应用：

1. **旋钮旋转** → 仅"预览"目标 profile（刷新键位标签 / 名称 / 图标 / 序号），不应用不落盘
2. **旋钮单击（ENTER）** → 应用当前预览的 profile，等待 `apply_pending` 流程
3. **已应用标记**：显示的 profile == 已应用 profile 时，序号 "N/8" 格子红框(0xD33A31)加粗 + 序号/名称文字红色

状态机：

- MainTask 侧 `g_preview_profile_index`（预览目标）
- UI 侧 `s_applied_profile_index`（已应用标记）
- `KeymapProfileInfo.profile_index` 描述本消息对应的 profile；`is_preview` 区分预览/已应用

---

## 9. RGB 灯光 [RGBLightControl]

- 硬件：WS2812B × 11 颗（GRB 顺序），数据 IO15，电源使能 IO21（**低电平有效** P-MOS）
- 亮度：`SetBrightness(0~100)`
- 模式（`RGB_MODE` 枚举）：
  - `RGB_NONE_MODE` 关灯
  - `RGB_SINGLE_MODE` 单色（24 色调色板索引）
  - `RGB_RAINBOW_MODE` / `RGB_RAINBOWWARE_MODE` 彩虹
  - `RGB_COLORCYCLE_MODE` 颜色循环
  - `RGB_METER_MODE`（保留，MainTask 未调度）
  - `RGB_FIRE_MODE` 火焰
  - `RGB_PULSE_MODE` 呼吸
- 按键点击高亮（`RGB_CLICK_MODE`）：
  - `CLICK_NONE_COLOR_MODE` 不响应
  - `CLICK_SINGLE_COLOR_MODE` 按下点亮对应键、抬起熄灭
  - `CLICK_WARE_COLOR_MODE`（保留）
- 显示任务负责驱动 LED 动画循环（`run()` 内 tick）

---

## 10. 音频

### 10.1 喇叭 [Speaker]

- I2S 引脚：BCLK=IO10, LRCLK=IO9, DIN=IO11（MAX98357）
- 音量：`SetVolume(0~21)` = `device_volume / 5`
- 播放能力：`PlayRemoteAudio(url)` / `PlayLocalAudio(path)` / Pause / Resume / Stop / 进度查询
- 数据源：本地 WAV / MP3（`data/` 根目录，白名单 `^[a-z0-9_]{1,20}\.(mp3|wav)$`）；启动时按 `data/config.ini` 预置播放
- 录音互斥：录音期间拒绝播放

### 10.2 麦克风 [Mic]

- I2S 引脚：BCLK=IO13, WS=IO12, SD=IO14（ICS43434）
- 采样率 16kHz，缓冲 512 samples
- 由 [VoiceRecognizer](#11-语音识别) 与 [AudioAnalyzer](#103-频谱分析) 共享

### 10.3 频谱分析 [AudioAnalyzer]

- FFT_SIZE=512，BANDS=16，使用 `arduinoFFT` 2.0.x
- DC 去除 + 汉宁窗 → 16 个频段
- `DisplayTask::updateSpectrum()` 在 `UI_SCREEN_AUDIO(_SECONDARY)` 不可见时不消耗 CPU
- 进入音乐屏接管 Mic；离开释放 Mic 并恢复语音识别

### 10.4 音效板 [AudioPad]

- 11 个矩阵键各自绑定一个本地音频文件，键按下播放
- 绑定表持久化到 `/audio_pad.ini`（SimpleIni，[pads] 节），文件不存在用空表
- 协议命令：`CMD_AUDIO_PAD` (0x17) `op=get/set/play/stop`；`CMD_AUDIO_FILE` (0x16) `op=list/begin/data/end/abort/delete`
- 文件传输：1024 B / 块（base64 内嵌 JSON），单文件 ≤2 MB，free ≥ size + 64 KB headroom
- 线程模型：MainTask 上下文执行 `trigger/stop/service`，DisplayTask 仅置请求标志（避免跨核破坏 Audio 实例）
- 导航环：主屏 → 键映射 → 音乐 → **音效** → PC状态 → HA → 设置 → 主屏
- 录音互斥由 `Speaker` 内部守卫兜底

---

## 11. 语音识别 [VoiceRecognizer]

### 11.1 引擎与协议

- 后端：**腾讯云一句话识别** (`SentenceRecognition`)
- 协议：`POST https://asr.tencentcloudapi.com`，JSON 体 + `Authorization: TC3-HMAC-SHA256 ...`
- 凭证：`voice_tencent_secret_id` + `voice_tencent_secret_key`（无 OAuth token；每次请求重新签名）
- 引擎模型：固定 `16k_zh`（中文普通话）
- 音频：`SourceType=1` + base64 编码 PCM（16k / 16bit / mono，固件录音缓冲为无头 raw PCM，`VoiceFormat = "pcm"`）
- 签名器：[TencentAsrSigner](src/voice/TencentAsrSigner.h)，基于 mbedtls HMAC-SHA256

### 11.2 触发流程

1. 应用键（默认 11）按下 → `voiceTriggerBit_` 命中 → `startVoiceCapture()`
2. `Mic.Begin()` → `voiceRecognizer_.startCapture()` → 推 `ASR_RECORDING_STATE=true`
3. 主循环 `voiceRecognizer_.feedCapture()` 持续喂 PCM
4. 触发键松开 → `finishVoiceCapture()` → 调 ASR
5. 结果：
   - 优先通过 `CMD_VOICE_TEXT` (0x0c) 推给桌面 App（支持 UTF-8 中文）
   - 若上位机未连接且文本是 ASCII，`sendAsciiTextToHost()` 兜底 HID 注入（按字符 delay 8ms）
6. 可选 `voice_auto_enter=1` 时追加回车

### 11.3 限制

- 仅在 `WIRED_KEYBOARD_MODE` + WiFi 已连接 + 语音启用时工作
- 音乐屏 (`UI_SCREEN_MUSIC*`) 进入会 `suspend()` ASR，离开自动 `resume()`
- 录音时长限制 1000~60000 ms

---

## 12. PC 状态（来自桌面 App 上报）

`CMD_PC_STATUS` (0x0d) 桌面 App 主动推送，解析后填充 [PcStatusInfo](src/message_types.h)：

- 网络：`network_connected / network_up_kbps / network_down_kbps`
- 性能：`cpu_usage_percent / memory_usage_percent / cpu_temp_c / disk_io_percent`
- 过滤：负值字段显示 `--`
- 不支持：`caps_lock / num_lock / scroll_lock / on_ac_power / battery_percent`（电量走 `BatteryStatus`，其它字段本协议未实现）

UI 展示在 `UI_SCREEN_PC_STATUS(_SECONDARY)`。

---

## 13. 音乐控制

### 13.1 上行（UI → 桌面 App）

- `CMD_MUSIC_CONTROL` (0x0f)：`previous` / `toggle` / `next`
- 触发源：二级音乐屏按钮、旋钮（在音乐二级屏映射为上 / 切 / 下）、应用键映射的 `MEDIA_PLAY` / `MEDIA_NEXT_TRACK` 等

### 13.2 下行（桌面 App → 主控）

- `CMD_MUSIC_STATUS` (0x0e)：更新 `MusicPlayerInfo`
  - 标题、艺人、播放器名、当前/下一句歌词
  - 播放状态、进度、上下首按钮可用性
- 主控本地进度：每 1000ms 自增 `current_seconds`（用于 App 离线时的本地回放）
- 超时处理：30s 未收到 MUSIC_STATUS 自动标记 `PLAYER OFFLINE`

### 13.3 ASR 与音乐的互斥

- `updateMusicUiAsrOwnership()` 自动管理录音状态机

---

## 14. HA 状态聚合屏

`SendHaStatusSnapshot()` 每 2.5s 推送一次 `HaStatusInfo`（当前仅本机 UI 消费，未发协议帧）：

- WiFi（启用 / 已连 / RSSI / IP）
- 与桌面 App 的 TCP 连接状态 + endpoint
- 当前工作模式 / 语音启用与录音状态
- ModA / ModB 在线指示
- 显示在 `UI_SCREEN_HA(_SECONDARY)`

---

## 15. 日志系统 [LogManager]

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

## 17. 已完成项 / 后续可补

| 项                                          | 状态            | 位置                                            |
| ------------------------------------------- | --------------- | ----------------------------------------------- |
| 2.4G 无线键盘模式                            | 按用户决定暂不实现（选择该模式回退 USB） | `KeyboardFactory::create` |
| 麦克风频谱显示                              | 已实现（音乐屏可见时调度） | `DisplayTask::updateSpectrum` 与 `AudioAnalyzer` |
| OTA 升级                                    | 已实现（HTTP + MD5 校验） | `src/upgrade/Upgrade.cpp`（0x0b 携带 url/checksum 触发） |
| 复位进 USB 下载模式                         | 已实现          | `CMD_FIRMWARE_DOWNLOAD` (0x14) → `cmd_firmware.cpp` |
| Profile 名称（UTF-8 中文）                  | 已实现          | `CMD_PROFILE_NAME_SET` (0x15) → `cmd_profile.cpp` |
| 音效板（11 键本地音频）                     | 已实现          | `src/audio/AudioPad.cpp` + `cmd_audio.cpp`      |
| 腾讯云一句话识别迁移                        | 已完成（替代百度 ASR） | `src/voice/TencentAsrSigner.cpp`                |
| 系统时间注入                                | 已实现          | `CMD_TIME_SET` (0x13) → `cmd_time.cpp`          |
| WiFi STA 自动重连精细策略                   | 已实现（15s 宽限后强制重启；BLE 模式整体短路） | `processWiFiReconnect` |
| 键映射二级页预览/应用分离                   | 已实现          | `KeymapProfileInfo.is_preview` + `MainTask` 状态机 |
| `CMD_KEY_EVENT` (0x09) 上报                 | 当前未实现协议序列化 | 仅本机 HID/UI 路径                            |
| `CMD_HA_STATUS` (0x12) 上报                 | 当前未实现协议序列化 | 仅本机 HA 屏消费                              |
| UI 音乐控制到 `CMD_MUSIC_CONTROL` 发送链路  | 当前未接通      | `SerialProtocol::sendMusicControl()` 已实现，无 UI 调用方 |

---

## 18. 关键路径参考

- **启动流程**：[main.cpp](src/main.cpp) → SPIFFS 挂载 / 配置加载 → 启动 [MainTask](src/tasks/MainTask.cpp) 与 [DisplayTask](src/tasks/DisplayTask.cpp)
- **按键 → 桌面**：[MatrixScanner::scan](src/input/MatrixScanner.cpp) → MainTask 边沿识别 → HID 注入
- **设置同步链路**：
  - App → [SerialProtocol](src/protocol/SerialProtocol.cpp) → [CommandRegistry](src/protocol/CommandRegistry.cpp) → [cmd_config](src/protocol/commands/cmd_config.cpp) → MainTask → `parseConfigSetCommand` → `SendDisplaySetting` 队列 → DisplayTask → LVGL
  - 反向：LVGL → `ui_settings_request_*` → MainTask → `applyUiSettingsSnapshot`
- **音效板播放链路**：矩阵键 1~11 按下 → DisplayTask ACTION_INPUT 矩阵键分支 → `AudioPad::requestTrigger()` 置请求 → MainTask tick `service()` → `Speaker::PlayLocalAudio()` → 推 `AUDIO_PAD_UPDATE` 消息 → `ui_AudioScreen_set_playing()`

---

文档基于当前仓库代码静态梳理，逻辑与接口如后续重构，请以代码为准。
