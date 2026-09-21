# EKeys 项目功能文档

> 适用代码版本：ESP32-S3-WROOM-1（N16R8，16MB Flash / 8MB OPI PSRAM）
> 框架：Arduino + PlatformIO 6.x
> 本文档基于当前代码静态梳理，完整描述项目的所有功能，逻辑与接口如后续重构，请以代码为准。

---

## 1. 项目概览

### 1.1 项目定位

EKeys 是一款面向 **桌面 / 平板场景** 的多功能宏键盘主控设备，集成 **11 键物理按键 + 1 个 EC11 旋钮 + 1 块 SPI LCD + 11 颗 RGB 灯**，可作为普通 HID 键盘使用，也可通过 Wi-Fi / USB 与桌面 App 联动，在 8 个 Profile 中一键切换键映射、灯光、主题与 PC 状态显示。

### 1.2 硬件平台

- **MCU**：ESP32-S3-WROOM-1 模组（N16R8 = 16MB Flash / 8MB OPI PSRAM）
- **USB**：TinyUSB CDC + HID（USB 键盘 + 消费控制 + 串口）
- **无线**：Wi-Fi STA、蓝牙 BLE（`t-vk/ESP32 BLE Keyboard`）
- **存储**：SPIFFS 文件系统 + NVS
- **显示**：Arduino_GFX（NV3007，428×142 横条屏）+ LVGL 8.3.11（SquareLine Studio 生成的 11~13 屏 UI）
- **音频**：ESP32-audioI2S（MAX98357A 喇叭，ICS43434 麦克风）
- **电源**：锂电池供电（1S，电池电压通过 ADC 采样），5V 升压可控

### 1.3 软件架构

- **双 FreeRTOS 任务**：
  - `MainTask`（Core 1，栈 12288）：按键扫描、EC11 旋钮、WiFi/BLE、协议收发、语音触发、键映射解析
  - `DisplayTask`（Core 0，栈 8192）：LVGL tick、RGB LED 动画、频谱渲染、消息队列消费
- **任务间通信**：FreeRTOS `xQueue`（`DisplayMessage` 队列，长度 10，约 1.2KB/消息）
- **协议命令分发**：`CommandRegistry` 单例 + `protocol::commands` 模块化注册
- **持久化**：`Configuration` + `KeymapRepository` + `ConfigStore`，基于 `SimpleIni` 读写 SPIFFS
- **同步策略**：除已声明接口外，禁止跨任务直接调用对方模块方法

### 1.4 文件目录结构

```
src/
├── main.cpp                       # 启动入口：SPIFFS → Backlight → DisplayDriver → LVGL → AppContext
├── app/                          # 全局单例（AppContext）
├── audio/                        # 音频子系统（Speaker / Mic / AudioAnalyzer / AudioPad）
├── config/                       # 配置层（Configuration / DeviceSettings / parseConfigSetCommand）
├── display/                      # 屏幕驱动（DisplayDriver / LvglPort / Backlight）
├── hardware/                     # 硬件抽象（PinMap / BatteryMonitor）
├── input/                        # 输入层（MatrixScanner / RotaryEncoder）
├── keymap/                       # 键映射（KeyMapping / KeyNameTable / KeyResolver / KeyEventDispatcher）
├── logging/                      # 日志（LogManager）
├── network/                      # 网络（WiFiManager / NtpSync / TcpChannel / DiscoveryService / NetDiagnostics）
├── output/                       # 键盘输出后端（IKeyboard / USBKeyboardImpl / BLEKeyboardImpl / KeyboardFactory）
├── protocol/                     # 私有协议（SerialProtocol / CommandRegistry / 各 cmd_*.cpp）
├── rgb/                          # RGB 灯效（RGBLightControl / RGBDriver / ClickHighlight）
├── services/                     # 服务层（ConfigStore / KeymapRepository）
├── tasks/                        # 任务层（MainTask / DisplayTask）
├── ui/                           # LVGL UI（SquareLine 生成 + 自定义 helpers / StatusBar）
├── upgrade/                      # OTA 升级（Upgrade）
├── utils/                        # 公共类型（event_types / keymap_types / ProfileIconImage）
└── voice/                        # 语音识别（VoiceRecognizer / TencentAsrSigner）
```

---

## 2. 输入层

### 2.1 矩阵扫描（MatrixScanner）

- **物理结构**：3 行 × 4 列 = 12 位置，第一行仅 3 键（COL3 缺），实际 **11 个物理按键**（应用键 ID 1~11）
- **引脚**：行 `{46, 39, 38}`、列 `{16, 17, 18, 8}`（PINOUT §2.5）
- **状态机**：每键独立消抖 `IDLE → DEBOUNCE_PRESS → PRESSED → DEBOUNCE_RELEASE`
- **消抖时间**：`kDebounceTimeMs = 10`
- **扫描周期**：MainTask 5ms tick
- **调试**：支持慢速扫描探针（300ms/格）用于硬件走线诊断
- **API**：`scan()` / `getStableState()` / `getPressedKeys()` / `getReleasedKeys()` / `keyIdToRowCol()`

### 2.2 旋钮（板载 EC11，RotaryEncoder）

- **引脚**：`SW=5, A=6, B=7`（PINOUT §2.3）
- **实现**：ESP32Encoder PCNT + OneButton
- **事件**：
  - 顺时针 → `LV_KEY_RIGHT`（19）
  - 逆时针 → `LV_KEY_LEFT`（20）
  - 单击 → `LV_KEY_ENTER`（10）
  - 双击 → `LV_KEY_ESC`（27）
- **用途**：**仅用于本机屏幕导航**，**不进入 HID 键映射**
- **场景映射**：
  - 音乐二级屏 → 上一首/下一首/播放暂停
  - 键映射二级屏 → 切换 Profile
  - 设置二级屏 → 上/下导航/数值增减
  - 其它屏 → 标准 `LV_KEY_LEFT/RIGHT/ENTER/ESC`

### 2.3 外挂模块接口（I2C，PINOUT §2.8）

