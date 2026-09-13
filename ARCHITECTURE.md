# EKeys 固件项目结构设计文档

> 适用硬件：ESP32-S3-WROOM-1（N16R8，16 MB Flash / 8 MB OPI PSRAM）
> 框架：Arduino + PlatformIO 6.x
> 本文描述当前代码目录结构、模块划分与依赖关系。功能描述见 [`FEATURE_DOC.md`](./FEATURE_DOC.md)，引脚定义见 [`PINOUT.md`](./PINOUT.md)。

---

## 1. 设计目标与原则

1. **功能闭环**：以 `FEATURE_DOC.md` 中的 18 节为功能目标，使每个文档条目都能在目录树中找到对应实现位置（占位也要显式存在）。
2. **任务边界清晰**：固件运行在两个 FreeRTOS 任务（`MainTask` / `DisplayTask`）之上，目录结构按"主任务侧 / 显示任务侧 / 共享服务"三个域划分，避免跨域直接依赖。
3. **可替换的实现**：USB / BLE 键盘、WiFi/BLE 通信、I2C 外挂模块等可能切换实现的部分统一抽象成接口（`IKeyboard` 等），便于后续替换。
4. **SquareLine Studio 兼容**：`src/ui/` 目录保持与 SquareLine Studio 工程输出兼容，避免手工改动 UI 文件后被生成器覆盖。
5. **构建产物可追溯**：分区表 / LVGL 配置 / SPIFFS 资源单独成目录，烧录脚本可重复执行。
6. **单一职责**：每个 `.cpp/.h` 仅负责 FEATURE_DOC 中的某一节或某一子模块，禁止出现"杂项"文件。

---

## 2. 顶层目录结构

```
EKeys/
├── platformio.ini              # 构建配置（esp32-s3-wroom-1-n16r8 环境）
├── partitions-16MB.csv         # 16MB Flash 分区表
├── FEATURE_DOC.md              # 功能需求文档（输入）
├── PINOUT.md                   # 硬件引脚文档
├── ARCHITECTURE.md             # 本文件：项目结构设计
│
├── include/                    # 全局包含路径（lv_conf.h 等覆盖配置）
│   ├── lv_conf.h
│   └── LvglMemPool.h
│
├── lib/                        # PlatformIO 本地库（不放入 src 的第三方/复用代码）
│   └── GFX Library for Arduino/   # 已固定 1.6.0，DisplayDriver 唯一依赖
│
├── data/                       # SPIFFS 映像源文件（platformio uploadfs 上传）
│   ├── config.ini              # 主配置
│   ├── keymap1.ini ~ keymap8.ini
│   └── audio_pad.ini           # 音效板绑定表
│
├── src/                        # 主源码（按子系统划分，见第 3 节）
│
├── boards/                     # PlatformIO 自定义 board 定义
│
└── test/                       # 单元测试占位
```

---

## 3. `src/` 子目录详细设计

`src/` 内部按"职责域 + 子系统"二维划分，避免平铺。每个目录下文件名尽量与 `FEATURE_DOC.md` 中出现的类一一对应。

### 3.1 入口与全局上下文

```
src/
├── main.cpp                    # Arduino setup/loop；只做初始化编排 + MainTask::loop()
├── app/
│   ├── AppContext.h/.cpp       # 全局单例，持有 Configuration / IKeyboard / WiFi 等子系统指针
│   └── BootStage.*             # 启动阶段日志（已并入 main.cpp）
```

- `main.cpp` **禁止**放业务逻辑，只负责：
  1. 启动画面（`LvglPort::instance().showSplash()`）；
  2. SPIFFS 挂载（失败 LOG_ERROR 死循环）；
  3. `AppContext::instance().init()`（Configuration 加载 / MainTask.begin / DisplayTask.begin / AudioPad 加载 / 阶段 06 服务初始化）。
- 业务编排集中在 `MainTask::begin()` 与 `DisplayTask::begin()`。

### 3.2 任务层（FreeRTOS Tasks）

