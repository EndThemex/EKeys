# EKeys 固件项目结构设计文档

> 适用硬件：ESP32-S3-WROOM-1（N16R8，16 MB Flash / 8 MB OPI PSRAM）
> 框架：Arduino + PlatformIO 6.x
> 本文档用于规划 `EKeys` 固件从最小 demo 演进为完整功能键盘主控时的代码目录、模块划分与依赖关系。
> 名称沿用 [`FEATURE_DOC.md`](./FEATURE_DOC.md) 中已出现的关键类，保持术语一致。

---

## 1. 设计目标与原则

1. **功能闭环**：以 `FEATURE_DOC.md` 中的 18 节为功能目标，使每个文档条目都能在目录树中找到对应实现位置（占位也要显式存在）。
2. **任务边界清晰**：固件运行在两个 FreeRTOS 任务（`MainTask` / `DisplayTask`）之上，目录结构需按"主任务侧 / 显示任务侧 / 共享服务"三个域划分，避免跨域直接依赖。
3. **可替换的实现**：USB / BLE / 2.4G 键盘、WiFi/BLE 通信、I2C 外挂模块等可能切换实现的部分统一抽象成接口（`IKeyboard`、`ITransport` 等），便于后续替换。
4. **SquareLine Studio 兼容**：`ui/` 目录保持与 SquareLine Studio 工程输出兼容，避免手工改动 UI 文件后被生成器覆盖。
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
├── include/                    # 全局包含路径（lv_conf_local.h 等覆盖配置）
│   └── lv_conf_local.h
│
├── lib/                        # PlatformIO 本地库（不放入 src 的第三方/复用代码）
│   └── README                  # 本地库占位说明
│
├── data/                       # SPIFFS 映像源文件（platformio uploadfs 上传）
│   ├── config.ini              # 主配置
│   ├── keymap1.ini ~ keymap8.ini
│   ├── profile_icons/          # 各 Profile 自定义 48×48 PNG 图标
│   ├── audio/                  # 本地 WAV（coin2.wav / dino_jump.wav 等）
│   └── fonts/                  # 字体资源（如需 SPIFFS 化）
│
├── src/                        # 主源码（按子系统划分，见第 3 节）
│
├── tools/                      # 辅助脚本（图标编码、SPIFFS 打包等）
│   ├── profile_icon_encoder.py
│   └── spiffs_pack.py
│
└── test/                       # 单元测试（基于 PlatformIO native test）
    ├── test_keymap_parser.cpp
    ├── test_protocol.cpp
    └── ...