- **引脚**：SDA=IO48, SCL=IO47, INT=IO45
- **频率**：100kHz
- **预留地址**：ModA=`0x06`（旋钮+滑动电位器）、ModB=`0x08`（机械旋钮）
- **状态**：当前固件未注册 I2C 主控，模块接口预留

### 2.4 输入键值编码（matrix → UI）

矩阵键 ID（1~11）以 `kMatrixKeyActionBase(100) + key_id`（101~111）编码进 `ActionInput.action`，**与 `LV_KEY_*` 数值完全不重叠**，避免裸传 key_id 时与 `LV_KEY_ENTER=10` 冲突。

| 语义     | LVGL 编码                         | 旋钮   | 矩阵键       | 触屏兜底按钮   |
| -------- | --------------------------------- | ------ | ------------ | -------------- |
| LEFT     | `LV_KEY_LEFT` (20)                | 逆时针 | —            | `ButtonLeft*`  |
| RIGHT    | `LV_KEY_RIGHT` (19)               | 顺时针 | —            | `ButtonRight*` |
| UP       | `LV_KEY_UP` (17)                  | —      | key_id=7     | —              |
| DOWN     | `LV_KEY_DOWN` (18)                | —      | key_id=11    | —              |
| ENTER    | `LV_KEY_ENTER` (10)               | 单击   | —            | `ButtonEnter*` |
| ESC      | `LV_KEY_ESC` (27)                 | 双击   | —            | `ButtonExit*`  |
| 焦点跳转 | `100 + key_id` (101~111)          | —      | 编码透传     | —              |

---

## 3. 键映射与配置

### 3.1 键位模型（KeyMapping）

每个物理键（应用键 ID 1~11）支持三种内容（**互斥生效**）：

- **`function_key`**：单个功能字符串（如 `KEY_FUNCTION_ASR`、`MEDIA_PLAY`、`MEDIA_NEXT_TRACK`），非空时优先使用
- **`normal_key[]`**：普通键序列，最多 6 个，支持 `+` 分隔同时按下（如 `Ctrl+c`）
- **`macros_key[]`**：宏键序列，最多 5 个，按顺序先压后弹（当前 KeyResolver 尚未实现宏播放，仅存储/协议透传）
- **`text_key`**：纯文本输入通道
- **`combo1_*` / `combo2_*`**：FUN 组合层通道（FUN1 优先于 FUN2），`fun_key1` / `fun_key2` 按住时触发

**支持的键名**：
- 字母 a\~z 与 A\~Z（ASCII 自动处理 Shift）
- 数字 0\~9（符号名 `NUM_0` 等）
- 符号：`Space , . ; ' [ ] \ / - = ` 等
- 控制键：`Enter / Backspace / Tab / Esc / Space`
- F1\~F12、方向键、编辑键、锁定键、PrintScreen / Pause / Menu
- 修饰键：`Ctrl/Shift/Alt/Win`（左 + 右）
- 解析支持 `0xNN` 十六进制与十进制字符串

### 3.2 配置持久化（Configuration + ConfigStore + KeymapRepository）

- **文件位置**：SPIFFS
  - `/config.ini`：全局配置（设备 SID，PROFILE/RGB/屏幕/音频/语音等）
  - `/keymap1.ini ~ keymap8.ini`：每个 Profile 的键映射
  - `/profile_icons/profile{N}.png`：每个 Profile 的自定义 48×48 PNG 图标
  - `/audio_pad.ini`：音效板绑定表（AudioPad）
- **解析库**：SimpleIni 4.19
- **互斥量**：`Configuration::mutex_`（FreeRTOS semaphore），所有公开接口内部加锁
- **批量持久化**：单次读/写覆盖多个键（避免 SPIFFS 原子写开销被 N 次放大）
- **API**：`load()` / `saveSetting()` / `saveSettings()` / `loadActiveProfileKeyMapping()` / `switchActiveProfile()` / `mutateSettings()`

### 3.3 Profile 切换

- **Profile 数量**：`CONFIG_PROFILE_COUNT = 8`
- **文件路径规则**：`Configuration::getProfileConfigPath(idx)` / `getProfileIconPath(idx)`
- **元数据**：
  - **显示名**：UTF-8（支持中文），通过 `CMD_PROFILE_NAME_SET` (0x15) 由 App 设置，持久化到 `config.ini [profile]` 节
  - **图标**：48×48 PNG，通过 `CMD_PROFILE_ICON_SET` (0x11) 上传，lodepng 解码为 RGBA
- **激活切换**：`CMD_CONFIG_SET active_keymap_profile` → `Configuration::switchActiveProfile()` → `MainTask::reloadKeymap()` → 刷新 KeyResolver 与 UI

### 3.4 HID 触发流程（KeyResolver）

1. `currentKeyboard_->releaseAll()` 先释放全部键（防止叠加）
2. 遍历 11 个应用键的边沿事件（pressed/released）
3. 当前激活的 FUN 层（0=单击 / 1=FUN1 / 2=FUN2）决定查表优先级
4. 按通道优先级注入 HID：`function_key` > `text_key` > `normal_key[]`（修饰键位自动处理）
5. 物理按键边沿会向桌面 App 上报 `CMD_KEY_EVENT`（用于屏幕点击高亮同步）
6. 按键边沿同时触发 `KeyEventDispatcher::onKeyEdge()`：
   - 命中语音触发键（`voice_trigger_key` 或 `function_key=KEY_FUNCTION_ASR`）→ VoiceRecognizer 启停录音
   - 其余 → ClickHighlight 写入 RGB 高亮 override

---

## 4. 键盘输出（多模式）

### 4.1 三种工作模式（DeviceSettings.work_mode）

| 模式                              | 实现               | 备注                                    |
| --------------------------------- | ------------------ | --------------------------------------- |
| `WIRED_KEYBOARD_MODE` (0)         | USBKeyboardImpl    | TinyUSB HID Keyboard + Consumer Control |
| `BLUETOOTH_KEYBOARD_MODE` (1)     | BLEKeyboardImpl    | 启用 BLE 后释放经典蓝牙内存              |
| `WIRELESS_2_4G_KEYBOARD_MODE` (2) | **未实现**         | 按用户决定暂不实现，选择该模式打印 warning 并安全回退 USB |
| `NONE_MODE`                       | 占位               | 启动前                                  |

