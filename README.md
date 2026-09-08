# EKeys

基于 **ESP32-S3-WROOM-1 (N16R8)** 的 11 键 HID 键盘 + 旋钮 + LCD 触摸显示固件（Arduino + PlatformIO 6.x）。
支持 USB HID / BLE 双键盘、LVGL 8.3 图形界面、按键矩阵扫描、EC11 旋钮、WS2812B RGB LED、I2S 麦克风/功放、Wi-Fi 配网与 TCP 桌面 App 协议。

---

## 1. 项目简介

EKeys 是一个面向 **桌面 / 平板场景** 的多功能宏键盘主控：11 个物理按键 + 1 个 EC11 旋钮 + 一块 SPI LCD + 一圈 RGB 灯，既可以当普通 HID 键盘使用，也可以通过 Wi-Fi / USB 与桌面 App 联动，在 11 个 Profile 中一键切换键映射、灯光、主题与 PC 状态显示。

整套固件运行在 ESP32-S3 (N16R8) 上，使用 Arduino + PlatformIO 工具链开发。

---

## 2. 核心功能

### 2.1 输入

- **3×4 矩阵（11 键）**：行列扫描 + 10 ms 消抖，1~11 应用键 ID
- **板载 EC11 旋钮**：单击/双击/旋转，仅用于屏幕导航（**不进入 HID 键映射**）
- **外挂模块（I2C）**：ModA（旋钮 + 滑动电位器，`0x06`）、ModB（机械旋钮，`0x08`）
- **语音触发**：默认键 11 触发 ASR（百度短语音 REST API）

### 2.2 输出

- **USB HID 键盘 + 消费控制**（TinyUSB CDC + HID）
- **BLE HID 键盘**（`t-vk/ESP32 BLE Keyboard`，自动释放经典蓝牙内存）
- **WS2812B × 11**：单色 / 彩虹 / 颜色循环 / 火焰 / 呼吸 / 按键点击高亮
- **I2S 音频**：MAX98357 功放 + ICS43434 麦克风；本地 WAV + 远程音频流

### 2.3 显示

- **SPI LCD (NV3007, 428×142)**：Arduino_GFX 驱动
- **LVGL 8.3.11 UI**：SquareLine Studio 生成的 11 屏界面
  - 主屏 / 键映射 / 音乐 / PC 状态 / HA 状态 / 设置（含二级页）
- **状态条**：工作模式 / 音量 / WiFi / TCP / 模块在线 / 录音动画

### 2.4 存储与配置

- **SPIFFS**：`config.ini` + 8 套 `keymapN.ini` + Profile 图标 + 音频
- **SimpleIni** 读写，互斥量保护
- **NVS**：DeviceSettings 持久化
- **8 套 Profile**：自定义图标 + 显示名 + 独立键映射

### 2.5 网络与协议

- **Wi-Fi STA**：自动重连，NTP 同步（GMT+8）
- **TCP Server (30000)**：桌面 App 控制通道
- **UDP 广播发现 (30001)**：自动协商 IP
- **USB CDC 115200**：双通道与 TCP 并行；JSON 行协议 + 心跳
- **18 类命令**：键映射 / 配置 / Profile / 设备信息 / 固件 / PC 状态 / 音乐 / HA 状态 / 语音文本 / OTA

### 2.6 其它

- **OTA 升级**：HTTP 流式下载 + MD5 校验，失败不覆盖原固件
- **日志系统**：分级（DEBUG/INFO/WARN/ERROR），可注册回调
- **电源模式**：NORMAL / LOW / LIGHT / DEEPSLEEP（占位，5V 升压可控）
- **双 FreeRTOS 任务**：`MainTask` (Core 1) + `DisplayTask` (Core 0)，消息队列通信

---

## 3. 项目特点

- **硬件无触屏**：UI 操作仅依赖 EC11 旋钮 + 11 键矩阵，SquareLine 触屏按钮保留作未来扩展
- **桌面 App 联动**：通过自研协议在局域网内与 PC 双向同步键事件、键映射、设置、状态
- **键映射灵活**：每键支持 `function_key` / `normal_key[]` / `macros_key[]` 三种内容，互斥生效
- **8 套 Profile**：按场景一键切换（如：默认 / 剪辑 / 设计 / 代码 / …）
- **PC 状态屏**：显示 CapsLock、网络速率、CPU / 内存 / 温度 / 磁盘 IO（数据由桌面 App 推送）
- **可替换架构**：USB / BLE 键盘后端通过 `IKeyboard` 抽象，按 `WORK_MODE` 动态切换
- **构建可追溯**：依赖版本固定、分区表 ASCII、字体白名单、烧录参数集中
- **完整文档体系**：FEATURE_DOC（功能）/ ARCHITECTURE（结构）/ PINOUT（引脚）/ docs（阶段计划）

