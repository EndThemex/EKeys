# EKeys 桌面 App 开发对接文档

> 版本：v0.1（对应固件阶段 04）
> 固件协议代码：[`src/protocol/`](../src/protocol/)；字段定义：[`FEATURE_DOC.md §5/§6`](../FEATURE_DOC.md)
> 本文档面向桌面 App 开发者，描述需要实现的通信功能、协议格式与字段约定。

---

## 1. 通信概述

| 项               | 说明                                                                       |
| ---------------- | -------------------------------------------------------------------------- |
| 物理链路（当前） | USB CDC 虚拟串口（ESP32-S3 USB 设备，非 UART0 烧录口）                     |
| 波特率           | 115200（CDC 忽略波特率，任意值均可）                                       |
| 帧格式           | JSON 行：一帧 = 一行 UTF-8 JSON + `\n`（`\r` 会被固件剥离）                |
| 后续扩展         | 阶段 06 接入 TCP（端口 30000）+ UDP 自动发现（端口 30001），协议帧格式不变 |
| 字段命名         | `lower_snake_case`，与固件 `DeviceSettings` 字段一致                       |

### 1.1 重要：日志与协议共用端口

固件日志（形如 `[I]MAIN: ...`）与协议 JSON 行走同一个 CDC 端口。**App 必须逐行过滤：只解析以 `{` 开头的行，其余按日志丢弃/展示。**

### 1.2 帧结构

请求（App → 设备）：

```json
{"cmd": <int 命令ID>, "seq": <int 序列号>, "data": { ... }}
```

- `seq` 由 App 生成（建议自增），响应原样回带，用于请求-响应配对
- `data` 可省略（如 GET 类命令）

响应（设备 → App）：

```json
{"cmd": <请求cmd | 0x80>, "seq": <int>, "status": 0}
{"cmd": <请求cmd | 0x80>, "seq": <int>, "status": 1, "error": "<错误信息>"}
```

- `status = 0` 成功；`status = 1` 失败，`error` 为人读信息
- 响应命令 ID = 请求命令 ID | 0x80（如 `0x07 → 0x87`）
- 固件会忽略 App 发来的响应包（`cmd` 最高位为 1 的帧）

---

## 2. 命令清单与实现状态

| ID        | 命令                       | 方向     | 状态                  | 说明                                                                                                     |
| --------- | -------------------------- | -------- | --------------------- | -------------------------------------------------------------------------------------------------------- |
| 0x01      | `CMD_CONF_VERSION_GET`     | App→设备 | **已实现**            | 配置版本号                                                                                               |
| 0x02      | `CMD_CONF_VERSION_SET`     | App→设备 | **已实现（阶段 07）** | `data.version` 写入并持久化                                                                              |
| 0x03      | `CMD_DEVICE_INFO_GET`      | App→设备 | **已实现**            | 设备信息                                                                                                 |
| 0x04      | `CMD_DEVICE_INFO_SET`      | App→设备 | **已实现（阶段 07）** | `data.device_name` / `data.serial` 写入并持久化                                                          |
| 0x05      | `CMD_KEYMAP_GET`           | App→设备 | 规划（阶段 05/07）    | 键映射读取                                                                                               |
| 0x06      | `CMD_KEYMAP_SET`           | App→设备 | 规划（阶段 05/07）    | 键映射写入                                                                                               |
| **0x07**  | **`CMD_CONFIG_GET`**       | App→设备 | **已实现**            | 读取全部设置（§3）                                                                                       |
| **0x08**  | **`CMD_CONFIG_SET`**       | App→设备 | **已实现**            | 原子写入设置（§4）                                                                                       |
| 0x09      | `CMD_KEY_EVENT`            | 设备→App | 规划                  | 按键边沿上报                                                                                             |
| **0x0a**  | **`CMD_HEARTBEAT`**        | 双向     | **已实现**            | 心跳（§5）                                                                                               |
| 0x0b      | `CMD_FIRMWARE_INFO`        | App→设备 | **已实现（阶段 07）** | 无 data：查询固件信息；`data.url`+`data.checksum`（MD5 hex）：触发 OTA，校验失败不覆盖固件，成功自动重启 |
| 0x0d      | `CMD_PC_STATUS`            | App→设备 | 规划（阶段 05）       | PC 状态位掩码推送                                                                                        |
| 0x0e/0x0f | `CMD_MUSIC_STATUS/CONTROL` | 双向     | 规划（阶段 05）       | 音乐控制                                                                                                 |
| 0x10      | `CMD_PROFILE_STATE`        | 双向     | 规划                  | Profile 状态                                                                                             |
| 0x11      | `CMD_PROFILE_ICON_SET`     | App→设备 | 规划                  | 图标上传（PNG base64）                                                                                   |
| 0x12      | `CMD_HA_STATUS`            | 设备→App | 规划（阶段 06）       | 状态聚合推送                                                                                             |