### 4.2 统一接口（IKeyboard）

```cpp
virtual bool begin() = 0;
virtual void press(uint8_t keycode, uint8_t modifier = 0) = 0;
virtual void release(uint8_t keycode) = 0;
virtual void type(const String &text) = 0;
virtual void releaseAll() = 0;
virtual bool isConnected() const = 0;
virtual void send() = 0;
```

### 4.3 模式切换副作用

- **进入 BLE 模式**：`esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)` 释放经典蓝牙内存
- **进入 BLE 模式**：自动关闭 WiFi 重连（BLE 模式下内存紧张）
- **进入 BLE 模式**：TCP 通道停用，仅 Serial 协议通道工作
- **键盘实例重建**：`AppContext::applyWorkMode()` 调 `KeyboardFactory::recreate()`

---

## 5. 私有协议（与桌面 App 通信）

### 5.1 传输层（SerialProtocol）

- **双通道**：USB CDC Serial + TCP（自动发现）
- **USB CDC**：115200 波特率（UART0 已不再承担日志，仅作烧录用）
- **帧格式**：JSON 行协议
  - 请求：`{"cmd":0x07,"seq":1,"data":{...}}`
  - 响应：`cmd | 0x80`（如 `0x87`）
- **心跳**：`CMD_HEARTBEAT` (0x0a) 自处理，直接回 `0x8a`
- **通道分流**：TCP 在线时帧只走 TCP（防心跳刷屏）；TCP 离线时帧走 Serial

### 5.2 上位机自动发现（UDP，DiscoveryService）

- **端口**：UDP 30001
- **设备广播**：`FUNKEYBOARD_DISCOVER`
- **App 应答**：`FUNKEYBOARD_HERE`
- **超时**：800ms 失败则回退 IP `192.168.31.1`
- **控制通道**：TCP 30000（`TcpChannel::connectTo`）

### 5.3 命令清单（23 条，CommandType）

| ID     | 命令                     | 来源     | 实现文件                          |
| ------ | ------------------------ | -------- | --------------------------------- |
| 0x01   | `CMD_CONF_VERSION_GET`   | App→主控 | `cmd_firmware.cpp`                |
| 0x02   | `CMD_CONF_VERSION_SET`   | App→主控 | `cmd_firmware.cpp`                |
| 0x03   | `CMD_DEVICE_INFO_GET`    | App→主控 | `cmd_device_info.cpp`             |
| 0x04   | `CMD_DEVICE_INFO_SET`    | App→主控 | `cmd_device_info.cpp`             |
| 0x05   | `CMD_KEYMAP_GET`         | App→主控 | `cmd_keymap.cpp`                  |
| 0x06   | `CMD_KEYMAP_SET`         | App→主控 | `cmd_keymap.cpp`（写入+直刷运行时） |
| 0x07   | `CMD_CONFIG_GET`         | App→主控 | `cmd_config.cpp`                  |
| 0x08   | `CMD_CONFIG_SET`         | App→主控 | `cmd_config.cpp`                  |
| 0x09   | `CMD_KEY_EVENT`          | 主控→App | 物理按键边沿上报                   |
| 0x0a   | `CMD_HEARTBEAT`          | 双向     | SerialProtocol 自处理             |
| 0x0b   | `CMD_FIRMWARE_INFO`      | App→主控 | 触发 OTA 升级（含 url+checksum） |
| 0x0c   | `CMD_VOICE_TEXT`         | 主控→App | ASR 文本推送                      |
| 0x0d   | `CMD_PC_STATUS`          | App→主控 | 推送 PC 状态（CapsLock/网络/CPU） |
| 0x0e   | `CMD_MUSIC_STATUS`       | App→主控 | 推送音乐播放器状态                 |
| 0x0f   | `CMD_MUSIC_CONTROL`      | 主控→App | UI 控制按钮触发                    |
| 0x10   | `CMD_PROFILE_STATE`      | 双向     | 8 个 profile 名称+图标元数据      |
| 0x11   | `CMD_PROFILE_ICON_SET`   | App→主控 | PNG base64 上传/删除               |
| 0x12   | `CMD_HA_STATUS`          | 主控→App | 周期推送状态聚合                   |
| 0x13   | `CMD_TIME_SET`           | App→主控 | 写入系统时间（epoch + tz）        |
| 0x14   | `CMD_FIRMWARE_DOWNLOAD`  | App→主控 | 复位进 USB 下载模式                |
| 0x15   | `CMD_PROFILE_NAME_SET`   | App→主控 | 设置 profile 名称（UTF-8 中文）   |
| 0x16   | `CMD_AUDIO_FILE`         | App→主控 | 音效文件管理（op=list/begin/data/end/abort/delete）|
| 0x17   | `CMD_AUDIO_PAD`          | App→主控 | 音效板绑定与播放（op=get/set/play/stop）|

### 5.4 注册机制

`registerAllCommandHandlers()` 在 `MainTask::begin()` 之后统一注册：
- `cmd_config` / `cmd_keymap` / `cmd_profile` / `cmd_pc_status` / `cmd_music`
- `cmd_device_info` / `cmd_firmware` / `cmd_time` / `cmd_audio`
- 注册表采用 `std::array<Entry, 64>` + 临界区保护，零堆分配

---

## 6. 桌面 App / 主控设置（CMD_CONFIG_SET）

`parseConfigSetCommand` 支持原子写入以下字段（FEATURE_DOC §6）：