```

> **关于现有 `lib/GFX Library for Arduino/`**：当前以本地库形式存在。可保留（已固定 1.6.0 版本），后续若有需要可迁移到 `platformio.ini` 的 `lib_deps` 统一管理。

---

## 3. `src/` 子目录详细设计

`src/` 内部按"职责域 + 子系统"二维划分，避免平铺。每个目录下文件名尽量与 `FEATURE_DOC.md` 中出现的类一一对应。

### 3.1 入口与全局上下文

```
src/
├── main.cpp                    # Arduino setup/loop；只做初始化编排 + 任务创建
├── app/
│   ├── AppContext.h/.cpp       # 全局单例，持有 Configuration / IKeyboard / WiFi 等子系统指针
│   ├── BootStage.h/.cpp        # 启动阶段日志（SPIFFS → Config → WiFi → Display）
│   └── Version.h               # 固件版本号（与 cmd_firmware.cpp 复用）
```

- `main.cpp` **禁止**放业务逻辑，只负责：
  1. 创建 `AppContext`；
  2. 调用 `BootStage::runAll()`；
  3. 在 `loop()` 中执行 LVGL tick。
- 业务编排集中在 `MainTask::begin()` 与 `DisplayTask::begin()`。

### 3.2 任务层（FreeRTOS Tasks）

```
src/tasks/
├── MainTask.h/.cpp             # Core 1, 栈 12288：按键扫描 / WiFi / BLE / I2C / 协议 / 语音
├── DisplayTask.h/.cpp          # Core 0：LVGL tick / RGB LED / 频谱动画 / 队列消费
└── TaskMonitor.h/.cpp          # 栈高水位、运行状态观测（可选）
```

- 任务间通信：**只允许**通过 `message_types.h::DisplayMessage` 队列或 `EventBus` 传递，禁止共享全局可变状态。
- `MainTask` 拥有所有"动作源"；`DisplayTask` 拥有所有"渲染目标"。

### 3.3 输入层（FEATURE_DOC §2）

```
src/input/
├── MatrixScanner.h/.cpp        # 3×4 矩阵扫描、消抖状态机
├── RotaryEncoder.h/.cpp        # 板载 EC11（PCNT + OneButton）
├── I2CMasterController.h/.cpp  # I2C 主控，ModA(0x06) / ModB(0x08)
├── InputEvent.h                # 统一输入事件类型（按键 / 旋钮 / 滑动 / 模块）
└── InputRouter.h/.cpp          # 把原始输入路由到 keymap 或 LVGL（旋钮仅导航）
```

- `InputRouter` 决定一个 `InputEvent` 是进入 `KeyEventDispatcher`（走键映射）还是转 `DisplayMessage::ACTION_INPUT`（屏幕导航）。
- 对应文档：`2.1 矩阵扫描`、`2.2 旋钮`、`2.3 外挂模块`、`2.4 特殊输入`。

### 3.4 键映射与 HID 输出（FEATURE_DOC §3、§4）

```
src/keymap/
├── KeyMapping.h                # POD：function_key / normal_key[] / macros_key[]
├── KeyNameTable.h/.cpp         # 字符 → HID keycode 表（含 0xNN 解析）
├── KeymapProfile.h             # 单个 Profile 的元数据（索引、显示名、图标路径）
├── KeyEventDispatcher.h/.cpp   # KeyMapping → IKeyboard（press/release/sequence）
└── KeyResolver.h/.cpp          # 应用键 ID → KeyMapping 查表
```

- `KeyEventDispatcher::handleKeyEvent` 对应 `FEATURE_DOC 3.4`。
- Profile 切换涉及 `Configuration::switchActiveProfile()` → `KeyResolver::reload()`。

### 3.5 键盘输出后端（FEATURE_DOC §4）

```
src/output/
├── IKeyboard.h                 # 抽象接口：begin / press / release / releaseAll / isConnected / send
├── USBKeyboardImpl.h/.cpp      # TinyUSB HID + Consumer Control
├── BLEKeyboardImpl.h/.cpp      # BLE HID（释放经典蓝牙内存）
├── Wireless24GKeyboardImpl.h/.cpp  # 占位实现（FEATURE_DOC §4.1 占位项）
├── KeyboardFactory.h/.cpp      # 根据 Configuration::WORK_MODE 创建对应实例
└── ConsumerControlCodes.h      # 媒体键码常量
```

- `KeyboardFactory::create()` 是 `MainTask::setWorkMode()` 的唯一入口。
- 当前 `WIRELESS_2_4G_KEYBOARD_MODE` 仅打印 warning；保留文件以体现"接口存在，实现待补"。

### 3.6 协议层（FEATURE_DOC §5）

```
src/protocol/
├── SerialProtocol.h/.cpp       # 双通道（USB CDC + TCP）+ 心跳 + JSON 行解析
├── CommandRegistry.h/.cpp      # std::array<Entry,64> 零堆分配注册表
├── Transport.h                 # ITransport：Serial / TCP（自动发现）
├── TcpChannel.h/.cpp           # 桌面 App 控制通道（端口 30000）
├── registration.cpp            # registerAllCommandHandlers()（按 Phase 分组）
└── commands/
    ├── cmd_firmware.cpp        # 0x01 / 0x0b
    ├── cmd_device_info.cpp     # 0x03
    ├── cmd_keymap.cpp          # 0x05 / 0x06
    ├── cmd_config.cpp          # 0x07 / 0x08
    ├── cmd_pc_status.cpp       # 0x0d
    ├── cmd_music.cpp           # 0x0e / 0x0f
    └── cmd_profile.cpp         # 0x10 / 0x11