```
src/tasks/
├── MainTask.h/.cpp             # Core 1, 栈 12288：按键扫描 / WiFi / BLE / I2C / 协议 / 语音 / 音效板 service
└── DisplayTask.h/.cpp          # Core 0：LVGL tick / RGB LED / 频谱动画 / 队列消费 / UI 导航
```

- 任务间通信：**只允许**通过 `message_types.h::DisplayMessage` 队列或 `EventBus` 传递，禁止共享全局可变状态。
- `MainTask` 拥有所有"动作源"；`DisplayTask` 拥有所有"渲染目标"。
- 跨核互斥：`AudioPad::bindings_/playing_key_/playing_name_/请求标志` 用 portMUX 临界区保护。

### 3.3 输入层（FEATURE_DOC §2）

```
src/input/
├── MatrixScanner.h/.cpp        # 3×4 矩阵扫描、消抖状态机
├── RotaryEncoder.h/.cpp        # 板载 EC11（PCNT + OneButton），仅 UI 导航
└── event_types.h               # 统一输入事件类型（按键 / 旋钮 / 滑动 / 模块）
```

- 矩阵键 1~11 物理按下不直接进入键映射（除 KEYMAPPED 屏被截胡为焦点跳转）。
- 旋钮全部走 `ACTION_INPUT`，与 `LV_KEY_*` 数值一一对应（详见 `.trae/rules/rules.md`）。

### 3.4 键映射与 HID 输出（FEATURE_DOC §3、§4）

```
src/keymap/
├── KeyMapping.h                # POD：function_key / normal_key[] / macros_key[] / text / FUN 组合层
├── KeyNameTable.h/.cpp         # 字符 → HID keycode 表（含 0xNN 解析）
└── KeyEventDispatcher.h/.cpp   # KeyMapping → IKeyboard（press/release/sequence）；ASR / RGB 联动
```

- `KeyEventDispatcher::handleKeyEvent` 对应 `FEATURE_DOC 3.4`。
- Profile 切换涉及 `Configuration::switchActiveProfile()` → `KeyResolver::reload()`。

### 3.5 键盘输出后端（FEATURE_DOC §4）

```
src/output/
├── IKeyboard.h                 # 抽象接口：begin / press / release / releaseAll / isConnected / send
├── USBKeyboardImpl.h/.cpp      # TinyUSB HID + Consumer Control
├── BLEKeyboardImpl.h/.cpp      # BLE HID（释放经典蓝牙内存）
└── KeyboardFactory.h/.cpp      # 根据 Configuration::WORK_MODE 创建对应实例
```

- `KeyboardFactory::create()` 是 `AppContext::applyWorkMode()` 的唯一入口。
- `WIRELESS_2_4G_KEYBOARD_MODE`：按用户决定暂不实现（2026-08-31），选择该模式打印 warning 并安全回退 USB。

### 3.6 协议层（FEATURE_DOC §5）

```
src/protocol/
├── SerialProtocol.h/.cpp       # 双通道（USB CDC + TCP）+ 心跳 + JSON 行解析
├── CommandRegistry.h/.cpp      # std::array<Entry,64> 零堆分配注册表
├── TcpChannel.h/.cpp           # 桌面 App 控制通道（端口 30000）
├── registration.cpp            # registerAllCommandHandlers()（按 Phase 分组）
└── commands/
    ├── cmd_firmware.cpp        # 0x01 / 0x02 / 0x0b / 0x14
    ├── cmd_device_info.cpp     # 0x03 / 0x04
    ├── cmd_keymap.cpp          # 0x05 / 0x06
    ├── cmd_config.cpp          # 0x07 / 0x08
    ├── cmd_pc_status.cpp       # 0x0d
    ├── cmd_music.cpp           # 0x0e / 0x0f
    ├── cmd_profile.cpp         # 0x10 / 0x11 / 0x15
    ├── cmd_time.cpp            # 0x13
    └── cmd_audio.cpp           # 0x16 / 0x17（音效板）
```

- 每个 `cmd_*.cpp` 仅 `registerCmd(命令ID, handler, name)`，handler 函数保持 <200 行。
- 命令清单与字段定义见 [`docs/desktop-app-protocol.md`](./docs/desktop-app-protocol.md)。