| 类别       | 字段                                                       |
| ---------- | ---------------------------------------------------------- |
| WiFi       | `wifi_switch`、`connect_host`、`wifi_ssid`、`wifi_password` |
| 工作模式   | `work_mode`（触发 `setWorkMode()` 重建键盘实例）           |
| RGB LED    | `rgb_mode`、`rgb_single_color`、`rgb_click_mode`、`rgb_brightness` |
| 屏幕       | `tft_theme`、`tft_brightness`（下限 5，避免 OLED 烧屏）   |
| 音频       | `device_volume`（联动 `Speaker::SetVolume(volume/5)`）、`audio_enable`、`power_mode` |
| 语音       | `voice_enable`、`voice_trigger_key`、`voice_max_record_ms`、`voice_auto_enter`、`voice_cuid`、`voice_tencent_secret_id`、`voice_tencent_secret_key` |
| PC 状态    | `pc_status_mask`                                            |
| Profile    | `active_keymap_profile`（写入后刷新 UI + 上报完整状态）     |
| FUN 组合键 | `fun_key1`、`fun_key2`（FUN 按住时其它键触发组合层）        |
| 设备元数据 | `device_name`、`serial_number`（系统节持久化）              |
| UI 语言    | `ui_lang`（0=中文/1=英文，主页日期/星期显示）              |

**设置变更后**：自动向 `DisplayTask` 推送 `SETTING_UPDATE`，自动重算语音运行时，自动 diff 副作用（`AppContext::applyUiSideEffects()`）。

---

## 7. 网络层

### 7.1 WiFi 管理（WiFiManager）

- **默认状态**：`wifi_switch = false`（关闭）
- **连接策略**：
  - 连接中每 5s 重试一次（`kWifiRetryIntervalMs`）
  - 单次连接超时 10s（`kWifiConnectTimeoutMs`）
  - 已连接断链后给 **15s 自恢复宽限**（STA 自动重连），仍未恢复则 **强制重启 WiFi**（mode off → 重新 begin）→ 回到 WaitingRetry
- **BLE 模式短路**：`process()` 整体短路（不启动任何 WiFi 活动）
- **连上后回调**：触发 NTP 同步 + UDP 自动发现（`on_connected_` 回调）

### 7.2 NTP 同步（NtpSync）

- **服务器**：`pool.ntp.org`
- **时区**：GMT+8（CST-8）
- **优先级**：USB 注入时间（`CMD_TIME_SET` 0x13）> NTP
- **同步后**：`settimeofday()` 写入系统时钟；UI 时间显示

### 7.3 TCP 客户端（TcpChannel）

- **自动发现**：UDP 30001（`DiscoveryService`）
- **控制通道**：TCP 30000
- **复用 SerialProtocol**：收到的行喂给 `handleTcpLine()`，发送经 `SerialProtocol::setLineSink()` 钩子
- **断线重连**：15s 未恢复则强制重启 WiFi
- **BLE 模式停用**

### 7.4 主动推送（CMD_HA_STATUS，0x12）

`SendHaStatusSnapshot()` 每 2.5s 推送一次 `HaStatusInfo`：
- WiFi（启用/已连/RSSI/IP）
- 与桌面 App 的 TCP 连接状态 + endpoint
- 当前工作模式 / 语音启用与录音状态
- ModA / ModB 在线指示（预留）
- 键盘主机连接（BLE/USB HID）

---

## 8. 显示与 UI

### 8.1 LVGL 屏（SquareLine 生成）

定义在 `ui.h` 中，共 **13 个屏幕**（含 2 个音频屏）：

| Screen Tag                                              | 中文用途                                    |
| ------------------------------------------------------- | ------------------------------------------- |
| `UI_SCREEN_MAIN`                                        | 主屏（时间 / 状态条 / 工作模式）            |
| `UI_SCREEN_KEYMAPPED`                                   | 键映射概览（11 应用键 + Profile 图标）      |
| `UI_SCREEN_KEYMAPPED_SECONDARY`                         | 键映射详情（每个键的文字标签）              |
| `UI_SCREEN_MUSIC` / `UI_SCREEN_MUSIC_SECONDARY`         | 音乐控制（标题/艺人/歌词/进度条）           |
| `UI_SCREEN_AUDIO` / `UI_SCREEN_AUDIO_SECONDARY`         | 音效板（11 键绑定的本地音频 + 播放控制）    |
| `UI_SCREEN_PC_STATUS` / `UI_SCREEN_PC_STATUS_SECONDARY` | PC 状态（CapsLock/网络/CPU/内存/温度）       |
| `UI_SCREEN_HA` / `UI_SCREEN_HA_SECONDARY`               | HA 状态聚合（WiFi/TCP/模块/语音）           |
| `UI_SCREEN_SETTING` / `UI_SCREEN_SETTING_SECONDARY`     | 设置（work mode / RGB / 屏幕 / 音量 / 电源 / Profile / WiFi） |

**字体**：内置 BebasNeue（14/16/24/28/36/48/86）与自定义中文 FontCKJGT（16/24/28，部分尺寸被 build filter 排除）

### 8.2 状态条（ui_StatusBar）

- **显示内容**：
  - 工作模式（WIR / BLT / 2.4）
  - 音量条（`device_volume`）
  - WiFi / TCP 状态（图标+色）
  - ModA / ModB 在线指示（预留）
  - 录音中动画（ASR 录制）
  - 电池电量（`battery_percent`，5s 节流）

### 8.3 主任务 → 显示任务消息（DisplayMessage，13 种类型）

| 类型                       | 载荷                          | 触发源                                       |
| -------------------------- | ----------------------------- | -------------------------------------------- |
| `SettingUpdate`            | `ui_settings_snapshot_t`      | `CMD_CONFIG_SET` / UI 反向同步               |
| `TimeUpdate`               | `time_text / date_text / week_text` | NTP 同步 + 1s tick                    |
| `ActionInput`              | `action`（LV_KEY_* / 矩阵编码） | EC11 旋钮 / 矩阵键                          |
| `KeyInput`                 | `key_value` 位掩码            | RGB 点击高亮                                 |
| `ModuleStatus`             | `ModuleStatusInfo`            | I2C 模块在线状态                             |
| `AsrRecording`             | `asr_recording` bool          | 语音识别开始/结束                            |
| `PcStatus`                 | `PcStatusInfo`                | `CMD_PC_STATUS`                              |
| `HaStatus`                 | `HaStatusInfo`                | 2.5s 节流推送                                |
| `MusicPlayer`              | `MusicPlayerInfo`             | `CMD_MUSIC_STATUS` / 30s 超时                |
| `KeymapProfile`            | `KeymapProfileInfo`           | Profile 切换 / 预览 / FUN 层变化             |
| `Navigate`                 | `navigate_target`             | 屏幕路由                                     |
| `BatteryStatus`            | `battery_percent`             | 5s 节流                                      |
| `AudioPad`                 | `AudioPadInfo`                | 音效板绑定/播放状态                          |