未注册命令会收到 `status=1, error="unknown command"`，固件不崩溃。

---

## 3. CMD_CONFIG_GET（0x07）— 读取设置

请求：

```json
{ "cmd": 7, "seq": 1 }
```

响应（`cmd=0x87`）`data` 字段全集：

| 字段                             | 类型   | 范围/说明                 | 生效阶段          |
| -------------------------------- | ------ | ------------------------- | ----------------- |
| `wifi_switch`                    | int    | 0/1                       | 06                |
| `connect_host`                   | int    | 0/1                       | 06                |
| `wifi_ssid`                      | string | ≤32 字节                  | 06                |
| `wifi_password`                  | string | ≤64 字节                  | 06                |
| `work_mode`                      | int    | 0=USB 1=BLE 2=2.4G        | 04（仅 USB 生效） |
| `rgb_mode`                       | int    | 0~255                     | 06                |
| `rgb_single_colar`               | int    | 0~255                     | 06                |
| `rgb_click_mode`                 | int    | 0~255                     | 06                |
| `rgb_brightness`                 | int    | 0~100                     | 06                |
| `tft_theme`                      | int    | 0~255                     | 05                |
| `tft_brightness`                 | int    | 5~100（下限 5）           | **04 已生效**     |
| `device_volume`                  | int    | 0~100                     | 06                |
| `audio_enable`                   | int    | 0/1                       | 06                |
| `power_mode`                     | int    | 0~255                     | 06                |
| `voice_enable`                   | int    | 0/1                       | 06                |
| `voice_trigger_key`              | int    | 键 ID                     | 06                |
| `voice_max_record_ms`            | int    | 毫秒                      | 06                |
| `voice_auto_enter`               | int    | 0/1                       | 06                |
| `voice_dev_pid`                  | int    | 百度 PID                  | 06                |
| `voice_cuid`                     | string | ≤32 字节                  | 06                |
| `voice_baidu_api_key`            | string | ≤64 字节                  | 06                |
| `voice_baidu_secret_key`         | string | ≤64 字节                  | 06                |
| `pc_status_mask`                 | int    | 位掩码（无符号）          | 05                |
| `active_keymap_profile`          | int    | 0~7                       | **04 已生效**     |
| `active_profile_name`            | string | LVGL 符号名               | 04（只读）        |
| `active_profile_has_custom_icon` | bool   | SPIFFS 是否有 icon{N}.png | 04（只读）        |

> "生效阶段"指固件侧功能接入时点；字段本身已全部可读写并持久化。

---

## 4. CMD_CONFIG_SET（0x08）— 写入设置

请求：

```json
{ "cmd": 8, "seq": 2, "data": { "config": { "tft_brightness": 50 } } }
```

- `data.config` 内为**增量字段**，只发需要修改的字段；未知字段被忽略并返回日志（不影响其它字段）
- 固件原子写入内存 + 逐键持久化到 SPIFFS `/config.ini`（重启后保留）

响应与主动上报：

1. 立即回 `{"cmd":0x88,"seq":2,"status":0}`
2. 若有字段实际变更，固件随后**主动推送**一帧 `{"cmd":0x87,"seq":0,"status":0,"data":{...全量快照}}`，App 应以其刷新 UI，无需再发 GET
3. 无实际变更（值相同）时不推送快照

### 4.1 校验规则

| 字段                    | 规则                                                           |
| ----------------------- | -------------------------------------------------------------- |
| `tft_brightness`        | 钳位到 5~100                                                   |
| `work_mode`             | 合法域 0~2，越界忽略该字段                                     |
| `active_keymap_profile` | 合法域 0~7，越界忽略该字段                                     |
| 字符串字段              | 超长截断（ssid 32 / password 64 / cuid 32 / 两个 key 64 字节） |