### 3.7 配置与服务层（FEATURE_DOC §3.2、§3.3、§6）

```
src/config/
├── Configuration.h/.cpp        # 顶层单例：load() / SaveKeyMapping() / SaveSetting()
├── ConfigurationSchema.h       # CMD_CONFIG_SET 字段定义（与 cmd_config.cpp 共享）
├── DeviceSettings.h            # POD：所有可设置项（WiFi / 屏幕 / RGB / 音量 / 语音 ...）
└── parseConfigSetCommand.h/.cpp# FEATURE_DOC §6 的字段原子写入逻辑
```

```
src/services/
├── ConfigStore.h/.cpp          # SPIFFS + SimpleIni 封装
├── KeymapRepository.h/.cpp     # keymap{N}.ini 读写
└── EventBus.h/.cpp             # 同步发布订阅（FEATURE_DOC §1.2）
```

- `Configuration` 内部互斥量 `mutex_`（FreeRTOS semaphore）在 `ConfigStore` / `KeymapRepository` 间共享。
- `parseConfigSetCommand` 是 FEATURE_DOC §6 的实现入口。

### 3.8 网络层（FEATURE_DOC §7）

```
src/network/
├── WiFiManager.h/.cpp          # ConnectToWiFi / 重连策略 / stopWiFiReconnect
├── NtpSync.h/.cpp              # pool.ntp.org, GMT+8
├── TcpChannel.h/.cpp           # 桌面 App 控制通道（端口 30000）
├── DiscoveryService.h/.cpp     # UDP 30001 自动发现
└── NetDiagnostics.h/.cpp       # RSSI / IP 收集，用于 HA 状态聚合
```

### 3.9 显示与 UI（FEATURE_DOC §8）

#### 3.9.1 驱动 / 端口层（自研代码，放在 `src/display/` 与 `src/ui/`）

```
src/display/
├── DisplayDriver.h/.cpp        # 包装 Arduino_GFX（NV3007）初始化、fill、bitmap draw
├── LvglPort.h/.cpp             # lv_init / lv_disp_draw_buf / flush_cb / tick / splash
├── LvglMemPool.c               # LVGL 内存池包装
└── Backlight.h/.cpp            # LCD_BL 控制（LEDC PWM）
```

```
src/ui/
├── ui.h/.c                     # SquareLine Studio 生成（勿手工改）
├── ui_events.h                 # SquareLine 生成
├── ui_helpers.h/.c             # 自定义辅助：StatusBar 更新 / 设置屏反向同步
├── ui_StatusBar.h/.c           # 状态条实现（FEATURE_DOC §8.2）
├── ui_settings_types.h         # 设置屏快照结构定义
└── 各屏 ui_<Screen>.h/.c       # 主屏/键映射/音乐/音效/PC状态/HA/设置（含 _Secondary 详情页）
```

```
src/hardware/
├── PinMap.h                    # 集中所有 IO 定义；LCD 引脚按 PINOUT.md §2.6
└── BatteryMonitor.h/.cpp       # GPIO4 电压采样（可选）
```

#### 3.9.2 引脚约定

按项目规则（`.trae/rules/rules.md`：引脚以 `PINOUT.md` 为准）。

LCD 引脚（按当前 `PinMap.h` / `PINOUT.md §2.6`）：

| 信号      | `PinMap.h` 常量                | `PINOUT.md` 引脚 | 说明                                                    |
| --------- | ------------------------------ | ---------------- | ------------------------------------------------------- |
| `LCD_BL`  | `kPinLcdBacklight = 1`         | IO1              | 背光 PWM 控制                                           |
| `LCD_CS`  | `kPinLcdCs        = 2`         | IO2              | SPI 片选                                                |
| `LCD_DC`  | `kPinLcdDc        = 42`        | IO42             | 数据/命令选择（DC/RS）                                  |
| `LCD_SDA` | `kPinLcdMosi      = 40`        | IO40             | SPI 数据（MOSI）                                        |
| `LCD_SCL` | `kPinLcdSclk      = 41`        | IO41             | SPI 时钟（CLK）                                         |
| `LCD_RST` | `kPinLcdRst = GFX_NOT_DEFINED` | —                | 硬件直接拉低，未分配引脚；Arduino_GFX 在 `begin()` 时跳过 RST 操作 |