### 8.4 设置 UI 反向同步（ui_settings_request_*）

- **LVGL → MainTask**：控件修改后调用 `ui_settings_request_apply()` / `_save()`
- **临界区**：`g_ui_settings_lock` spinlock 保护 `ui_settings_snapshot_t`
- **MainTask 消费**：`consumeUiSettingsRequest()` → `applyUiSettingsSnapshot()`
- **写回**：`Configuration::mutateSettings()` 原子写入 → `persist=true` 时逐键 `saveSetting()` 持久化
- **副作用**：work_mode 走 `AppContext::applyWorkMode()`，Profile 走 `reloadKeymap()`，随后投递全量 `SETTING_UPDATE`

### 8.5 屏幕路由（navigateTo / Navigate 消息）

- `DisplayTask::navigateTo(ui_screen_tag_t tag)` → 转 `DisplayMessageType::Navigate`
- `navigateNow(tag)` 调 `lv_scr_load_anim()` 直切（无动画或带主题动画）
- **5s 无操作自动回主页**：仅一级页（非 MAIN / 非 SECONDARY）启用，二级页不启用（详情页需较长时间查看）

### 8.6 UI 主题系统（主题切换）

- **主题套数**：表驱动 `kThemes[]` 数组，不预设数量
- **共享样式**：后序遍历挂载/卸载**共享 `lv_style_t`**（非逐对象 local style 覆写）
- **切屏动画**：双收口（屏内事件切屏改 `ui_helpers.c::_ui_screen_change` + DisplayTask 发起切屏在 `navigateNow` 覆写 anim 入参）
- **重渲范围**：13 屏全部参与覆写；懒应用（切主题只刷当前屏，其余屏进入时在 `navigateNow` 补刷）
- **紧凑单页主题**：主页替换为手写 `ui_CompactMain`（左时间 / 中间当前页名 / 右状态灯）
- **运行时状态色**：WiFi/电量/READY/RECORDING 等不走静态遍历，统一改引用主题取色 API

---

## 9. RGB 灯光（RGBLightControl + RGBDriver + ClickHighlight）

### 9.1 硬件

- **WS2812B × 11 颗**（GRB 顺序）
- **数据 IO**：IO15（kPinRgbDin）
- **电源使能**：IO21（kPinRgbPowerCtrl，**低电平有效**，MOSFET 高边开关）
- **11 颗灯珠**与 **11 个应用键 ID 一一对应**

### 9.2 模式（RGB_MODE，9 种）

| 模式               | 枚举值       | 说明                               |
| ------------------ | ------------ | ---------------------------------- |
| `RGB_NONE_MODE`    | 0            | 关灯                                |
| `RGB_SINGLE_MODE`  | 1            | 单色（24 色调色板索引）            |
| `RGB_RAINBOW_MODE` | 2            | 彩虹                                |
| `RGB_RAINBOWWARE_MODE` | 3         | 彩虹波                              |
| `RGB_COLORCYCLE_MODE` | 4          | 颜色循环                            |
| `RGB_METER_MODE`   | 5            | 频谱柱（实现保留）                  |
| `RGB_FIRE_MODE`    | 6            | 火焰                                |
| `RGB_PULSE_MODE`   | 7            | 呼吸                                |
| `RGB_SOUND_MODE`   | 8            | 拾音波形                            |
| `RGB_MATRIX_MODE`  | 9            | 拾音矩阵律动                        |

### 9.3 点击高亮（RGB_CLICK_MODE，ClickHighlight）

| 模式                      | 枚举值 | 行为                                    |
| ------------------------- | ------ | --------------------------------------- |
| `CLICK_NONE_COLOR_MODE`   | 0      | 不响应                                   |
| `CLICK_SINGLE_COLOR_MODE` | 1      | 按下点亮对应键 LED、抬起熄灭             |
| `CLICK_WARE_COLOR_MODE`   | 2      | 保留（同 SINGLE，含相邻 LED 微亮）       |

### 9.4 拾音模式（Sound/Matrix）

- **触发条件**：`RGB_SOUND_MODE` 或 `RGB_MATRIX_MODE` 启用时，`wantsMic()` 返回 true
- **数据源**：`DisplayTask` 从 `Mic → AudioAnalyzer → setAudioBands(16 bands)` 喂入
- **频谱**：FFT_SIZE=512，BANDS=16，DC 去除 + 汉宁窗
- **互斥**：与 `VoiceRecognizer` 共享 Mic，使用 `Speaker::Stop()` 兜底（I2S BCLK 共用）

### 9.5 动画驱动

- **周期**：DisplayTask::run() tick，约 30ms 一帧
- **override**：`ClickHighlight` 写入 `highlight_[i]`，渲染时优先

---

## 10. 音频子系统

### 10.1 喇叭（Speaker，MAX98357A）

- **I2S 引脚**（PINOUT §2.7，I2S_NUM_1）：BCLK=IO10, LRCLK=IO9, DIN=IO11
- **音量**：`SetVolume(0~21)` = `device_volume / 5`，线性映射
- **播放能力**：
  - `PlayRemoteAudio(url)`：远程 URL
  - `PlayLocalAudio(path)`：SPIFFS 本地文件（WAV/MP3）
  - `Pause / Resume / Stop`
  - `applyDeviceVolume(volume)`：全局音量联动
- **淡入抑制**：播放启动后 0 → target_volume 线性爬升 `kRampDurationMs`，抑制 MP3 开头电流音
- **底层库**：ESP32-audioI2S@3.0.11