---

## 4. 快速开始

```bash
# 1. 克隆仓库
git clone <repo-url> EKeys
cd EKeys

# 2. 编译
pio run -e esp32-s3-wroom-1-n16r8

# 3. 烧录固件
pio run -e esp32-s3-wroom-1-n16r8 -t upload

# 4. 上传 SPIFFS 资源（首次必做）
pio run -e esp32-s3-wroom-1-n16r8 -t uploadfs

# 5. 串口监视
pio device monitor -b 115200
```

> 详细编译参数、烧录失败排查、擦除 Flash 等见 [`docs/COMPILING.md`](./docs/COMPILING.md)。

---

## 5. 文档导航

| 文档                                                               | 说明                                     |
| ------------------------------------------------------------------ | ---------------------------------------- |
| [**FEATURE_DOC.md**](./FEATURE_DOC.md)                             | 功能需求总览（输入文档，按 18 节展开）   |
| [**ARCHITECTURE.md**](./ARCHITECTURE.md)                           | 项目结构设计 / 模块划分 / 依赖与同步策略 |
| [**PINOUT.md**](./PINOUT.md)                                       | 全部硬件引脚分配（按模块 / 按引脚号）    |
| [**docs/COMPILING.md**](./docs/COMPILING.md)                       | 编译、烧录、SPIFFS 上传、串口监视、擦除  |
| [**docs/PROJECT_LAYOUT.md**](./docs/PROJECT_LAYOUT.md)             | 目录速览 / 关键文件 / 文档体系           |
| [**docs/TROUBLESHOOTING.md**](./docs/TROUBLESHOOTING.md)           | 硬件 / 软件注意事项 / 常见问题速查       |
| [**docs/desktop-app-protocol.md**](./docs/desktop-app-protocol.md) | 桌面 App 通信协议（命令与字段约定）      |
| [**docs/README.md**](./docs/README.md)                             | 阶段任务计划索引（01 ~ 07）              |

### 阶段任务

| 阶段 | 文档                                                                  | 目标                                |
| ---- | --------------------------------------------------------------------- | ----------------------------------- |
| 01   | [01-minimal-hid.md](./docs/01-minimal-hid.md)                         | 按键矩阵 → USB HID 键盘             |
| 02   | [02-display-lvgl-port.md](./docs/02-display-lvgl-port.md)             | NV3007 + LVGL 初始化迁出 `main.cpp` |
| 03   | [03-config-persistence.md](./docs/03-config-persistence.md)           | SPIFFS + SimpleIni 持久化键映射     |
| 04   | [04-protocol-config-sync.md](./docs/04-protocol-config-sync.md)       | 私有协议 `CMD_CONFIG_SET` 同步      |
| 05   | [05-ui-screens.md](./docs/05-ui-screens.md)                           | 音乐 / PC 状态 / HA / 设置屏        |
| 06   | [06-network-voice-rgb-audio.md](./docs/06-network-voice-rgb-audio.md) | WiFi / BLE / 语音 / RGB / 音频      |
| 07   | [07-placeholder-completion.md](./docs/07-placeholder-completion.md)   | 2.4G / 频谱 / OTA / 占位命令补齐    |

---

## 6. 仓库结构（速览）

```
EKeys/
├── platformio.ini              # 构建配置
├── partitions-16MB.csv         # 16MB Flash 分区表
├── README.md                   # 主页（本文件）
├── FEATURE_DOC.md              # 功能需求
├── ARCHITECTURE.md             # 项目结构
├── PINOUT.md                   # 硬件引脚
├── docs/                       # 阶段任务 / 协议 / 编译指南 / 排错
├── data/                       # SPIFFS 资源（config.ini / keymapN.ini / …）
├── include/                    # 全局头（lv_conf.h）
├── lib/                        # 本地库（GFX Library for Arduino）
├── src/                        # 主源码（app/ audio/ config/ display/ hardware/
│                               #         input/ keymap/ logging/ network/ output/
│                               #         protocol/ rgb/ services/ tasks/ ui/ upgrade/ voice/）
├── boards/                     # PlatformIO 自定义 board 定义
└── test/                       # 单元测试占位
```

> 完整目录与文件用途见 [`docs/PROJECT_LAYOUT.md`](./docs/PROJECT_LAYOUT.md)。