```

- 每个 `cmd_*.cpp` 仅 `registerCmd(命令ID, handler, name)`，handler 函数保持 `<200` 行。
- 对应文档：§5.3 表格中所有已注册命令；§5.4 注册机制。
- 待补：`CMD_CONF_VERSION_SET` (0x02) 与 `CMD_DEVICE_INFO_SET` (0x04) 已在 FEATURE_DOC §17 中标注未实现，需要在 `commands/` 下预留文件或在文档中显式说明。

### 3.7 配置与服务层（FEATURE_DOC §3.2、§3.3、§6）

```
src/config/
├── Configuration.h/.cpp        # 顶层单例：load() / SaveKeyMapping() / SaveSetting()
├── ConfigurationSchema.h       # CMD_CONFIG_SET 字段定义（与 cmd_config.cpp 共享）
├── DeviceSettings.h            # POD：所有可设置项（WiFi / 屏幕 / RGB / 音量 / 语音 ...）
├── PowerMode.h                 # NORMAL / LOW / LIGHT / DEEPSLEEP 枚举
└── parseConfigSetCommand.h/.cpp# FEATURE_DOC §6 的字段原子写入逻辑
```

```
src/services/
├── ConfigStore.h/.cpp          # SPIFFS + SimpleIni 封装
├── KeymapRepository.h/.cpp     # keymap{N}.ini 读写
├── ProfileManager.h/.cpp       # 8 套 Profile 的图标 / 显示名 / 切换
├── EventBus.h/.cpp             # 同步发布订阅（FEATURE_DOC §1.2）
└── PngDecoder.h/.cpp           # lodepng 桥接，48×48 RGBA 解码
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

```
src/display/
├── DisplayDriver.h/.cpp        # 包装 Arduino_GFX（NV3007）+ LVGL flush
├── LvglPort.h/.cpp             # lv_init / lv_disp_draw_buf / lv_tick
├── ThemePalette.h             # 主题色板（tft_theme）
└── Backlight.h/.cpp           # LCD_BL PWM 控制
```

```
src/ui/                          # SquareLine Studio 输出 + 自定义辅助
├── ui.h/.cpp                    # SquareLine 生成（勿手工改）
├── ui_events.h/.cpp             # SquareLine 生成
├── ui_helpers.h/.cpp            # 自定义辅助：StatusBar 更新 / 设置屏反向同步
├── ui_StatusBar.h/.cpp          # 状态条实现（FEATURE_DOC §8.2）
└── screens/                      # 若 SquareLine 按屏输出文件，可分子目录
```

- `src/main.cpp` 中现有 NV3007 + LVGL 初始化代码应迁入 `display/` 与 `ui/`；`main.cpp` 只剩"启动 DisplayDriver + 装载 ui"。
- `ui_helpers.cpp` 暴露 `ui_settings_request_apply()` / `ui_settings_request_save()`，对应 FEATURE_DOC §8.4。

### 3.10 RGB 灯光（FEATURE_DOC §9）

```
src/rgb/
├── RGBLightControl.h/.cpp      # 显示任务驱动 LED 动画循环
├── RGBMode.h                   # 枚举（NONE / SINGLE / RAINBOW / ...）
├── RGBDriver.h/.cpp            # WS2812B 驱动（RMT / SPI），GRB 顺序
└── ClickHighlight.h/.cpp       # RGB_CLICK_MODE 三种点击高亮
```

- 11 颗灯珠索引与 11 个应用键 ID 对齐（详见 `KeyMapping` → `RGBDriver::mapKeyIdToLedIndex()`）。

### 3.11 音频与语音（FEATURE_DOC §10、§11）

```
src/audio/
├── Speaker.h/.cpp              # MAX98357 数字功放（IO10/IO9/IO14），PlayRemoteAudio / PlayLocalAudio
├── Mic.h/.cpp                  # ICS43434 数字 MEMS 麦克风（IO10/IO12/IO13/IO11）录音
├── AudioAnalyzer.h/.cpp        # FFT_SIZE=512 / BANDS=16（FEATURE_DOC §10.3 占位）
└── AudioConfig.h               # 采样率、缓冲区等常量
```

```
src/voice/
├── VoiceRecognizer.h/.cpp      # 百度短语音 ASR REST，token 缓存
├── VoiceConfig.h               # dev_pid / cuid / api_key 字段
└── AsrTokenCache.h/.cpp        # access_token 自动获取与失效
```

- `voice_trigger_key` 默认 11，按 `KEY_FUNCTION_ASR` 命中；与 `KeyEventDispatcher` 联动。

### 3.12 共享消息与工具

```
src/
├── message_types.h             # DisplayMessage 枚举 + union（FEATURE_DOC §8.3）
├── event_types.h               # EventBus 事件类型（FEATURE_DOC §1.2）
├── keymap_types.h              # 键映射相关 POD（FEATURE_DOC §1.3 utils）
├── logging/
│   └── LogManager.h/.cpp       # FEATURE_DOC §15
└── utils/
    ├── RingBuffer.h
    ├── JsonLineParser.h/.cpp
    ├── TimeUtil.h/.cpp         # NTP / settimeofday 辅助
    └── BoundedString.h         # 固定容量字符串，避免堆分配
```