### 10.2 麦克风（Mic，ICS43434）

- **I2S 引脚**（PINOUT §2.7，I2S_NUM_0）：SCK=IO13（专用）, WS=IO12, SD=IO14
- **采样率**：16kHz，缓冲 512 samples
- **Mic 与 Speaker 互斥**：`prepareI2sForMicCapture()` 停 Speaker（BCLK=IO10 共用）

### 10.3 频谱分析（AudioAnalyzer）

- **FFT_SIZE=512**，**BANDS=16**，`arduinoFFT`
- **处理流程**：DC 去除 + 汉宁窗 → 16 个频段能量（0~1）
- **调用方**：DisplayTask 进入音乐屏可见时调度，离开时释放

### 10.4 音效板（AudioPad）

- **绑定表**：11 个矩阵键各自的本地音频文件绑定（文件名 `^[a-z0-9_]{1,20}\.(mp3|wav)$`）
- **持久化**：`/audio_pad.ini`（SimpleIni `[pads]` 节）
- **播放**：复用 `Speaker::PlayLocalAudio()`（按扩展名选解码器）
- **音量**：全局复用 `device_volume`
- **协议**：`CMD_AUDIO_FILE` (0x16 文件管理) + `CMD_AUDIO_PAD` (0x17 绑定播放)
- **线程模型**：
  - MainTask 上下文：`load()` / `setBinding()` / `persist()` / `trigger()` / `stop()` / 协议 handler
  - DisplayTask（Core 0）只允许 `requestTrigger()` / `requestStop()` 置请求标志
  - `MainTask::loop()` 的 `service()` 消费执行（保证 Audio 实例启停与喂流同任务串行）
- **录音互斥**：录音期间 Speaker 拒绝播放

---

## 11. 语音识别（VoiceRecognizer）

### 11.1 引擎与协议

- **后端**：腾讯云一句话识别 REST API（TC3 签名）
- **凭证**：`voice_tencent_secret_id` + `voice_tencent_secret_key`
- **签名**：`TencentAsrSigner` 实现 TC3-HMAC-SHA256 签名
- **cuid**：`voice_cuid`（TC3 协议不使用，保留占位）

### 11.2 触发流程

1. 应用键（默认 11，配置 `voice_trigger_key`）按下 → `startCapture()`
2. `Mic::begin()` → `voiceRecognizer_.startCapture()` → 推 `ASR_RECORDING_STATE=true`
3. MainTask 周期 `feedCapture()` 持续喂 PCM（PSRAM 缓冲）
4. 触发键松开 → `finishCapture()` 截断 PCM 并投递 ASR 后台 Task
5. 后台 ASR Task 调腾讯云 REST → 结果经 `CMD_VOICE_TEXT` (0x0c) 推 App
6. 若 App 未连接且文本为 ASCII，`sendAsciiTextToHost()` 兜底 HID 注入
7. `voice_auto_enter=1` 时追加回车

### 11.3 工作条件

- 仅在 `WIRED_KEYBOARD_MODE` + WiFi 已连 + `voice_enable=1` 时工作
- 音乐屏（`UI_SCREEN_MUSIC*`）进入会 `suspend()` ASR，离开自动 `resume()`
- 录音时长限制 1000~8000ms

### 11.4 ASR 后台任务

- **HTTP 阻塞搬离 MainTask**：录音结束立即返回，后台 Task 执行识别 + 上报
- **任务队列**：单元素（避免 heap 抖动），新录音需等前一段识别完成
- **临界区保护**：`capturing_` / `asr_job_pending_` 跨任务读写加自旋锁

---

## 12. PC 状态（来自桌面 App 上报）

`CMD_PC_STATUS` (0x0d，App 主动推送) 解析后填充 `PcStatusInfo`：

| 类别   | 字段                                         |
| ------ | -------------------------------------------- |
| 网络   | `network_connected` / `network_up_kbps` / `network_down_kbps` |
| 性能   | `cpu_usage_percent` / `memory_usage_percent` / `cpu_temp_c` / `disk_io_percent` |
| 过滤   | 负值字段显示 `--`                            |
| 不支持 | `caps_lock / num_lock / scroll_lock / on_ac_power / battery_percent`（电量走 `BatteryStatus`） |

UI 展示在 `UI_SCREEN_PC_STATUS(_SECONDARY)`。

---

## 13. 音乐控制

### 13.1 上行（UI → 桌面 App，`CMD_MUSIC_CONTROL` 0x0f）

- **触发源**：
  - 二级音乐屏按钮
  - 旋钮（在音乐二级屏映射为上/切/下）
  - 应用键映射的 `MEDIA_PLAY` / `MEDIA_NEXT_TRACK` 等
- **命令**：`previous` / `toggle` / `next`

### 13.2 下行（桌面 App → 主控，`CMD_MUSIC_STATUS` 0x0e）

- **更新字段**：`title` / `artist` / `player_name` / `lyric_current` / `lyric_next`
- **状态**：`is_playing` / `is_paused` / `can_prev` / `can_next` / `current_seconds` / `total_seconds`
- **本地进度**：每 1000ms 自增 `current_seconds`（用于 App 离线时的本地回放）
- **超时处理**：30s 未收到 MUSIC_STATUS 自动标记 `PLAYER OFFLINE`

### 13.3 ASR 与音乐互斥

- `updateMusicUiAsrOwnership()` 自动管理录音状态机
- 音乐屏进入 → 暂停 ASR；离开 → 恢复 ASR

---

## 14. HA 状态聚合屏（`UI_SCREEN_HA`）

`SendHaStatusSnapshot()` 每 2.5s 推送一次 `HaStatusInfo`（CMD_HA_STATUS 0x12）：

- **WiFi**：启用 / 已连 / RSSI / IP 地址
- **TCP**：与桌面 App 连接状态 + endpoint
- **工作模式**：当前 `work_mode`
- **语音**：启用 + 录音状态
- **模块**：ModA / ModB 在线指示（预留）
- **键盘连接**：BLE/USB HID 主机连接状态