> 启动画面：`main.cpp` 在面板就绪后立即 `LvglPort::instance().showSplash()`，覆盖 SPIFFS 挂载 / 配置加载 / `ui_init` 建屏期间的黑屏；主 UI 建好后由 `DisplayTask::run()` 调 `clearSplash()` 销毁回收 LVGL 池内存。

### 3.10 RGB 灯光（FEATURE_DOC §9）

```
src/rgb/
├── RGBLightControl.h/.cpp      # 显示任务驱动 LED 动画循环
├── RGBDriver.h/.cpp            # WS2812B 驱动（RMT / SPI），GRB 顺序；LED_PWR_CTRL 低电平有效
└── ClickHighlight.h/.cpp       # RGB_CLICK_MODE 三种点击高亮
```

- 11 颗灯珠索引与 11 个应用键 ID 对齐。

### 3.11 音频与语音（FEATURE_DOC §10、§11）

```
src/audio/
├── Speaker.h/.cpp              # MAX98357 数字功放（IO10/IO9/IO11），PlayRemoteAudio / PlayLocalAudio
├── Mic.h/.cpp                  # ICS43434 数字 MEMS 麦克风（IO13/IO12/IO14）录音
├── AudioAnalyzer.h/.cpp        # FFT_SIZE=512 / BANDS=16
└── AudioPad.h/.cpp             # 11 键本地音频绑定表（持久化 /audio_pad.ini）
```

```
src/voice/
├── VoiceRecognizer.h/.cpp      # 腾讯云一句话识别 ASR REST，mbedtls HMAC-SHA256
├── TencentAsrSigner.h/.cpp     # TC3-HMAC-SHA256 签名器
└── VoiceConfig.h               # 端点 / 引擎 / 区域常量
```

- `voice_trigger_key` 默认 11，按 `KEY_FUNCTION_ASR` 命中；与 `KeyEventDispatcher` 联动。
- ASR 后端已从百度短语音 REST 迁移到腾讯云一句话识别（2026-09-08），凭证字段为 `voice_tencent_secret_id` / `voice_tencent_secret_key`，无 OAuth token。

### 3.12 共享消息与工具

```
src/
├── message_types.h             # DisplayMessage 枚举 + 各 Info 结构 + KeymapProfileInfo（含 is_preview）
├── event_types.h               # EventBus 事件类型
├── keymap_types.h              # 键映射相关 POD
├── logging/
│   └── LogManager.h/.cpp       # FEATURE_DOC §15
└── utils/
    └── （仅 keymap_types.h / event_types.h 公共 POD，其它内联在各模块内）
```

### 3.13 OTA 升级（FEATURE_DOC §17）

```
src/upgrade/
├── Upgrade.h/.cpp              # HTTP 流式下载 + MD5 校验（校验失败 abort 不覆盖固件）
└── OtaPlan.h                   # 后续扩展：双 OTA 分区切换策略
```

- `CMD_FIRMWARE_INFO (0x0b)` 请求携带 `data.url` + `data.checksum`（固件 MD5，32 位十六进制，必填）；回 0x8b 成功响应后 `performOta()` 在 MainTask 上下文阻塞执行，成功自动重启进入新固件。
- `CMD_FIRMWARE_DOWNLOAD (0x14)` 复位进 USB 下载模式（烧录）。

---

## 4. 功能划分对照表（FEATURE_DOC ↔ 代码位置）

