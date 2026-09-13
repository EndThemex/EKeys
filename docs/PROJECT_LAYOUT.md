# 项目目录速览

> 详细模块划分、依赖与设计原则见 [`../ARCHITECTURE.md`](../ARCHITECTURE.md)；
> 功能与命令清单见 [`../FEATURE_DOC.md`](../FEATURE_DOC.md)；
> 编译 / 烧录命令见 [`./COMPILING.md`](./COMPILING.md)；
> 桌面 App 协议见 [`./desktop-app-protocol.md`](./desktop-app-protocol.md)。

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
├── .trae/                      # 项目规则与方案稿存档
├── docs/                       # 协议 / 编译 / 排错 / 项目布局
├── data/                       # SPIFFS 资源（config.ini / keymapN.ini / audio_pad.ini / 音频）
├── include/                    # 全局头（lv_conf.h / LvglMemPool.h）
├── lib/                        # 本地库（GFX Library for Arduino）
├── src/                        # 主源码
├── boards/                     # PlatformIO 自定义 board 定义
└── test/                       # 单元测试占位
```

---

## 2. `src/` 子系统

```
src/
├── main.cpp                    # Arduino setup/loop：初始化编排 + MainTask::loop()
├── message_types.h             # MainTask → DisplayTask 队列消息定义
├── app/                        # 全局上下文（AppContext）
├── audio/                      # Speaker / Mic / AudioAnalyzer / AudioPad
├── config/                     # Configuration / DeviceSettings / parseConfigSetCommand
├── display/                    # DisplayDriver / LvglPort / Backlight
├── hardware/                   # PinMap / BatteryMonitor
├── input/                      # MatrixScanner / RotaryEncoder / event_types
├── keymap/                     # KeyEventDispatcher / KeyNameTable
├── logging/                    # LogManager
├── network/                    # WiFi / NTP / TCP / Discovery / NetDiagnostics
├── output/                     # USB / BLE 键盘实现（IKeyboard 后端）
├── protocol/                   # SerialProtocol / CommandRegistry / commands
├── rgb/                        # WS2812B 驱动 / 动画 / 点击高亮
├── services/                   # ConfigStore / KeymapRepository / EventBus
├── tasks/                      # MainTask / DisplayTask
├── ui/                         # SquareLine 生成的 LVGL 屏幕（含 ui_<Screen>.c/.h）
├── upgrade/                    # OTA
├── utils/                      # 公共 POD 类型（event_types / keymap_types）
└── voice/                      # VoiceRecognizer / TencentAsrSigner
```

> 各目录职责与功能对照表见 [`../ARCHITECTURE.md`](../ARCHITECTURE.md) §3 / §4。

---

## 3. 关键文件

| 路径                            | 说明                                                                          |
| ------------------------------- | ----------------------------------------------------------------------------- |
| `platformio.ini`                | 构建环境、依赖、烧录参数                                                      |
| `partitions-16MB.csv`           | 16 MB Flash 分区（app / spiffs / nvs）                                         |
| `src/main.cpp`                  | 启动入口：splash → SPIFFS → AppContext → loop 仅跑 MainTask::loop()            |
| `src/hardware/PinMap.h`         | 集中所有引脚定义（与 `PINOUT.md` 保持一致）                                    |
| `src/config/DeviceSettings.h`   | 所有可设置项默认值（唯一来源）                                                |
| `src/display/LvglPort.h/.cpp`   | LVGL 端口 + 启动画面（splash）                                                |
| `data/config.ini`               | 设备主配置（WiFi / 模式 / RGB / 屏幕 / 语音 ...）                              |
| `data/keymapN.ini`              | 8 套 Profile 键映射                                                           |
| `data/audio_pad.ini`            | 11 键音效板绑定表                                                             |

---

## 4. 文档体系

```
EKeys/
├── README.md                              # 主页：项目介绍 + 文档导航
├── FEATURE_DOC.md                         # 功能需求（按 18 节展开，与代码 1:1 对应）
├── ARCHITECTURE.md                        # 项目结构设计
├── PINOUT.md                              # 硬件引脚
├── .trae/rules/rules.md                   # 项目长期规则（引脚 / UI 输入 / 任务路由 / 二级页契约）
└── docs/
    ├── README.md                          # 文档索引
    ├── desktop-app-protocol.md            # 桌面 App 通信协议（命令 / 字段 / 报文 / 重连）
    ├── COMPILING.md                       # 编译、烧录、SPIFFS 上传、监视、擦除
    ├── PROJECT_LAYOUT.md                  # 目录速览（本文件）
    └── TROUBLESHOOTING.md                 # 注意事项 / 常见问题
```

阅读建议顺序：`README.md` → `FEATURE_DOC.md` → `ARCHITECTURE.md` → `PINOUT.md` → `.trae/rules/rules.md` → `docs/desktop-app-protocol.md`（仅桌面 App 开发者）。