UI 展示在 `UI_SCREEN_HA(_SECONDARY)`。

---

## 15. 电池监测（BatteryMonitor）

- **硬件**：GPIO4 (kPinBatteryAdc) ADC1 通道，两个 200kΩ 电阻 1:1 分压
- **算法**：ADC 端电压 × 2 = 电池电压
- **锂电池放电曲线**：
  - 4.20V → 100%
  - 3.70V → 50%
  - 3.30V → 10%
  - 3.00V → 0%
- **API**：
  - `begin()`：配置 ADC 通道 + 衰减
  - `readPercent()`：0~100，内置 8 次滑动平均 + 5s TTL 缓存
  - `readMilliVolts()`：mV 值，同样 5s 节流
- **DisplayTask**：5s 节流推送 `BatteryStatus` 消息

---

## 16. OTA 升级（Upgrade）

- **触发**：`CMD_FIRMWARE_INFO` (0x0b) 请求携带 `data.url` + `data.checksum`（固件 MD5，32 位十六进制，**必填**）
- **流程**：
  1. App 推送 OTA 命令
  2. cmd handler 回成功响应后调 `performOta()`（MainTask 上下文阻塞）
  3. 下载流式写入 ota_1 分区，边写边计算 MD5
  4. 校验失败 `abort()` **不覆盖当前固件**
  5. 校验通过 → 自动重启进入新固件
- **复位下载模式**：`CMD_FIRMWARE_DOWNLOAD` (0x14) 主动复位进 USB 下载模式

---

## 17. 电源管理

### 17.1 5V 升压使能

- **引脚**：IO3（kPinBoost5VEnable）
- **行为**：仅在 `NORMAL_POWER_MODE` 时开启 5V；其它模式关闭以省电

### 17.2 电源模式（PowerMode）

| 模式                          | 枚举值 | 当前状态                                      |
| ----------------------------- | ------ | --------------------------------------------- |
| `NORMAL_POWER_MODE`           | 0      | 实际生效（5V 升压开启）                       |
| `LOW_POWER_MODE`              | 1      | 占位（5V 升压关闭，预留）                     |
| `LIGHT_POWER_MODE`            | 2      | 占位                                          |
| `DEEPSLEEP_POWER_MODE`        | 3      | 占位（保留 RTC IO 唤醒源）                    |

---

## 18. 日志系统（LogManager）

- **输出**：默认 115200 串口（USB CDC）
- **级别**：DEBUG / INFO / WARNING / ERROR
- **可注册回调**：用于扩展 SPIFFS / Web 等通道（占位，当前未启用）
- **全局宏**：`LOG_DEBUG(tag, fmt, ...)` / `LOG_INFO(tag, fmt, ...)` / `LOG_WARNING(tag, fmt, ...)` / `LOG_ERROR(tag, fmt, ...)`
- **与协议共用 USB CDC**：桌面 App 需跳过非 `{` 开头的行

---

## 19. 任务与同步策略

| 资源                     | 拥有方      | 消费者                 | 同步方式                            |
| ------------------------ | ----------- | ---------------------- | ----------------------------------- |
| `DisplayMessage` 队列    | MainTask    | DisplayTask            | FreeRTOS `xQueue`（长度 10）        |
| `Configuration::mutex_`  | 共享        | MainTask / ConfigStore | FreeRTOS semaphore                  |
| `ui_settings_lock`       | LVGL 端     | MainTask               | spinlock + `ui_settings_snapshot_t` |
| `keyboard_` 指针替换     | MainTask    | MainTask 内部          | 仅在 `setWorkMode` 内替换，外部只读 |
| `RGB` 状态               | DisplayTask | DisplayTask 内部       | 局部变量，外部通过队列投递事件      |
| `VoiceRecognizer` 状态   | 跨任务      | MainTask / 后台 ASR    | portMUX 自旋锁                      |
| `AudioPad` 请求标志      | 跨任务      | MainTask service       | portMUX 自旋锁                      |
| `g_active_screen_tag`    | DisplayTask 单写 | MainTask 单读     | uint8_t 原子读写                    |

> **规则**：除已声明接口外，**禁止**跨任务直接调用对方模块的方法。

---

## 20. 启动流程

```
main.cpp::setup():
  Serial.begin(115200)
  → LOG_INFO("===== EKeys boot =====")
  → Backlight::begin()                    # 背光点亮
  → DisplayDriver::instance().begin(40MHz) # NV3007 初始化
  → DisplayDriver::fillScreen(BLACK)
  → LvglPort::instance().init()           # LVGL 初始化
  → LvglPort::showSplash()                # 开机画面（黑底+EKeys）
  → ConfigStore::mount()                  # SPIFFS 挂载（失败死循环）
  → AppContext::instance().init()
       ├─ Configuration::load()           # 加载 /config.ini
       ├─ KeymapRepository 初始化
       ├─ MainTask::begin()               # 启动 MainTask（Core 1）
       │   ├─ WiFiManager::begin()
       │   ├─ NtpSync::requestSync()
       │   ├─ DiscoveryService::start()
       │   ├─ TcpChannel::start()
       │   ├─ Speaker::begin()
       │   ├─ VoiceRecognizer::init()
       │   ├─ 注册所有 cmd_* 协议 handler
       │   └─ KeyResolver::begin() + reloadKeymap()
       └─ DisplayTask::begin()            # 启动 DisplayTask（Core 0）
           ├─ 创建 DisplayMessage 队列
           ├─ ui_init()                   # 创建 11~13 屏 UI
           ├─ LvglPort::clearSplash()     # 销毁开机画面
           └─ RGBLightControl::applySettings() + RGB LED 启动

main.cpp::loop():
  → AppContext::mainTask().loop()         # MainTask 5ms tick
  → delay(1)                              # 让出调度
```

---

## 21. 关键路径参考