### 4.2 副作用（App 需感知）

| 变更字段                  | 固件行为                                                  |
| ------------------------- | --------------------------------------------------------- |
| `tft_brightness`          | 屏幕背光**立即**变化（SETTING_UPDATE → PWM）              |
| `work_mode`               | 重建键盘实例（阶段 04 仅 USB，BLE/2.4G 回退 USB 并 WARN） |
| `active_keymap_profile`   | 立即重载 keymap{N}.ini 键映射                             |
| `wifi_*` / `connect_host` | 仅持久化 + 日志；WiFi 连接阶段 06 接入                    |
| 其它字段                  | 持久化，功能随对应阶段生效                                |

---

## 5. CMD_HEARTBEAT（0x0a）— 心跳

```json
→ {"cmd": 10, "seq": 3}
← {"cmd": 138, "seq": 3, "status": 0, "data": {"timestamp": 123456, "device": "EKeys"}}
```

- 固件侧仅**应答**，不主动发心跳；App 自行定时（建议 1~3s）发送以判断在线
- `data.timestamp` 为固件 `millis()`（开机毫秒数），可用于粗略对时/重启检测（数值回退=设备重启）

---

## 6. App 需实现的功能清单

### 阶段 04（当前，可立即开发）

- [ ] **串口管理**：枚举 ESP32-S3 USB CDC 端口（VID 0x303A）、开关连接、断线重连
- [ ] **JSON 行编解码**：发送加 `\n`；接收按行分割，跳过非 `{` 行（固件日志）
- [ ] **seq 配对与超时**：建议单请求 500ms~1s 超时，超时重试 1 次
- [ ] **心跳保活**：定时发 0x0a，连续 N 次无响应判定离线
- [ ] **设置面板**：读取 0x07 全量快照渲染 UI；修改经 0x08 增量下发；处理 0x87 主动推送（seq=0，非请求响应）
- [ ] **错误提示**：`status=1` 时展示 `error` 字段

### 阶段 05（预留）

- [ ] PC 状态上报（0x0d：CapsLock/网络/CPU/内存/温度位掩码）
- [ ] 音乐状态/控制（0x0e/0x0f：标题、艺人、进度、播放/暂停/切歌）
- [ ] 键映射编辑（0x05/0x06）与 Profile 图标上传（0x11）

### 阶段 06（预留）

- [ ] UDP 自动发现（广播 `FUNKEYBOARD_DISCOVER` 到 30001，监听 `FUNKEYBOARD_HERE`，超时 800ms 回退 `192.168.31.1`）
- [ ] TCP 通道（30000 端口，复用同一 JSON 行协议；Serial/TCP 双通道任一在线即生效）

---

## 7. 调试示例（Python）

```python
import serial, json

ser = serial.Serial("COM5", 115200, timeout=1)

def send(cmd, seq, data=None):
    frame = {"cmd": cmd, "seq": seq}
    if data is not None:
        frame["data"] = data
    ser.write((json.dumps(frame) + "\n").encode())

def read_frame():
    for _ in range(10):                 # 跳过日志行
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("{"):
            return json.loads(line)
    return None

send(7, 1)                               # CMD_CONFIG_GET
print(read_frame())

send(8, 2, {"config": {"tft_brightness": 50}})   # CMD_CONFIG_SET
print(read_frame())                      # 0x88 成功响应
print(read_frame())                      # 0x87 全量快照推送
```

> 也可用 `python -m serial.tools.miniterm` 手动输入 JSON 行验证。

---

## 8. 兼容性注意事项

1. **日志混流**：固件升级可能调整日志内容/频率，唯一稳定约定是"协议行以 `{` 开头"。
2. **快照推送**：`cmd=0x87` 且 `seq=0` 的帧是主动推送，不是任何请求的响应；App 的响应分发逻辑需按 `seq` 区分（seq=0 的 0x87 归推送通道）。
3. **增量语义**：SET 只携带变更字段；值未变化的字段固件不会写文件、也不会触发快照推送。
4. **未来兼容**：后续阶段会新增 `data` 内字段与命令 ID，App 解析应忽略未知字段/命令，保持前向兼容。