| FEATURE_DOC 章节  | 关键类 / 文件                                                                                                                                                                                                                          |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| §1.2 软件架构     | `src/tasks/MainTask.cpp`、`src/tasks/DisplayTask.cpp`、`src/message_types.h`、`src/services/EventBus.h`                                                                                                                                |
| §2.1 矩阵扫描     | `src/input/MatrixScanner.cpp`                                                                                                                                                                                                          |
| §2.2 旋钮         | `src/input/RotaryEncoder.cpp`                                                                                                                                                                                                          |
| §2.3 外挂模块     | `src/hardware/PinMap.h`（I2C 引脚） + I2C 扫描逻辑（`src/input/` 或 `src/app/`）                                                                                                                                                       |
| §2.4 特殊输入     | `src/config/Configuration.h` (`CONFIG_SPECIAL_INPUT_NUM`)                                                                                                                                                                              |
| §3 键映射         | `src/keymap/`                                                                                                                                                                                                                          |
| §3.2 配置持久化   | `src/services/ConfigStore.cpp`、`src/services/KeymapRepository.cpp`                                                                                                                                                                    |
| §3.3 Profile 切换 | `src/config/Configuration.cpp`（`switchActiveProfile` / `getProfileConfigPath` / `getProfileIconPath` / `getProfileDisplayName`）                                                                                                       |
| §3.4 HID 触发     | `src/keymap/KeyEventDispatcher.cpp`                                                                                                                                                                                                    |
| §4 键盘输出       | `src/output/`                                                                                                                                                                                                                          |
| §5 私有协议       | `src/protocol/`                                                                                                                                                                                                                        |
| §6 CMD_CONFIG_SET | `src/config/parseConfigSetCommand.cpp`                                                                                                                                                                                                 |
| §7 网络           | `src/network/`                                                                                                                                                                                                                         |
| §8 显示与 UI      | `src/display/{DisplayDriver,LvglPort,Backlight}.cpp`、`src/ui/`、`src/hardware/PinMap.h`（LCD 引脚）；详见 §3.9。                                                                                                                       |
| §9 RGB            | `src/rgb/`                                                                                                                                                                                                                             |
| §10 音频          | `src/audio/Speaker.cpp`、`src/audio/Mic.cpp`、`src/audio/AudioAnalyzer.cpp`、`src/audio/AudioPad.cpp`                                                                                                                                   |
| §11 语音          | `src/voice/VoiceRecognizer.cpp` + `src/voice/TencentAsrSigner.cpp`                                                                                                                                                                     |
| §12 PC 状态       | `src/ui/ui_PcStatusScreen.c` + `src/protocol/commands/cmd_pc_status.cpp`                                                                                                                                                               |
| §13 音乐控制      | `src/protocol/commands/cmd_music.cpp` + `src/ui/ui_MusicScreen.c`                                                                                                                                                                      |
| §14 HA 状态       | `src/services/EventBus.cpp` + `src/ui/ui_StatusBar.c` + `src/network/NetDiagnostics.cpp`                                                                                                                                                |
| §15 日志          | `src/logging/LogManager.cpp`                                                                                                                                                                                                           |
| §16 电源          | 电源使能：`src/hardware/PinMap.h` + `src/app/AppContext.cpp`（5V 使能按 power_mode 启停）                                                                                                                                              |
| §17 待办 / 已完成 | OTA：`src/upgrade/Upgrade.cpp`（0x0b）；Profile 名：`src/protocol/commands/cmd_profile.cpp`（0x15）；音效板：`src/audio/AudioPad.cpp` + `src/protocol/commands/cmd_audio.cpp`（0x16/0x17）；时间注入：`src/protocol/commands/cmd_time.cpp`（0x13）；复位下载：`src/protocol/commands/cmd_firmware.cpp`（0x14） |

---

## 5. 任务、队列与同步策略