- **按键 → HID**：`MatrixScanner::scan()` → MainTask 边沿识别 → `KeyResolver::press/release` → `IKeyboard`
- **按键 → RGB 高亮**：`MatrixScanner::scan()` → `KeyResolver::press/release` → `KeyEventDispatcher::onKeyEdge` → `ClickHighlight::onKeyEdge` → `RGBLightControl::setHighlight`
- **按键 → 桌面 App**：`MatrixScanner::scan()` → `KeyEventDispatcher::onKeyEdge` → `SerialProtocol::sendDocument(CMD_KEY_EVENT)`
- **按键 → 语音触发**：`KeyEventDispatcher::onKeyEdge(pressed=true)` 命中 `voice_trigger_key` 或 `KEY_FUNCTION_ASR` → `VoiceRecognizer::startCapture()`
- **旋钮 → UI 导航**：`RotaryEncoder` 回调 → `MainTask::sendDisplayAction(action)` → 队列 → `DisplayTask::applyMessage(ActionInput)` → `lv_event_send(LV_EVENT_KEY)`
- **矩阵键 → UI 焦点跳转**：键映射屏 `KEYMAPPED_SECONDARY` 截胡 101~111 → 跳入并把 key_id 作为焦点键传 UI；二级页透传 101~111，UI 侧解码
- **设置同步链路**：
  - App → `SerialProtocol::poll` → `CommandRegistry` → `cmd_config::handle` → MainTask → `parseConfigSetCommand` → `SendDisplaySetting` 队列 → DisplayTask → LVGL
  - 反向：LVGL → `ui_settings_request_apply/save` → MainTask → `applyUiSettingsSnapshot`
- **OTA 流程**：App → `0x0b` → `Upgrade::requestStart()` + `Upgrade::performOta()` → 流式下载 + MD5 校验 → 重启
- **ASR 流程**：触发键按下 → `startCapture()` → Mic 录音 → PSRAM 缓冲 → 触发键松开 → `finishCapture()` → 后台 ASR Task → 腾讯云 TC3 签名 → HTTP POST → 文本结果 → `CMD_VOICE_TEXT` (0x0c) 推 App

---

## 22. 已实现命令清单（完整）

### App → 主控（请求）

| ID   | 命令                    | 描述                                       |
| ---- | ----------------------- | ------------------------------------------ |
| 0x01 | `CMD_CONF_VERSION_GET`  | 获取配置结构版本                            |
| 0x02 | `CMD_CONF_VERSION_SET`  | 写入配置版本                                |
| 0x03 | `CMD_DEVICE_INFO_GET`   | 获取设备名 / 设备 ID / 固件版本            |
| 0x04 | `CMD_DEVICE_INFO_SET`   | 写入设备名 / 序列号                         |
| 0x05 | `CMD_KEYMAP_GET`        | 获取 11 键映射（指定 profile）             |
| 0x06 | `CMD_KEYMAP_SET`        | 写入 11 键映射（直刷运行时）               |
| 0x07 | `CMD_CONFIG_GET`        | 获取 DeviceSettings                        |
| 0x08 | `CMD_CONFIG_SET`        | 设置 DeviceSettings 字段                   |
| 0x0a | `CMD_HEARTBEAT`         | 心跳（自处理回 0x8a）                      |
| 0x0b | `CMD_FIRMWARE_INFO`     | 触发 OTA（url + checksum）                |
| 0x0d | `CMD_PC_STATUS`         | 推送 PC 状态                               |
| 0x0e | `CMD_MUSIC_STATUS`      | 推送音乐播放器状态                          |
| 0x10 | `CMD_PROFILE_STATE`     | 获取/上报 8 个 profile 元数据               |
| 0x11 | `CMD_PROFILE_ICON_SET`  | 上传/删除 Profile 图标（PNG base64）        |
| 0x13 | `CMD_TIME_SET`          | 写入系统时间（epoch + tz）                 |
| 0x14 | `CMD_FIRMWARE_DOWNLOAD` | 复位进 USB 下载模式                         |
| 0x15 | `CMD_PROFILE_NAME_SET`  | 设置 profile 名称（UTF-8 中文）             |
| 0x16 | `CMD_AUDIO_FILE`        | 音效文件管理（op 分发）                     |
| 0x17 | `CMD_AUDIO_PAD`         | 音效板绑定与播放（op 分发）                 |

### 主控 → App（推送）

| 类型                    | 描述                                       |
| ----------------------- | ------------------------------------------ |
| `CMD_KEY_EVENT` (0x09) | 物理按键边沿上报                           |
| `CMD_VOICE_TEXT` (0x0c) | ASR 识别结果文本                           |
| `CMD_MUSIC_CONTROL` (0x0f) | UI 控制按钮触发                          |
| `CMD_HA_STATUS` (0x12) | 2.5s 节流的状态聚合推送                    |
| `CMD_PROFILE_STATE` (0x10) | Profile 状态推送                        |

---

## 23. 文档与规范

| 文档                                                          | 说明                                      |
| ------------------------------------------------------------- | ----------------------------------------- |
| [README.md](../README.md)                                     | 项目主页（快速开始 + 文档索引）           |
| [FEATURE_DOC.md](../FEATURE_DOC.md)                           | 功能需求总览（输入文档，按 18 节展开）    |
| [ARCHITECTURE.md](../ARCHITECTURE.md)                         | 项目结构设计 / 模块划分 / 依赖与同步策略 |
| [PINOUT.md](../PINOUT.md)                                     | 全部硬件引脚分配                          |
| [docs/COMPILING.md](./COMPILING.md)                           | 编译、烧录、SPIFFS 上传、串口监视、擦除  |
| [docs/PROJECT_LAYOUT.md](./PROJECT_LAYOUT.md)                 | 目录速览                                  |
| [docs/TROUBLESHOOTING.md](./TROUBLESHOOTING.md)               | 硬件 / 软件注意事项                       |
| [docs/desktop-app-protocol.md](./desktop-app-protocol.md)     | 桌面 App 通信协议（命令与字段约定）       |
| [docs/01-minimal-hid.md](./01-minimal-hid.md) ~ 10            | 阶段任务计划索引                          |
| [docs/theme-spec/](./theme-spec/)                             | UI 主题系统规格（阶段 09）                |