# 项目目录速览

> 详细模块划分、依赖与设计原则见 [`../ARCHITECTURE.md`](../ARCHITECTURE.md)；
> 功能与命令清单见 [`../FEATURE_DOC.md`](../FEATURE_DOC.md)；
> 编译 / 烧录命令见 [`./COMPILING.md`](./COMPILING.md)。

---

## 1. 顶层目录

```
EKeys/
├── platformio.ini              # 构建配置
├── partitions-16MB.csv         # 16MB Flash 分区表
├── PINOUT.md                   # 硬件引脚
├── FEATURE_DOC.md              # 功能需求
├── ARCHITECTURE.md             # 项目结构
├── README.md                   # 项目主页
├── docs/                       # 分阶段任务计划 & 协议说明
├── data/                       # SPIFFS 资源（config.ini / keymapN.ini / …）
├── include/                    # 全局头（lv_conf.h）
├── lib/                        # 本地库（GFX Library for Arduino）
├── src/                        # 主源码
├── boards/                     # PlatformIO 自定义 board 定义
└── test/                       # 单元测试占位
```

---

## 2. `src/` 子系统

```
src/
├── main.cpp                    # Arduino setup/loop：初始化编排 + 任务创建
├── message_types.h             # MainTask → DisplayTask 队列消息定义
├── app/                        # 全局上下文（AppContext）
├── audio/                      # 喇叭 / 麦克风 / 频谱分析
├── config/                     # 配置定义 / 解析
├── display/                    # DisplayDriver / LvglPort / Backlight
├── hardware/                   # PinMap
├── input/                      # MatrixScanner / RotaryEncoder
├── keymap/                     # KeyEventDispatcher / KeyResolver / KeyNameTable
├── logging/                    # LogManager
├── network/                    # WiFi / NTP / TCP / Discovery / NetDiagnostics
├── output/                     # USB / BLE 键盘实现
├── protocol/                   # 私有协议（SerialProtocol / CommandRegistry / commands）
├── rgb/                        # WS2812B 驱动 / 动画 / 点击高亮
├── services/                   # ConfigStore / KeymapRepository
├── tasks/                      # MainTask / DisplayTask
├── ui/                         # SquareLine 生成的 LVGL 屏幕
├── upgrade/                    # OTA
├── utils/                      # 公共 POD 类型
└── voice/                      # VoiceRecognizer / AsrTokenCache
```

> 各目录职责与功能对照表见 [`../ARCHITECTURE.md`](../ARCHITECTURE.md) 第 3 节。

---

## 3. 关键文件

| 路径                          | 说明                                                                 |
| ----------------------------- | -------------------------------------------------------------------- |
| `platformio.ini`              | 构建环境、依赖、烧录参数                                             |
| `partitions-16MB.csv`         | 16 MB Flash 分区（app / spiffs / nvs）                               |
| `src/main.cpp`                | 启动入口：SPIFFS → Backlight → DisplayDriver → LvglPort → AppContext |
| `src/hardware/PinMap.h`       | 集中所有引脚定义（与 `PINOUT.md` 保持一致）                          |
| `src/config/DeviceSettings.h` | 所有可设置项默认值（唯一来源）                                       |
| `data/config.ini`             | 设备主配置（WiFi / 模式 / RGB / 屏幕 / 语音 …）                      |
| `data/keymapN.ini`            | 8 套 Profile 键映射                                                  |

---

## 4. 文档体系

```
EKeys/
├── README.md                   # 主页：项目介绍 + 文档导航
├── FEATURE_DOC.md              # 功能需求（输入文档）
├── ARCHITECTURE.md             # 项目结构设计
├── PINOUT.md                   # 硬件引脚
└── docs/
    ├── README.md               # 阶段任务索引
    ├── COMPILING.md            # 编译、烧录、监视、擦除
    ├── PROJECT_LAYOUT.md       # 目录速览（本文件）
    ├── TROUBLESHOOTING.md      # 注意事项 / 常见问题
    ├── desktop-app-protocol.md # 桌面 App 对接协议
    ├── 01-minimal-hid.md       # 阶段 01：按键 → USB HID
    ├── 02-display-lvgl-port.md
    ├── 03-config-persistence.md
    ├── 04-protocol-config-sync.md
    ├── 05-ui-screens.md
    ├── 06-network-voice-rgb-audio.md
    └── 07-placeholder-completion.md
```

阅读建议顺序：`README.md` → `FEATURE_DOC.md` → `ARCHITECTURE.md` → `PINOUT.md` → 阶段文档。