| 资源                       | 拥有方      | 消费者                 | 同步方式                            |
| -------------------------- | ----------- | ---------------------- | ----------------------------------- |
| `DisplayMessage` 队列      | MainTask    | DisplayTask            | FreeRTOS `xQueue`（长度 10）        |
| `EventBus`                 | 共享        | 各订阅者               | 内部临界区 + 调用链同步             |
| `Configuration::mutex_`    | 共享        | MainTask / ConfigStore | FreeRTOS semaphore                  |
| `ui_settings_lock`         | LVGL 端     | MainTask               | 临界区 + `ui_settings_snapshot_t`   |
| `keyboard_` 指针替换       | MainTask    | MainTask 内部          | 仅在 `setWorkMode` 内替换，外部只读 |
| `RGB` 状态                 | DisplayTask | DisplayTask 内部       | 局部变量，外部通过队列投递事件      |
| `AudioPad` 绑定表 / 请求位 | MainTask    | 跨 MainTask / Display | portMUX 临界区；SPIFFS / Speaker 调用在临界区外 |
| `g_active_screen_tag`      | DisplayTask | MainTask               | 8 位读写天然原子无锁（`ui_screen_tag_t`） |

> 规则：除已声明的接口（`Configuration` 的 API、`EventBus` 的订阅、`DisplayMessage` 队列）外，**禁止**跨任务直接调用对方模块的方法。

---

## 6. 构建与烧录相关文件

- `platformio.ini`：`build_flags` 区域追加 LVGL 编译选项（`-DLV_CONF_INCLUDE_SIMPLE` 等）以及 `ARDUINO_USB_MODE=1` 的开关（FEATURE_DOC §1.1）。
- `include/lv_conf.h`：覆盖 LVGL 默认配置（颜色深度、主题、字体子集）。`build_flags` 同步追加 `-DLV_CONF_INCLUDE_SIMPLE` 与 `-I include`。
- `partitions-16MB.csv`：当前 16 MB 分区已划分；SPIFFS 占用 3.875 MB，可容纳 8 套 keymap + 音频 + Profile 图标 + 音效板文件（≤2 MB/文件）。
- `data/`：打包 SPIFFS 前的预处理（如 PNG → base64 编码、keymap 校验）。

### 屏幕驱动构建要点

- `lib/GFX Library for Arduino/`：继续保留 1.6.0，作为 `DisplayDriver` 唯一依赖（`Arduino_ESP32SPI` / `Arduino_NV3007` / `nv3007_279_init_operations`）。
- LVGL 缓冲区约 17 KB（参见 `src/display/LvglPort.cpp::kLvglBufferLines`）。
- `LV_COLOR_16_SWAP`：默认 0；通过 `#if LV_COLOR_16_SWAP != 0` 分支，保证与 LVGL 不同 swap 配置兼容。
- `lib_deps` 暂不增加 `lvgl/lvgl@8.3.11` 之外的任何屏幕相关依赖。

---

## 7. 命名规范

1. **类名**：与 `FEATURE_DOC.md` 中出现的英文类名一致（`MatrixScanner`、`RotaryEncoder`、`CommandRegistry`、`SerialProtocol`、`Configuration`、`KeymapRepository`、`EventBus`、`RGBLightControl`、`VoiceRecognizer`、`LogManager`、`USBKeyboardImpl`、`BLEKeyboardImpl`、`IKeyboard`、`Speaker`、`Mic`、`AudioAnalyzer`、`AudioPad`、`TencentAsrSigner`）。
2. **文件名**：`类名 + 扩展名`，全部 PascalCase（`KeyEventDispatcher.cpp`）。
3. **命名空间**：仅 `protocol::commands` 与 `protocol::registration` 使用命名空间，其它全部平铺到 `src/` 的 `ekeys::*` 命名空间下（保持一致）。
4. **常量**：`kCamelCase`（`kWifiRetryIntervalMs`），枚举值 `ALL_CAPS`。
5. **POD/类型**：`event_types.h` / `keymap_types.h` 仅放 `struct`，不放实现。

---

## 8. 演进路线（已完成）

阶段任务计划（01 最小 HID → 02 显示迁移 → 03 配置持久化 → 04 协议同步 → 05 UI 屏扩展 → 06 网络/语音/RGB/音频 → 07 占位项补齐 → 08 腾讯 ASR 迁移 → 09 音效板/Profile 名/时间注入/复位下载）均已完成。后续演进以功能增量为单位，按 `FEATURE_DOC.md` 中标注的"未接通"项推进（如 `CMD_KEY_EVENT` 上报、`CMD_HA_STATUS` 上报、UI 音乐控制到协议发送链路）。
