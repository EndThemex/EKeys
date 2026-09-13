# EKeys 桌面 App 通信协议与接入指南

> 本文基于当前固件源码整理，面向需要与 EKeys 通信的桌面 App 开发者。
>
> 固件：ESP32-S3 N16R8，当前协议实现位于 [`src/protocol/`](../src/protocol/)。
>
> 文档版本：v0.2（按当前源码更新）
>
> 重要：本文描述的是“固件当前已经实现的协议”。部分命令虽然在代码中保留了 ID，但当前尚未真正完成协议收发，详见“已定义但未接通”。

---

## 1. 快速了解

当前固件提供两种桌面 App 通信方式：

```text
桌面 App
 ├─ USB CDC Serial
 │    └─ 115200，ESP32-S3 原生 USB 虚拟串口
 │
 └─ WiFi STA
      ├─ UDP 30001：设备发现
      └─ TCP 30000：控制通道，复用同一套 JSON 行协议
```

协议采用：

```text
一条消息 = 一行 UTF-8 JSON + 换行符 \n
```

命令 ID 范围为 `0x01~0x12`，当前已注册的命令在 [SerialProtocol.h](../src/protocol/SerialProtocol.h#L23-L44) 中定义，由 [registration.cpp](../src/protocol/registration.cpp#L23-L33) 统一注册。

桌面 App 必须同时考虑两种通道：一旦 TCP 已连接，固件发送的协议帧会同时通过 USB CDC 和 TCP 输出，见 [SerialProtocol.cpp](../src/protocol/SerialProtocol.cpp#L130-L145)。

---

## 2. 通信链路

### 2.1 USB CDC

USB CDC 是当前最简单、最稳定的调试和配置方式。

- ESP32-S3 原生 USB 控制器；
- USB D- 为 IO19，USB D+ 为 IO20，参考 [PINOUT.md](../PINOUT.md#L60-L67)；
- 固件构建选项：
  - `ARDUINO_USB_MODE=0`（TinyUSB CDC，注意不是硬件 HWCDC）
  - `ARDUINO_USB_CDC_ON_BOOT=1`
- 配置位于 [platformio.ini](../platformio.ini#L38-L50)；
- **主机必须断言 DTR**：TinyUSB CDC 下固件的 `USBCDC::write()` 在 host 未拉高 DTR 时会静默丢弃全部输出（`tud_cdc_n_connected()` 等价于 DTR 状态）。App 打开串口后必须设置 DTR/RTS（Rust `serialport` crate：`write_data_terminal_ready(true)`；pyserial 默认已拉高，无需处理），否则固件能收到请求但 App 收不到任何回复和日志。
- `main.cpp` 中使用 `Serial.begin(115200)` 初始化，见 [main.cpp](../src/main.cpp#L24-L27)。

CDC 本身不依赖波特率，桌面 App 可以按 115200 打开。实际串口枚举时应优先匹配 USB VID `0x303A`。

USB 串口同时承载两类内容：

```text
[I]MAIN: MainTask started
{"cmd":135,"seq":1,"status":0,"data":{...}}
```

日志和协议会混流。App 应按行读取后只处理以 `{` 开头的行，其他行作为日志或无效行丢弃。不要用固定字节数直接拆包。

### 2.2 WiFi、UDP 发现和 TCP 控制

WiFi 相关配置字段：

- `wifi_switch`：是否启用 WiFi；
- `connect_host`：是否允许连接桌面 App；
- `wifi_ssid`：WiFi 名称；
- `wifi_password`：WiFi 密码。

只有满足以下条件时，固件才会启动桌面 App 发现：

```text
wifi_switch = 1
connect_host = 1
work_mode != 1
```

其中 `work_mode=1` 为 BLE 模式，BLE 模式下 WiFi 会被关闭。

#### UDP 发现流程

设备在 UDP `30001` 端口发送：

```text
FUNKEYBOARD_DISCOVER
```

桌面 App 监听该端口后，回复：

```text
FUNKEYBOARD_HERE
```

设备收到回复后，提取桌面 App 的 IP，并通过回调连接 TCP `30000`。

当前实现的发现参数：

- UDP 端口：`30001`；
- 广播地址：`255.255.255.255`；
- 单轮超时：`800ms`；
- 最多发送 3 轮；
- 无应答时回退 IP：`192.168.31.1`。

实现见 [DiscoveryService.cpp](../src/network/DiscoveryService.cpp#L22-L145)。

> 注意：设备是 UDP 广播的发送方，桌面 App 是 `FUNKEYBOARD_HERE` 的回复方。不是桌面 App 主动广播 `FUNKEYBOARD_DISCOVER`。

#### TCP 控制通道

设备主动连接桌面 App 的 TCP `30000` 端口：

```text
设备 WiFi IP  ->  桌面 App IP:30000
```

TCP 收到的一行 JSON 直接交给 `SerialProtocol` 解析，发送协议帧也直接复用 `SerialProtocol::sendDocument()`。

实现见 [TcpChannel.cpp](../src/network/TcpChannel.cpp#L27-L223)。

当前 TCP 没有认证和加密层。如果局域网不可信，不能把 WiFi 密码、腾讯云 SecretKey 等配置明文暴露给不可信客户端。

#### 重连和离线恢复

TCP 没有 App → 固件的断线通知指令，所有重连由固件内部状态机驱动。App 端在心跳连续失败后只需**保持 TCP `30000` 监听**并继续按 `FUNKEYBOARD_HERE` 应答 UDP `30001` 探测。

固件侧的重连链路：

```text
WiFi 已连接
   └─ TcpChannel::process
        ├─ State::Idle       → 立即触发 DiscoveryService::start（无 delay）
        ├─ State::Connecting → 单次 connect 超时 5s（kTcpConnectTimeoutMs）
        │                       超时后回到 Idle 并 stop UDP，重新发现
        └─ State::Connected  → client->connected() 失效 = link lost
                                 ├─ last_disconnect_ms_ 置位
                                 ├─ 期间 WiFi 仍在线 + Discovery 重新发现 → 回到 Idle
                                 └─ 持续 15s 未恢复（kTcpReviveWifiAfterMs）
                                       └─ WiFiManager::instance().scheduleConnect()
                                              触发 WiFi 整体重连
```

关键常量（[TcpChannel.cpp](../src/network/TcpChannel.cpp#L22-L25)）：

| 常量                    |      值 | 含义                                                    |
| ----------------------- | ------: | ------------------------------------------------------- |
| `kTcpConnectTimeoutMs`  |  `5000` | 单次 TCP connect 超时                                   |
| `kTcpReconnectDelayMs`  |  `2000` | **当前未在状态机中使用**（Idle 直接重新发现，无 delay） |
| `kTcpReviveWifiAfterMs` | `15000` | TCP 断开超过该时长未恢复 → 强制重启 WiFi                |

#### WiFi 重连状态机

[WiFiManager.cpp](../src/network/WiFiManager.cpp#L21-L23) 维护独立的 WiFi 重连：

| 常量                    |      值 | 含义                        |
| ----------------------- | ------: | --------------------------- |
| `kWifiRetryIntervalMs`  |  `5000` | 失败重试间隔                |
| `kWifiConnectTimeoutMs` | `10000` | 单次 connect 超时           |
| `kWifiLinkGraceMs`      | `15000` | 断链后给 STA 自恢复的宽限期 |

状态机：

```text
Idle ──scheduleConnect──> WaitingRetry
                              │  每 5s 一次
                              ▼
                          Connecting ──WL_CONNECTED──> Connected
                              │  10s 超时                    │
                              └──回 WaitingRetry            ▼
                                                  WL_LOST
                                                     │  给 15s 宽限
                                                     │   ├─ WL_CONNECTED 恢复 → 清 link_lost_ms_
                                                     │   └─ 超时 → disconnect(true) + WiFi OFF
                                                     │            → WaitingRetry（下一拍立即重试）
```

断链恢复后回调 [WiFiManager.cpp](../src/network/WiFiManager.cpp#L213-L217) 会重新触发 `on_connected_` 回调，进而再次启动 UDP 发现并重连 TCP。

---

## 3. 帧格式

### 3.1 请求：App → 固件

```json
{ "cmd": 7, "seq": 1, "data": {} }
```

字段：

| 字段   | 类型    | 必填 | 说明                                |
| ------ | ------- | ---: | ----------------------------------- |
| `cmd`  | integer |   是 | 命令 ID                             |
| `seq`  | integer |   否 | App 生成的请求序号；省略时按 0 处理 |
| `data` | object  |   否 | 命令参数；GET 类命令通常省略        |

### 3.2 响应：固件 → App

成功响应：

```json
{"cmd":135,"seq":1,"status":0,"data":{...}}
```

失败响应：

```json
{ "cmd": 135, "seq": 1, "status": 1, "error": "unknown command" }
```

一般约定：

```text
响应 cmd = 请求 cmd | 0x80
```

例如：

```text
0x01 → 0x81
0x03 → 0x83
0x08 → 0x88
```

但 `CMD_PROFILE_STATE`（`0x10`）是当前实现中的例外，详见第 7.2 节。

### 3.3 主动推送

固件主动发送的帧通常使用：

```json
{"cmd":135,"seq":0,"status":0,"data":{...}}
```

`seq=0` 表示这条消息不是某个 App 请求的同步响应。App 应单独处理主动推送，不应把它与请求响应放入同一个待匹配队列。

当前已实现的主要主动帧：

| 命令   | 触发条件                                           |
| ------ | -------------------------------------------------- |
| `0x87` | 配置实际发生变化后，固件推送配置全量快照           |
| `0x10` | Profile 状态变化，或 `CMD_PROFILE_ICON_SET` 成功后 |
| `0x0C` | 语音识别得到非空文本                               |
| `0x0F` | UI 音乐控制发送函数被调用，但当前 UI 链路尚未接通  |

### 3.4 行解析和错误帧

固件以 `\n` 结束一帧，忽略 `\r`，最大行长度为 2048 字节，见 [SerialProtocol.cpp](../src/protocol/SerialProtocol.cpp#L36-L73)。

JSON 解析失败或缺少 `cmd` 时，固件返回：

```json
{ "cmd": 128, "seq": 0, "status": 1, "error": "json parse error" }
```

或：

```json
{ "cmd": 128, "seq": 0, "status": 1, "error": "missing 'cmd' field" }
```

未知命令返回：

```json
{ "cmd": 139, "seq": 1, "status": 1, "error": "unknown command" }
```

App 应始终按 `cmd`、`seq`、`status` 解析，不要依赖响应字段的固定位置。

---

## 4. 命令总览

命令枚举见 [SerialProtocol.h](../src/protocol/SerialProtocol.h#L23-L44)。

|     ID | 命令名                 | 方向       | 当前状态       | 用途                          |
| -----: | ---------------------- | ---------- | -------------- | ----------------------------- |
| `0x01` | `CMD_CONF_VERSION_GET` | App → 固件 | 已接通         | 查询配置结构版本              |
| `0x02` | `CMD_CONF_VERSION_SET` | App → 固件 | 已接通         | 写入配置结构版本              |
| `0x03` | `CMD_DEVICE_INFO_GET`  | App → 固件 | 已接通         | 查询设备信息                  |
| `0x04` | `CMD_DEVICE_INFO_SET`  | App → 固件 | 已接通         | 修改设备名或序列号            |
| `0x05` | `CMD_KEYMAP_GET`       | App → 固件 | 已接通         | 读取当前 Profile 的键映射     |
| `0x06` | `CMD_KEYMAP_SET`       | App → 固件 | 已接通         | 写入当前 Profile 的键映射     |
| `0x07` | `CMD_CONFIG_GET`       | App → 固件 | 已接通         | 读取配置全量快照              |
| `0x08` | `CMD_CONFIG_SET`       | App → 固件 | 已接通         | 增量修改配置                  |
| `0x09` | `CMD_KEY_EVENT`        | 固件 → App | 仅定义         | 预留的物理按键上报            |
| `0x0A` | `CMD_HEARTBEAT`        | 双向       | 已接通         | App 主动探测设备在线          |
| `0x0B` | `CMD_FIRMWARE_INFO`    | App → 固件 | 已接通         | 查询固件或触发 OTA            |
| `0x0C` | `CMD_VOICE_TEXT`       | 固件 → App | 已接通         | 推送语音识别文本              |
| `0x0D` | `CMD_PC_STATUS`        | App → 固件 | 已接通         | 推送 PC 状态                  |
| `0x0E` | `CMD_MUSIC_STATUS`     | App → 固件 | 已接通         | 推送音乐播放器状态            |
| `0x0F` | `CMD_MUSIC_CONTROL`    | 固件 → App | 发送函数已实现 | 推送上一首、播放/暂停、下一首 |
| `0x10` | `CMD_PROFILE_STATE`    | 双向       | 已接通         | 查询或推送当前 Profile        |
| `0x11` | `CMD_PROFILE_ICON_SET` | App → 固件 | 已接通         | 上传或删除 Profile 图标       |
| `0x12` | `CMD_HA_STATUS`        | 主控→App | 仅定义         | 预留的 HA 状态推送（当前仅本机屏消费） |
| `0x13` | `CMD_TIME_SET`         | App→主控 | 已接通         | 写入系统时间（epoch + tz）    |
| `0x14` | `CMD_FIRMWARE_DOWNLOAD`| App→主控 | 已接通         | 复位进 USB 下载模式（烧录）   |
| `0x15` | `CMD_PROFILE_NAME_SET` | App→主控 | 已接通         | 设置/清除 Profile 名称（UTF-8 中文） |
| `0x16` | `CMD_AUDIO_FILE`       | App→主控 | 已接通         | 音效文件管理（data.op 分发）  |
| `0x17` | `CMD_AUDIO_PAD`        | App→主控 | 已接通         | 音效板绑定与播放（data.op 分发） |

命令注册表最多支持 64 个命令，使用 FreeRTOS 临界区保护，见 [CommandRegistry.h](../src/protocol/CommandRegistry.h#L28-L63)。

---

## 5. 配置读写

配置字段定义在 [DeviceSettings.h](../src/config/DeviceSettings.h#L20-L64)，配置快照由 [cmd_config.cpp](../src/protocol/commands/cmd_config.cpp#L37-L78) 序列化。

### 5.1 读取配置：`0x07`

请求：

```json
{ "cmd": 7, "seq": 1 }
```

响应：

```json
{
  "cmd": 135,
  "seq": 1,
  "status": 0,
  "data": {
    "wifi_switch": 1,
    "connect_host": 1,
    "wifi_ssid": "EKeys-Network",
    "wifi_password": "12345678",
    "work_mode": 0,
    "rgb_mode": 1,
    "rgb_single_color": 12,
    "rgb_click_mode": 0,
    "rgb_brightness": 80,
    "tft_theme": 0,
    "tft_brightness": 75,
    "device_volume": 60,
    "audio_enable": 1,
    "power_mode": 0,
    "voice_enable": 1,
    "voice_trigger_key": 11,
    "voice_max_record_ms": 30000,
    "voice_auto_enter": 0,
    "voice_cuid": "EKeys",
    "voice_tencent_secret_id": "...",
    "voice_tencent_secret_key": "...",
    "pc_status_mask": 0,
    "active_keymap_profile": 0,
    "active_profile_name": "Profile 1",
    "active_profile_has_custom_icon": false
  }
}
```

字段说明：

| 字段                             | 类型   | 说明                                                |
| -------------------------------- | ------ | --------------------------------------------------- |
| `wifi_switch`                    | int    | `0` 关闭，`1` 开启                                  |
| `connect_host`                   | int    | 是否允许固件主动连接桌面 App                        |
| `wifi_ssid`                      | string | WiFi 名称，最大 32 字节                             |
| `wifi_password`                  | string | WiFi 密码，最大 64 字节                             |
| `work_mode`                      | int    | `0` USB、`1` BLE、`2` 2.4G                          |
| `rgb_mode`                       | int    | RGB 灯效模式                                        |
| `rgb_single_color`               | int    | 单色颜色索引；字段名中的 `colar` 是当前协议既有拼写 |
| `rgb_click_mode`                 | int    | 按键点击 RGB 模式                                   |
| `rgb_brightness`                 | int    | RGB 亮度                                            |
| `tft_theme`                      | int    | TFT 主题                                            |
| `tft_brightness`                 | int    | 屏幕亮度，范围 `5~100`                              |
| `device_volume`                  | int    | 设备音量                                            |
| `audio_enable`                   | int    | 音频使能                                            |
| `power_mode`                     | int    | 电源模式                                            |
| `voice_enable`                   | int    | 语音功能使能                                        |
| `voice_trigger_key`              | int    | 语音触发键；当前代码范围为 `0~11`                   |
| `voice_max_record_ms`            | int    | 最大录音时长，毫秒；当前代码范围 `1000~60000`       |
| `voice_auto_enter`               | int    | 识别后是否自动进入相关行为                          |
| `voice_cuid`                     | string | 语音 cuid（腾讯云 TC3 协议不使用，保留占位）        |
| `voice_tencent_secret_id`        | string | 腾讯云 SecretId                                     |
| `voice_tencent_secret_key`       | string | 腾讯云 SecretKey                                    |
| `pc_status_mask`                 | int    | PC 状态显示位掩码                                   |
| `active_keymap_profile`          | int    | 当前 Profile，`0~7`                                 |
| `active_profile_name`            | string | 当前 Profile 显示名，由固件生成                     |
| `active_profile_has_custom_icon` | bool   | 当前 Profile 是否有自定义图标                       |

### 5.2 修改配置：`0x08`

请求中的 `data.config` 是增量对象，只发送需要修改的字段：

```json
{
  "cmd": 8,
  "seq": 2,
  "data": {
    "config": {
      "tft_brightness": 50,
      "device_volume": 70
    }
  }
}
```

成功时，固件先返回：

```json
{ "cmd": 136, "seq": 2, "status": 0 }
```

如果至少一个字段实际发生变化，固件随后主动推送配置快照：

```json
{
  "cmd": 135,
  "seq": 0,
  "status": 0,
  "data": {
    "...": "完整配置"
  }
}
```

如果 App 发送的值与当前配置相同，则不推送 `0x87` 快照。

主要副作用：

- `tft_brightness`：立即调整屏幕背光；
- `device_volume`：立即调整扬声器音量；
- `work_mode`：重建 USB/BLE/2.4G 键盘后端；
- `active_keymap_profile`：重新加载对应 Profile 的键映射；
- WiFi 字段变化：触发 WiFi 重连或关闭；
- 其他字段：写入配置存储，并刷新固件内部状态。

配置修改入口见 [cmd_config.cpp](../src/protocol/commands/cmd_config.cpp#L100-L153)，具体字段解析和校验见 [parseConfigSetCommand.cpp](../src/config/parseConfigSetCommand.cpp#L40-L426)。

### 5.3 配置校验规则

| 字段                               | 当前固件行为                                       |
| ---------------------------------- | -------------------------------------------------- |
| `tft_brightness`                   | 小于 5 钳制为 5，大于 100 钳制为 100               |
| `work_mode`                        | 仅接受 `0~2`，越界忽略该字段                       |
| `active_keymap_profile`            | 仅接受 `0~7`                                       |
| `wifi_switch`、`connect_host`      | 归一化为 `0/1`                                     |
| `voice_enable`、`voice_auto_enter` | 归一化为 `0/1`                                     |
| `voice_trigger_key`                | 小于 0 取 0，大于 11 取 11                         |
| `voice_max_record_ms`              | 钳制到 `1000~60000`                                |
| 字符串字段                         | 超过容量时截断；WiFi 密码和腾讯云 Key 最大 64 字节 |
| 未知字段                           | 忽略，不影响请求结果                               |

配置修改在内存层通过一次 `mutateSettings()` 完成，持久化会逐项写入 `/config.ini`。因此协议层看起来是一次请求，但文件写入不是事务式的强一致提交。

---

## 6. 设备信息、版本和固件升级

### 6.1 设备信息：`0x03`

请求：

```json
{ "cmd": 3, "seq": 1 }
```

响应：

```json
{
  "cmd": 131,
  "seq": 1,
  "status": 0,
  "device_info": {
    "device_name": "EKeys",
    "device_id": "AABBCCDDEEFF",
    "firmware_version": "0.6.0"
  }
}
```

- `device_id` 为 12 位大写十六进制，由 ESP32 eFuse MAC 生成；
- 固件版本当前为 `0.6.0`；
- 固件版本常量在 [DeviceIdentity.h](../src/protocol/DeviceIdentity.h#L14-L21)；
- 处理实现见 [cmd_device_info.cpp](../src/protocol/commands/cmd_device_info.cpp#L40-L62)。

### 6.2 修改设备信息：`0x04`

请求：

```json
{
  "cmd": 4,
  "seq": 2,
  "data": {
    "device_name": "My EKeys",
    "serial": "EK-001"
  }
}
```

`device_name` 和 `serial` 至少提供一个，两个字段都可以修改。单个字符串最大 32 字节。

响应：

```json
{
  "cmd": 132,
  "seq": 2,
  "status": 0,
  "data": {
    "device_name": "My EKeys",
    "serial": "EK-001"
  }
}
```

实现见 [cmd_device_info.cpp](../src/protocol/commands/cmd_device_info.cpp#L64-L119)。

### 6.3 配置版本：`0x01/0x02`

查询：

```json
{ "cmd": 1, "seq": 1 }
```

响应：

```json
{ "cmd": 129, "seq": 1, "status": 0, "data": { "version": 1 } }
```

设置：

```json
{ "cmd": 2, "seq": 2, "data": { "version": 1 } }
```

`version` 合法范围是 `0~65535`。查询读取的是运行时实际保存的配置版本，不是固定返回常量。

实现见 [cmd_firmware.cpp](../src/protocol/commands/cmd_firmware.cpp#L32-L81)。

### 6.4 固件信息：`0x0B`

无 `data.url` 时查询固件信息：

```json
{ "cmd": 11, "seq": 1 }
```

响应：

```json
{
  "cmd": 139,
  "seq": 1,
  "status": 0,
  "firmware": {
    "version": "0.6.0",
    "device": "EKeys",
    "build_date": "Sep  7 2026",
    "build_time": "12:00:00"
  }
}
```

### 6.5 OTA 升级

携带 URL 和 MD5 时，固件将 `0x0B` 解释为 OTA 触发：

```json
{
  "cmd": 11,
  "seq": 2,
  "data": {
    "url": "http://192.168.31.1/firmware.bin",
    "checksum": "0123456789abcdef0123456789abcdef"
  }
}
```

协议约定：

- `checksum` 应为固件 MD5 十六进制字符串，长度为 32；
- 固件当前实现只检查 `checksum` 长度为 32；
- 固件先回 `cmd=0x8B` 的成功响应，再执行 OTA；
- 成功后自动重启。

实现见 [cmd_firmware.cpp](../src/protocol/commands/cmd_firmware.cpp#L83-L121)。OTA 前应确保 `url` 可被 ESP32 访问，且本地局域网或服务器允许下载。

### 6.6 复位进 USB 下载模式：`0x14`

App 可远程让主控立即重启进入 USB 烧录模式（无需按 BOOT 键）：

```json
{ "cmd": 14, "seq": 1, "data": {} }
```

固件先回 `cmd=0x94` 成功响应，再延迟数百毫秒后调用 `esp_restart()` 拉低 GPIO0 并复位。App 收到响应后可立即通过 USB CDC 重新枚举并执行 `pio run -t upload`。

实现见 [cmd_firmware.cpp](../src/protocol/commands/cmd_firmware.cpp)。

### 6.7 Profile 名称（UTF-8 中文）：`0x15`

设置或清除指定 Profile 的显示名：

```json
{
  "cmd": 21,
  "seq": 1,
  "data": {
    "profile": 0,
    "name": "剪辑"
  }
}
```

字段：

| 字段      | 类型    | 必填 | 说明                                                           |
| --------- | ------- | ---- | -------------------------------------------------------------- |
| `profile` | integer | 否   | Profile 索引 0~7；省略时使用当前激活 Profile                  |
| `name`    | string  | 是   | 名称字符串；空串 = 清除自定义名（恢复 LVGL 内置符号）        |

成功响应：

```json
{
  "cmd": 149,
  "seq": 1,
  "status": 0,
  "data": {
    "profile": 0,
    "profile_number": 1,
    "profile_name": "剪辑",
    "is_custom_name": true
  }
}
```

实现见 [cmd_profile.cpp](../src/protocol/commands/cmd_profile.cpp#L238-L307)。

### 6.8 系统时间注入：`0x13`

把桌面 App 持有的系统时间注入主控，替代或优先于 NTP 同步：

```json
{
  "cmd": 19,
  "seq": 1,
  "data": {
    "epoch": 1757236200,
    "tz": "CST-8"
  }
}
```

字段：

- `epoch`：自 `1970-01-01 00:00:00 UTC` 起的秒数，整数；必填
- `tz`：POSIX TZ 字符串（如 `CST-8`、`UTC`），可选；省略或空字符串保持当前 TZ（默认 `CST-8`）

行为：

- 主控调用 `settimeofday()` 写入系统时间，`setenv("TZ", ...)` 切换时区；
- 写入后立即标记 `synced_ = true`，下一 1s tick 主屏刷新时间（`HH:MM:SS`）+ 日期（`MMM DD`，例如 `SEP 07`）+ 星期缩写（`MON`/`TUE`/.../`SUN`）；
- 若 NTP 同步尚未完成，写入即生效；若 NTP 已同步，请求后 SNTP 后续仍可能再次校时，行为可接受；
- `epoch` 缺失时返回 `status: 1, error: "missing 'epoch'"`。

实现见 [cmd_time.cpp](../src/protocol/commands/cmd_time.cpp) 与 [NtpSync.cpp](../src/network/NtpSync.cpp#L52-L79)。

---

## 7. 音效板（Sound Pad）

`CMD_AUDIO_FILE` (0x16) 与 `CMD_AUDIO_PAD` (0x17) 是配套命令：0x16 管理 SPIFFS 上的音频文件，0x17 管理 11 键绑定与播放控制。

### 7.1 文件管理：`0x16`

按 `data.op` 分发：

| `op`       | 请求 `data`              | 响应 `data`                                              |
| ---------- | ------------------------ | -------------------------------------------------------- |
| `list`     | —                        | `files:[{name,size}]`（≤64 条）、`total_bytes/used_bytes/free_bytes` |
| `begin`    | `{name, size}`           | ack；校验后创建 `/name.part`                             |
| `data`     | `{name, index, b64}`     | `{received:N}`；1024 B / 块（base64 后 ~1.4 KB < 2048 行缓冲） |
| `end`      | `{name, size}`           | 校验大小一致 → `SPIFFS.rename(.part → 终名)` 原子提交 → 回 `free_bytes` |
| `abort`    | `{name}`                 | 删 `.part`（App 取消 / 失败回滚）                        |
| `delete`   | `{name}`                 | 删终名；先清绑定表中引用该文件的键 → `{pads:[{key,file}], free_bytes}` |

约束（固件侧强校验）：

- 文件名白名单 `^[a-z0-9_]{1,20}\.(mp3|wav)$`，存 SPIFFS **根目录**
- 单文件 ≤ 2 MB；`begin` 时校验 `free_bytes ≥ size + 64 KB headroom`
- 上传互斥：同一时刻仅一个 in_progress；正在播放的同名文件拒绝 `delete`

### 7.2 绑定与播放：`0x17`

| `op`   | 请求 `data`            | 响应 `data`                                                |
| ------ | ---------------------- | ---------------------------------------------------------- |
| `get`  | —                      | `pads:[{key, file}]`（11 键全量，空绑定 `""`）            |
| `set`  | `{key, file}`          | 单键即改即发，`file:""` 清除；先 ACK 再落盘 → `{key, file}` |
| `play` | `{key}` 或 `{file}`    | 立即 ACK（试播不亮键位高亮）                               |
| `stop` | —                      | ACK                                                        |

字段：

- `key`：1~11；省略时按 `file` 试播
- `file`：文件名（不含路径），白名单同 0x16；空串清除

### 7.3 报文示例

列出文件：

```json
{ "cmd": 22, "seq": 1, "data": { "op": "list" } }
```

```json
{
  "cmd": 150,
  "seq": 1,
  "status": 0,
  "data": {
    "files": [
      { "name": "kick.mp3", "size": 24576 },
      { "name": "coin2.wav", "size": 8192 }
    ],
    "total_bytes": 4055040,
    "used_bytes": 32768,
    "free_bytes": 4022272
  }
}
```

分块上传 1024 B（base64 内嵌 JSON，索引 `index` 从 0 开始）：

```json
{
  "cmd": 22,
  "seq": 3,
  "data": {
    "op": "data",
    "name": "kick.mp3",
    "index": 0,
    "b64": "SUQzAwAAAAA..."
  }
}
```

设置键 1 绑定：

```json
{
  "cmd": 23,
  "seq": 1,
  "data": { "op": "set", "key": 1, "file": "kick.mp3" }
}
```

实现见 [cmd_audio.cpp](../src/protocol/commands/cmd_audio.cpp)。

---

## 8. 键映射、Profile 和图标

### 8.1 键映射：`0x05/0x06`

#### 查询键映射

请求：

```json
{ "cmd": 5, "seq": 1 }
```

响应：

```json
{
  "cmd": 133,
  "seq": 1,
  "status": 0,
  "fun_key1": 1,
  "fun_key2": 0,
  "keymap": [
    {
      "physical": 1,
      "normal": "Ctrl+Shift+A",
      "macro": "",
      "text": "",
      "function": "",
      "combo1_normal": "",
      "combo1_text": "",
      "combo1_function": "",
      "combo2_normal": "",
      "combo2_text": "",
      "combo2_function": ""
    },
    {
      "physical": 2,
      "normal": "b",
      "macro": "",
      "text": "",
      "function": "MEDIA_PLAY",
      "combo1_normal": "Ctrl+c",
      "combo1_text": "",
      "combo1_function": "",
      "combo2_normal": "",
      "combo2_text": "",
      "combo2_function": ""
    }
  ]
}
```

每个 Profile 有 11 个物理键，`physical` 范围为 `1~11`。

- 顶层 `fun_key1` / `fun_key2`：FUN 组合键配置，`0~11`，`0` = 未配置；
- 每键 `combo1_*` / `combo2_*`：FUN 组合层输出（见下方触发语义）。

#### FUN 组合触发语义

FUN 键在设备端「设置二级页 → FUN键 1 / FUN键 2」或 `0x06` 的
`fun_key1` / `fun_key2` 字段配置（`0` = 关闭；两个 FUN 键是两个独立的
组合层）：

- 任一 FUN 键**先按住**，其它键在 FUN 按住期间按下 → 触发该键的
  `combo1_*`（FUN1 层）或 `combo2_*`（FUN2 层）组合输出；松开该键 →
  释放组合输出。两个 FUN 键同时按住时 FUN1 层优先；
- 其它键先按下（已触发单击输出）后再按 FUN → 不追加组合，保持单击状态；
- FUN 键本身按下/松开不产生任何 HID 输出；
- `fun_key1=0` 且 `fun_key2=0` 时组合功能关闭，所有键按单击行为；
- 组合层内部优先级与单击相同：`function > text > normal`；组合层与
  单击通道互相独立存储、互不影响。

#### 写入键映射

请求：

```json
{
  "cmd": 6,
  "seq": 2,
  "data": {
    "fun_key1": 1,
    "fun_key2": 0,
    "keymap": [
      {
        "physical": 1,
        "normal": "a+b",
        "macro": "",
        "text": "",
        "function": "",
        "combo1_normal": "",
        "combo1_text": "",
        "combo1_function": "",
        "combo2_normal": "",
        "combo2_text": "",
        "combo2_function": ""
      },
      {
        "physical": 2,
        "normal": "Ctrl+c",
        "macro": "",
        "text": "",
        "function": "",
        "combo1_normal": "Ctrl+Shift+c",
        "combo1_text": "",
        "combo1_function": "",
        "combo2_normal": "",
        "combo2_text": "",
        "combo2_function": ""
      },
      {
        "physical": 3,
        "normal": "",
        "macro": "",
        "text": "hello@example.com",
        "function": "",
        "combo1_normal": "",
        "combo1_text": "",
        "combo1_function": "",
        "combo2_normal": "",
        "combo2_text": "",
        "combo2_function": ""
      }
    ]
  }
}
```

规则：

- `normal` 和 `macro` 使用 `+` 分隔多个键；
- 组合键写在 `normal`（如 `Ctrl+c`、`Ctrl+Shift+A`）：前缀段为修饰键名
  （大小写不敏感：`Ctrl`/`Control`、`Shift`、`Alt`/`Option`、`Win`/`GUI`/`Meta`/`Cmd`），
  按住物理键期间修饰键与普通键同时保持按下，松开时一并释放；
  字母建议小写（大写字母会按普通键规则自动附带 Shift）；
- `text` 为文本注入串（如 `hello@example.com`）：按键触发整串输出一次，
  仅支持 ASCII（HID 键盘固有限制，不支持中文），上限 128 字符（超长截断）；
- 单击通道优先级：`function` > `text` > `normal` > `macro`，高优先级字段
  非空时其余字段忽略；
- `macro` 为宏序列（依次按下并释放），**设备端暂未实现宏播放，请勿使用**；
- `function` 非空时优先，`text` / `normal` / `macro` 留空；`function` 也可写
  单槽组合键（如 `Ctrl+c`，整串传入不拆槽）；
- `combo1_*` / `combo2_*` 为 FUN 组合层通道，规则同单击通道（层内优先级
  `function > text > normal`，`combo*_text` 同样 ≤128 字符截断），与单击
  通道互相独立，可同时配置；
- `fun_key1` / `fun_key2` 为**可选**字段：出现时校验 `0~11` 并持久化到
  config.ini `[system]`，缺省保持现值，越界返回错误；
- 至少要有一个合法物理键映射，否则返回错误；
- 键映射写入当前激活 Profile；成功后立即调用 `MainTask::reloadKeymap()`。

实现见 [cmd_keymap.cpp](../src/protocol/commands/cmd_keymap.cpp#L83-L185)。

### 8.2 Profile 状态：`0x10`

App 查询当前 Profile：

```json
{ "cmd": 16, "seq": 3 }
```

当前固件返回：

```json
{
  "cmd": 16,
  "seq": 3,
  "profile_state": {
    "active_profile": 0,
    "profile_number": 1,
    "profile_name": "Profile 1",
    "has_custom_icon": true,
    "icon_path": "/icon1.png"
  }
}
```

这是一个当前实现例外：

- 不返回 `cmd=0x90`；
- 没有 `status` 字段；
- `profile_state` 位于顶层，而不是 `data.profile_state`；
- 固件主动推送时使用 `cmd=16`、`seq=0`。

实现见 [cmd_profile.cpp](../src/protocol/commands/cmd_profile.cpp#L171-L193)。App 端应按命令名称识别，不要把所有响应都按 `cmd | 0x80` 解析。

### 8.3 Profile 图标：`0x11`

上传图标：

```json
{
  "cmd": 17,
  "seq": 4,
  "data": {
    "profile_icon": {
      "profile": 0,
      "clear": false,
      "png_base64": "iVBORw0KGgo..."
    }
  }
}
```

字段：

| 字段         | 说明                                             |
| ------------ | ------------------------------------------------ |
| `profile`    | Profile 索引，范围 `0~7`；省略时使用当前 Profile |
| `clear`      | 为 `true` 时删除图标，不接收 Base64              |
| `png_base64` | PNG 文件的 Base64 编码；需要 `clear=false`       |

成功响应：

```json
{
  "cmd": 145,
  "seq": 4,
  "status": 0,
  "data": {
    "profile": 0,
    "profile_number": 1,
    "has_custom_icon": true,
    "profile_name": "Profile 1"
  }
}
```

成功后，固件还会主动推送一条 `cmd=16, seq=0` 的 Profile 状态。

当前固件代码会解码 Base64 并写入 SPIFFS，但**没有在协议处理函数中实际校验 PNG 尺寸**；注释中提到的 `48×48 PNG` 不能视为当前协议层的强制保证。实现见 [cmd_profile.cpp](../src/protocol/commands/cmd_profile.cpp#L41-L89)。

由于主协议行缓冲区上限是 2048 字节，较大的 Base64 图标可能超过单帧限制。App 端应控制图片大小，或者在固件提供分帧/二进制通道后再传输大图。

---

## 9. 心跳、状态和音乐

### 9.1 心跳：`0x0A`

App 定时发送：

```json
{ "cmd": 10, "seq": 3 }
```

固件响应：

```json
{
  "cmd": 138,
  "seq": 3,
  "status": 0,
  "data": {
    "timestamp": 123456,
    "device": "EKeys"
  }
}
```

建议 App：

1. 每 1~3 秒发送一次心跳（经验值，非协议硬约束，可按场景调整）；
2. 连续多次未收到响应后判定离线；
3. `timestamp` 是设备开机后的 `millis()`，可用于粗略判断设备是否重启；
4. 固件不会主动周期发送心跳。

### 9.2 PC 状态：`0x0D`

推送 PC 状态到设备：

```json
{
  "cmd": 13,
  "seq": 1,
  "data": {
    "pc_status": {
      "network_connected": true,
      "cpu_usage_percent": 35.2,
      "memory_usage_percent": 61.8,
      "cpu_temp_c": 52.0,
      "disk_io_percent": 4.1,
      "network_up_kbps": 120.5,
      "network_down_kbps": 880.2
    }
  }
}
```

支持字段：

- `network_connected`
- `cpu_usage_percent`
- `memory_usage_percent`
- `cpu_temp_c`
- `disk_io_percent`
- `network_up_kbps`
- `network_down_kbps`

未实现（推送会被静默丢弃）：`caps_lock / num_lock / scroll_lock / on_ac_power / battery_percent`。电量走 `BatteryStatus` 命令。

配置 PC 状态掩码：

```json
{
  "cmd": 13,
  "seq": 2,
  "data": {
    "pc_status": {
      "type": "config",
      "mask": 4294967295
    }
  }
}
```

实现见 [cmd_pc_status.cpp](../src/protocol/commands/cmd_pc_status.cpp#L31-L78)。

### 9.3 音乐状态：`0x0E`

App 推送播放器状态：

```json
{
  "cmd": 14,
  "seq": 1,
  "data": {
    "music_status": {
      "connected": true,
      "is_playing": true,
      "is_paused": false,
      "can_prev": true,
      "can_next": true,
      "position_ms": 125000,
      "duration_ms": 240000,
      "title": "Song Title",
      "artist": "Artist",
      "player": "PC MUSIC",
      "lyric_current": "current lyric",
      "lyric_next": "next lyric"
    }
  }
}
```

固件将毫秒进度转换为秒后更新音乐屏。实现见 [cmd_music.cpp](../src/protocol/commands/cmd_music.cpp#L60-L95)。

### 9.4 音乐控制：`0x0F`

固件端协议函数定义的动作格式为：

```json
{
  "cmd": 15,
  "seq": 0,
  "music_control": {
    "action": "toggle"
  }
}
```

动作值预期为：

| 值       | 含义      |
| -------- | --------- |
| `prev`   | 上一首    |
| `toggle` | 播放/暂停 |
| `next`   | 下一首    |

实现位于 [SerialProtocol.cpp](../src/protocol/SerialProtocol.cpp#L170-L178)。目前没有找到 UI 事件到 `sendMusicControl()` 的实际调用链，因此桌面 App 可以按该格式解析，但当前不能依赖设备端一定发送此消息。

### 9.5 语音文本：`0x0C`

语音识别得到文本后，固件主动推送：

```json
{
  "cmd": 12,
  "seq": 0,
  "text": "识别出的文字",
  "timestamp": 123456
}
```

`timestamp` 为设备运行时间。发送函数在 [SerialProtocol.cpp](../src/protocol/SerialProtocol.cpp#L159-L168)，调用点在 [VoiceRecognizer.cpp](../src/voice/VoiceRecognizer.cpp#L395-L405)。

---

## 10. 已定义但当前未接通的命令

### 10.1 `CMD_KEY_EVENT`：`0x09`

命令 ID 已在协议头文件中定义，但当前源码没有实现 `CMD_KEY_EVENT` 序列化发送。

因此，App 当前不能依赖固件上报：

- 物理键按下/释放边沿；
- 物理键 ID；
- 键映射执行结果。

物理按键目前主要走本机 HID/UI 路径。

### 10.2 `CMD_HA_STATUS`：`0x12`

命令 ID 已定义，但当前 `MainTask` 只是将 HA 状态聚合为 `DisplayMessageType::HaStatus` 刷新本机 UI，尚未转换为 `cmd=0x12` 协议帧发送给桌面 App。

相关内部状态见：

- [`MainTask.cpp`](../src/tasks/MainTask.cpp#L507-L540)
- [`NetDiagnostics.cpp`](../src/network/NetDiagnostics.cpp#L19-L47)
- [`message_types.h`](../src/message_types.h#L62-L76)

### 10.3 Profile 图标尺寸校验

`0x11` 注释中描述了 `48×48 PNG`，但当前协议处理代码只做 Base64 解码和文件写入，没有尺寸校验。App 应自行保证图片格式和大小。

---

## 11. 桌面 App 接入流程

### 11.1 USB 方式

推荐流程：

```text
1. 枚举 USB CDC 串口
2. 打开 115200 CDC
3. 逐行读取，过滤日志
4. 发送 0x01 查询配置版本
5. 发送 0x07 读取配置快照
6. 建立 seq 响应匹配
7. 定时发送 0x0A 心跳
8. 监听 0x87 主动配置快照
9. 监听 0x0C 等主动消息
```

### 11.2 WiFi/TCP 方式

推荐流程：

```text
1. 固件连接到配置好的 WiFi
2. 桌面 App 监听 UDP 30001
3. 收到 FUNKEYBOARD_DISCOVER 后回复 FUNKEYBOARD_HERE
4. 桌面 App 监听 TCP 30000
5. 接受固件主动连接
6. 将 TCP 按行切分并使用同一套 JSON 解析器
7. 同时维护心跳和 seq
```

WiFi 模式启动和网络回调见 [MainTask.cpp](../src/tasks/MainTask.cpp#L162-L175)。

App 端的离线/重连行为要点（与 §2.2 重连状态机对应）：

- **协议层不提供 App → 固件的断线通知指令**。App 不能主动告诉固件"我断开了"；
- TCP 断开后，App 只需保持 `TCP 30000` 监听 + 继续应答 `FUNKEYBOARD_DISCOVER`，由固件侧 `TcpChannel` / `WiFiManager` 自动恢复；
- 心跳连续失败并不立即触发任何固件端动作；App 在判定离线后建议降低心跳频率或暂停一段时间再恢复探测，避免在 WiFi 重连窗口期反复发起请求。

### 11.3 请求/响应匹配

App 建议为每个请求生成唯一自增的 `seq`，并维护待响应队列：

```text
发送请求(seq=N)
        │
        ├─ 收到 cmd = request_cmd | 0x80 且 seq=N
        │      └─ 作为请求响应处理
        │
        └─ 收到 seq=0 或 seq 不匹配
               └─ 作为主动消息或异常消息处理
```

如果 USB 和 TCP 同时在线，固件可能从两条通道各发送一次相同内容。App 端应根据 `seq` 去重，或者只选择一个主动维护的连接。

### 11.4 最小 Python USB 示例

```python
import json
import time
import serial


def read_frames(ser):
    while True:
        line = ser.readline()
        if not line:
            return None

        text = line.decode("utf-8", errors="ignore").strip()
        if not text.startswith("{"):
            continue

        try:
            return json.loads(text)
        except json.JSONDecodeError:
            continue


def send(ser, cmd, seq, data=None):
    frame = {"cmd": cmd, "seq": seq}
    if data is not None:
        frame["data"] = data
    ser.write((json.dumps(frame, separators=(",", ":")) + "\n").encode("utf-8"))


# 使用时替换为实际 CDC 端口
with serial.Serial("COM5", 115200, timeout=1) as ser:
    time.sleep(1)

    send(ser, 1, 1)       # 配置版本
    print(read_frames(ser))

    send(ser, 7, 2)       # 配置全量快照
    print(read_frames(ser))

    send(ser, 8, 3, {     # 修改屏幕亮度
        "config": {
            "tft_brightness": 50
        }
    })
    print(read_frames(ser))  # 0x88
    print(read_frames(ser))  # 0x87 主动快照
```

实际设备端口可能不同。Windows 下应枚举串口并优先选择 VID `0x303A` 的 ESP32 USB CDC 设备。

---

## 12. 异常处理和兼容性建议

### 12.1 必须忽略未知字段和未知命令

协议后续版本可能增加字段。App 应：

- 保留已知字段；
- 忽略未知字段；
- 对未知命令记录日志，不应导致界面崩溃；
- 不应因为当前没有 `status` 字段就丢弃整个 JSON。

### 12.2 不应依赖固定行长度

App 始终使用 `\n` 分帧，不应假设每条消息固定长度。USB 和 TCP 都是流式连接。

### 12.3 注意日志和协议混流

只有以 `{` 开头的行才应进入协议解析器。日志行可能以 `[` 开头，也可能包含 JSON 文本片段，不能简单查找第一个 `{`。

### 12.4 配置和密钥信息

`CMD_CONFIG_GET` 会返回以下敏感信息：

- `wifi_password`
- `voice_tencent_secret_id`
- `voice_tencent_secret_key`

桌面 App 不应将完整快照写入普通日志，也不要通过不可信局域网长期暴露 TCP 30000。

### 12.5 大帧限制

当前每帧最大 2048 字节：

- 普通 JSON 控制帧没有问题；
- 大型 Profile PNG 的 Base64 可能超过限制；
- 当前没有协议分帧机制；
- 大图传输需要后续改为独立二进制文件通道或增加长度字段/分片协议。

### 12.6 识别命令格式差异

`0x10 Profile State` 不是标准 `cmd | 0x80` 响应。解析时建议优先判断命令类型，再判断是否存在 `status`：

```python
if frame.get("cmd") == 0x10 and "profile_state" in frame:
    # Profile 查询响应或主动推送
    pass
elif frame.get("cmd", 0) & 0x80:
    # 普通命令响应
    pass
```

---

## 13. 当前实现状态和后续建议

当前项目已经具备：

- USB CDC JSON 行协议；
- WiFi STA；
- UDP 30001 设备发现；
- TCP 30000 控制通道；
- 配置、设备信息、固件、键映射、Profile、PC 状态、音乐状态等命令；
- 配置和 Profile 的主动同步；
- 语音识别文本推送；
- 心跳机制。

当前仍建议桌面 App 按以下方式处理：

| 功能         | 建议                                                      |
| ------------ | --------------------------------------------------------- |
| 首次连接     | 发送 `0x01` 和 `0x07` 获取设备能力及初始配置              |
| 在线检测     | 定时发送 `0x0A`                                           |
| 配置修改     | 发送 `0x08`，同时接收 `0x88` 和可能的 `0x87`              |
| Profile 状态 | 单独解析 `0x10`，不要期待 `0x90`                          |
| 键映射编辑   | 发送 `0x06` 后重新读取或刷新 UI                           |
| PC/音乐状态  | 周期性发送 `0x0D` 和 `0x0E`                               |
| OTA          | 先用 `0x0B` 查询，再携带 URL 和 MD5 触发                  |
| 语音文本     | 监听主动 `0x0C`                                           |
| 音效板       | 连接后先 `0x17 get` + `0x16 list`，再分块 `0x16 begin/data/end` 上传，`0x17 set` 绑定 11 键 |
| 系统时间注入 | 发送 `0x13`（epoch + tz）覆盖或优先于 NTP                 |
| 设备发现     | 监听 UDP 30001，回复 `FUNKEYBOARD_HERE`，再等待 TCP 30000 |

后续若要完成完整桌面 App 联动，还需要补齐：

- `0x09 CMD_KEY_EVENT` 物理按键上报；
- `0x0F` UI 音乐控制到协议发送的调用链；
- `0x12 CMD_HA_STATUS` 的协议序列化和发送；
- 协议认证/加密；
- 超过 2048 字节的数据传输方案；
- 更明确的协议版本协商和兼容策略。

---

## 14. 主要源码索引

| 内容               | 文件                                                                            |
| ------------------ | ------------------------------------------------------------------------------- |
| 命令枚举           | [`SerialProtocol.h`](../src/protocol/SerialProtocol.h#L23-L44)                  |
| JSON 行解析和发送  | [`SerialProtocol.cpp`](../src/protocol/SerialProtocol.cpp#L36-L215)             |
| 命令注册表         | [`CommandRegistry.cpp`](../src/protocol/CommandRegistry.cpp#L29-L81)            |
| 命令统一注册       | [`registration.cpp`](../src/protocol/registration.cpp#L23-L33)                  |
| 配置读写           | [`cmd_config.cpp`](../src/protocol/commands/cmd_config.cpp#L37-L165)            |
| 配置字段解析       | [`parseConfigSetCommand.cpp`](../src/config/parseConfigSetCommand.cpp#L40-L426) |
| 设备信息           | [`cmd_device_info.cpp`](../src/protocol/commands/cmd_device_info.cpp#L40-L119)  |
| 固件和 OTA         | [`cmd_firmware.cpp`](../src/protocol/commands/cmd_firmware.cpp#L32-L121)        |
| 键映射             | [`cmd_keymap.cpp`](../src/protocol/commands/cmd_keymap.cpp#L83-L185)            |
| PC 状态            | [`cmd_pc_status.cpp`](../src/protocol/commands/cmd_pc_status.cpp#L31-L78)       |
| 音乐状态           | [`cmd_music.cpp`](../src/protocol/commands/cmd_music.cpp#L60-L95)               |
| Profile 状态、图标、名称 | [`cmd_profile.cpp`](../src/protocol/commands/cmd_profile.cpp)                  |
| 系统时间注入       | [`cmd_time.cpp`](../src/protocol/commands/cmd_time.cpp)                         |
| 音效板（文件+绑定+播放） | [`cmd_audio.cpp`](../src/protocol/commands/cmd_audio.cpp) / [`AudioPad.cpp`](../src/audio/AudioPad.cpp) |
| TCP 通道           | [`TcpChannel.cpp`](../src/network/TcpChannel.cpp#L27-L223)                      |
| UDP 发现           | [`DiscoveryService.cpp`](../src/network/DiscoveryService.cpp#L22-L145)          |
| WiFi 管理          | [`WiFiManager.cpp`](../src/network/WiFiManager.cpp#L34-L47)                     |
| 协议轮询和网络调度 | [`MainTask.cpp`](../src/tasks/MainTask.cpp#L194-L231)                           |
| HA 内部状态聚合    | [`MainTask.cpp`](../src/tasks/MainTask.cpp#L521-L540)                           |