### 3.13 电源与硬件抽象（FEATURE_DOC §16）

```
src/hardware/
├── PinMap.h                    # FEATURE_DOC §1.1 / PINOUT.md 汇总
├── Boost5V.h/.cpp              # kBoost5VEnablePin (GPIO3) 控制
├── BatteryAdc.h/.cpp           # GPIO4 电压采样（可选）
└── StrappingPins.h             # 上电电平确认（PINOUT §3.1）
```

### 3.14 OTA 升级（FEATURE_DOC §17 占位）

```
src/upgrade/
├── Upgrade.h/.cpp              # 当前空实现，保留占位接口
└── OtaPlan.h                   # 后续扩展：双 OTA 分区切换策略
```

---

## 4. 功能划分对照表（FEATURE_DOC ↔ 代码位置）

| FEATURE_DOC 章节  | 关键类 / 文件                                                                                                                                                                                          |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| §1.2 软件架构     | `src/tasks/MainTask.cpp`、`src/tasks/DisplayTask.cpp`、`src/message_types.h`、`src/services/EventBus.h`                                                                                                |
| §2.1 矩阵扫描     | `src/input/MatrixScanner.cpp`                                                                                                                                                                          |
| §2.2 旋钮         | `src/input/RotaryEncoder.cpp`                                                                                                                                                                          |
| §2.3 外挂模块     | `src/input/I2CMasterController.cpp`                                                                                                                                                                    |
| §2.4 特殊输入     | `src/config/Configuration.h` (`CONFIG_SPECIAL_INPUT_NUM`)                                                                                                                                              |
| §3 键映射         | `src/keymap/`                                                                                                                                                                                          |
| §3.2 配置持久化   | `src/services/ConfigStore.cpp`、`src/services/KeymapRepository.cpp`                                                                                                                                    |
| §3.3 Profile 切换 | `src/services/ProfileManager.cpp`                                                                                                                                                                      |
| §3.4 HID 触发     | `src/keymap/KeyEventDispatcher.cpp`                                                                                                                                                                    |
| §4 键盘输出       | `src/output/`                                                                                                                                                                                          |
| §5 私有协议       | `src/protocol/`                                                                                                                                                                                        |
| §6 CMD_CONFIG_SET | `src/config/parseConfigSetCommand.cpp`                                                                                                                                                                 |
| §7 网络           | `src/network/`                                                                                                                                                                                         |
| §8 显示与 UI      | `src/display/`、`src/ui/`                                                                                                                                                                              |
| §9 RGB            | `src/rgb/`                                                                                                                                                                                             |
| §10 音频          | `src/audio/`                                                                                                                                                                                           |
| §11 语音          | `src/voice/`                                                                                                                                                                                           |
| §12 PC 状态       | `src/ui/ui_helpers.cpp` + `src/protocol/commands/cmd_pc_status.cpp`                                                                                                                                    |
| §13 音乐控制      | `src/protocol/commands/cmd_music.cpp` + `src/ui/ui_helpers.cpp`                                                                                                                                        |
| §14 HA 状态       | `src/services/EventBus.cpp` + `src/ui/ui_StatusBar.cpp`                                                                                                                                                |
| §15 日志          | `src/logging/LogManager.cpp`                                                                                                                                                                           |
| §16 电源          | `src/hardware/Boost5V.cpp`                                                                                                                                                                             |
| §17 待办          | `src/output/Wireless24GKeyboardImpl.cpp`（占位）、`src/audio/AudioAnalyzer.cpp`（保留）、`src/upgrade/Upgrade.cpp`（占位）、`src/protocol/commands/cmd_firmware.cpp` 中 `CMD_CONF_VERSION_SET`（占位） |

---

## 5. 任务、队列与同步策略

| 资源                    | 拥有方      | 消费者                 | 同步方式                            |
| ----------------------- | ----------- | ---------------------- | ----------------------------------- |
| `DisplayMessage` 队列   | MainTask    | DisplayTask            | FreeRTOS `xQueue`（长度 10）        |
| `EventBus`              | 共享        | 各订阅者               | 内部临界区 + 调用链同步             |
| `Configuration::mutex_` | 共享        | MainTask / ConfigStore | FreeRTOS semaphore                  |
| `ui_settings_lock`      | LVGL 端     | MainTask               | 临界区 + `ui_settings_snapshot_t`   |
| `keyboard_` 指针替换    | MainTask    | MainTask 内部          | 仅在 `setWorkMode` 内替换，外部只读 |
| `RGB` 状态              | DisplayTask | DisplayTask 内部       | 局部变量，外部通过队列投递事件      |

> 规则：除已声明的接口（`Configuration` 的 API、`EventBus` 的订阅、`DisplayMessage` 队列）外，**禁止**跨任务直接调用对方模块的方法。

---

## 6. 构建与烧录相关文件

- `platformio.ini`：`build_flags` 区域追加 LVGL 编译选项（`-DLV_CONF_INCLUDE_SIMPLE` 等）以及 `ARDUINO_USB_MODE=1` 的开关（FEATURE_DOC §1.1）。
- `include/lv_conf_local.h`：覆盖 LVGL 默认配置（颜色深度、主题、字体子集）。
- `partitions-16MB.csv`：当前 16 MB 分区已划分；SPIFFS 占用 3.875 MB，可容纳 8 套 keymap + 音频 + 字体 + Profile 图标。
- `tools/`：打包 SPIFFS 前的预处理（如 PNG → base64 编码、keymap 校验）。

---

## 7. 命名规范

1. **类名**：与 `FEATURE_DOC.md` 中出现的英文类名一致（`MatrixScanner`、`RotaryEncoder`、`I2CMasterController`、`CommandRegistry`、`SerialProtocol`、`Configuration`、`KeymapRepository`、`EventBus`、`RGBLightControl`、`VoiceRecognizer`、`LogManager`、`USBKeyboardImpl`、`BLEKeyboardImpl`、`IKeyboard`、`Speaker`、`Mic`、`AudioAnalyzer`）。
2. **文件名**：`类名 + 扩展名`，全部 PascalCase（`KeyEventDispatcher.cpp`）。
3. **命名空间**：仅 `protocol::commands` 与 `protocol::registration` 使用命名空间，其它全部平铺到 `src/`（保持与现有 demo 一致）。
4. **常量**：`kCamelCase`（`kWifiRetryIntervalMs`），枚举值 `ALL_CAPS`。
5. **POD/类型**：`event_types.h` / `keymap_types.h` 仅放 `struct`，不放实现。

---

## 8. 演进路线（建议顺序）

1. **第 1 步**：实现 `input/MatrixScanner` + `keymap/KeyMapping` + `output/USBKeyboardImpl`，跑通"按键 → USB HID"。
2. **第 2 步**：接入 `display/DisplayDriver` + `ui/` 的最小主屏（基于现有 `main.cpp` 的 NV3007 初始化代码迁移）。
3. **第 3 步**：实现 `services/ConfigStore` + `config/Configuration`，让键映射从 SPIFFS 加载。
4. **第 4 步**：实现 `protocol/SerialProtocol` + `protocol/CommandRegistry` + `protocol/commands/cmd_config`，跑通"桌面 App 设置同步"。
5. **第 5 步**：扩展其它 `cmd_*.cpp` 与 UI 屏（音乐 / PC 状态 / HA / 设置）。
6. **第 6 步**：接入 WiFi / BLE / 语音 / RGB / 音频。
7. **第 7 步**：补齐占位（2.4G、频谱、OTA）。

每一步都应保证既有功能不被破坏（任务边界 + 同步策略严格执行）。

---

## 9. 与现有 demo 的迁移清单

当前 `src/main.cpp` 内已实现的内容需迁移到目标目录：

| 现有位置（`src/main.cpp`）           | 目标位置                                                             |
| ------------------------------------ | -------------------------------------------------------------------- |
| `TFT_*` 引脚宏 / `gfx` / `bus`       | `src/display/DisplayDriver.cpp`                                      |
| `lvgl_display_init` / `lvgl_tick_cb` | `src/display/LvglPort.cpp`                                           |
| `create_ui()` 占位                   | `src/ui/ui.cpp`（SquareLine 接管）+ `src/ui/ui_helpers.cpp` 启动钩子 |
| `setup()` 中的"启动顺序"             | `src/app/BootStage.cpp`                                              |
| `loop()` 中 LVGL tick                | `src/tasks/DisplayTask.cpp`（DisplayTask 接管 tick）                 |

迁移完成后，`src/main.cpp` 仅保留约 30 行：`Serial.begin` → `AppContext::init()` → `MainTask::begin()` / `DisplayTask::begin()`。
