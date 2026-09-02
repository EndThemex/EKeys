# EKeys Agent 文档

> 面向接手/维护 EKeys 项目的 AI Agent（以及后续人类工程师）的快速入门。
> 阅读完本文件后，应能理解项目的目标、架构、关键约定，并能安全地修改代码。

---

## 1. 项目一句话

**EKeys** 是一款基于 **ESP32-S3 + Arduino + LVGL 8.3** 的迷你无线键盘（9 键 + 1 EC11 旋钮），
通过 **BLE HID** 作为键盘输出，配套 **NV3007 142×428 竖屏** 显示菜单/状态/拟态 UI，
底部 **WS2812B ×9 RGB** 指示灯与按键 1:1 联动。集成番茄钟、麦克风频谱、键位 profile 切换等功能。

---

## 2. 目标读者与使用方式

- 本文件是给 LLM Agent 看的项目说明书。
- 所有设计决策、踩过的坑、对外契约均在 `src/docs/` 下；遇到不确定的细节，优先翻这里。
- 修改代码前必须先 `Read` 对应文件（不要凭记忆改）。

---

## 3. 硬件约束（写死，不可破坏）

| 项               | 值                                                    | 来源                                                                            |
| ---------------- | ----------------------------------------------------- | ------------------------------------------------------------------------------- |
| MCU              | ESP32-S3-WROOM-1                                      | `platformio.ini`                                                                |
| 屏幕             | NV3007，物理 142×428，旋转 1 后逻辑 428×142           | [main.cpp](file:///d:/search/esp/Keys/EKeys/src/main.cpp)                       |
| 屏幕逻辑尺寸常量 | `SCREEN_W_PX=428`, `SCREEN_H_PX=142`                  | [Pages.h](file:///d:/search/esp/Keys/EKeys/src/ui/Pages.h)                      |
| 键盘矩阵         | 3 行 × 4 列扫描，实际 **9 键**（梯形布局）            | [KeyScanConfig.h](file:///d:/search/esp/Keys/EKeys/src/keyscan/KeyScanConfig.h) |
| 旋钮             | EC11，A=5 / B=21 / SW=9，**不使用 ESP32Encoder PCNT** | [KeyScanConfig.h](file:///d:/search/esp/Keys/EKeys/src/keyscan/KeyScanConfig.h) |
| RGB              | WS2812B ×9，PIN=6，VCC 控制 PIN=36                    | [RGBLightControl.h](file:///d:/search/esp/Keys/EKeys/src/rgb/RGBLightControl.h) |
| 蓝牙名           | `EKeys`（`EKEYS_DEVICE_NAME`）                        | `platformio.ini`                                                                |

矩阵行/列 GPIO 与梯形键位映射见 [07-pinout.md](file:///d:/search/esp/Keys/EKeys/src/docs/07-pinout.md)。

---

## 4. 顶层架构

```
┌──────────────────────────────────────────────────────────┐
│                       main.cpp                           │
│  setup(): gfx → lvgl → rgb → ble → pageMgr → scanTask   │
│  loop():  drain queues → PageManager + BLE → lv_timer    │
└──────────────────────────────────────────────────────────┘
        │            │             │              │
        ▼            ▼             ▼              ▼
   ┌────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐
   │  GFX   │  │   LVGL   │  │ BLE HID  │  │  KeyScan   │
   │ NV3007 │  │  8.3.11  │  │T-vK 0.3.0│  │ FreeRTOS   │
   └────────┘  └──────────┘  └──────────┘  └────────────┘
                                       ▲
                                       │ 事件
                                       │
                              ┌────────┴────────┐
                              │  PageManager    │
                              │  + Page (LVGL)  │
                              └─────────────────┘
```

各模块职责：

| 模块     | 路径                                                          | 职责                                           |
| -------- | ------------------------------------------------------------- | ---------------------------------------------- |
| 显示     | `lib/GFX Library for Arduino/`（外部）+ `src/main.cpp`        | NV3007 SPI 初始化 + LVGL flush                 |
| UI       | [src/ui/](file:///d:/search/esp/Keys/EKeys/src/ui/)           | Page 基类 + 9 个具体页 + PageManager           |
| 按键扫描 | [src/keyscan/](file:///d:/search/esp/Keys/EKeys/src/keyscan/) | MatrixScanner / RotaryEncoder / IKeySource     |
| BLE      | [src/ble/](file:///d:/search/esp/Keys/EKeys/src/ble/)         | BleKeyboardSink（发送） + BleKeyMap（profile） |
| RGB      | [src/rgb/](file:///d:/search/esp/Keys/EKeys/src/rgb/)         | FastLED 封装 + 灯效状态机                      |
| 麦克风   | [src/mic/](file:///d:/search/esp/Keys/EKeys/src/mic/)         | arduinoFFT 频谱（MicPage 显示）                |
| 设计文档 | [src/docs/](file:///d:/search/esp/Keys/EKeys/src/docs/)       | 架构、契约、踩坑记录                           |

---

## 5. 关键设计决策（不要轻易推翻）

详见 [README.md](file:///d:/search/esp/Keys/EKeys/src/docs/README.md) 的"关键决策一览"。重点：

- **扫描周期 1 ms / 消抖 5 ms**：状态机要求至少 1 ms 一次采样。
- **scanTask 跑在 Core 0 / 优先级 2**：与 LVGL/Main 分离，避免互相阻塞。
- **事件走 FreeRTOS 队列，main loop 消费**：[main.cpp](file:///d:/search/esp/Keys/EKeys/src/main.cpp#L297-L308) 定义 5 条队列。
- **编码器不用 ESP32Encoder PCNT**：PCNT 需独占引脚，与矩阵 GPIO 复用冲突；改用轮询 + 滤波状态机。
- **BLE 库 key 码 ≠ USB HID Usage ID**：必须走 `hidToLibKey()` 转换（[BleKeyMap.h](file:///d:/search/esp/Keys/EKeys/src/ble/BleKeyMap.h#L40-L43)），否则只出空格。
- **`consumesEncoder()` 控制旋转是否穿透到 BLE 方向键**：[Page.h](file:///d:/search/esp/Keys/EKeys/src/ui/Page.h#L75)。
- **旋钮 BLE 按键"延后 release"**：[main.cpp](file:///d:/search/esp/Keys/EKeys/src/main.cpp#L419-L444) 用 `s_pendingEncRelease` 避免与矩阵键冲突。
- **PageKind 决定 KEY3..KEY9 语义**：[Page.h](file:///d:/search/esp/Keys/EKeys/src/ui/Page.h#L13-L20)；新增页必须声明 `kind()`。
- **LVGL draw_buf 必须手动挂**：[main.cpp](file:///d:/search/esp/Keys/EKeys/src/main.cpp#L159-L166) 否则 `LoadProhibited` 崩溃。
- **Serial 日志用 FreeRTOS mutex，不用关中断**：[main.cpp](file:///d:/search/esp/Keys/EKeys/src/main.cpp#L42-L67) 否则触发 Interrupt wdt timeout。

---

## 6. 页面清单（PageId 唯一）

来源：[Pages.h](file:///d:/search/esp/Keys/EKeys/src/ui/Pages.h#L17-L28)

| ID  | 名         | PageKind | 用途                        |
| --- | ---------- | -------- | --------------------------- |
| 1   | MenuPage   | List     | 主菜单首页                  |
| 2   | RgbPage    | Mode     | RGB 灯效/亮度控制           |
| 3   | TomatoPage | State    | 番茄钟状态机                |
| 4   | StatusPage | ReadOnly | 系统状态（保留）            |
| 5   | BlePage    | Action   | 蓝牙连接状态                |
| 6   | MicPage    | ReadOnly | 麦克风频谱                  |
| 7   | KeyMapPage | List     | BLE profile 切换 / 单键编辑 |
| 8   | NeumoPage  | —        | 拟态组件展示                |
| 9   | ThemePage  | —        | 主题色查看                  |

新增页面流程：

1. 在 [Pages.h](file:///d:/search/esp/Keys/EKeys/src/ui/Pages.h) 加 `PAGE_XXX`。
2. 新建 `XxxPage.{h,cpp}`，继承 `Page`，声明 `kind()`。
3. 在 [main.cpp](file:///d:/search/esp/Keys/EKeys/src/main.cpp#L246-L261) `registerAllPages()` 注册。
4. 在 [PageManager.h](file:///d:/search/esp/Keys/EKeys/src/ui/PageManager.h) 路由里加默认分支（如需）。

---

## 7. 输入分配规则（KEY / 旋钮 / PageKind）

详见 [10-input-mapping-rule.md](file:///d:/search/esp/Keys/EKeys/src/docs/10-input-mapping-rule.md)。

- **KEY1** → 弹栈（回上一级），全局统一。
- **KEY2** / 旋钮单击 → `onConfirm()`（进入/确认）。
- **KEY3..KEY9** → 按 `PageKind` 自动路由到 `selectItem/Mode/State/Action(idx)`。
- **旋钮旋转** → `onEncoder(delta)`；若 `consumesEncoder()=false`，额外发 BLE 方向键。
- **旋钮单击/双击/长按** → BLE 上报 Enter/Esc/Tab + 单击同步触发 `onConfirm()`。

---

## 8. BLE Profile（持久化）

- 4 套预置 profile，定义在 [BleKeyMap.cpp](file:///d:/search/esp/Keys/EKeys/src/ble/BleKeyMap.cpp)。
- 当前索引写入 NVS（namespace=`ekeys`, key=`ble_profile`），重启保留。
- 写入时机：[main.cpp](file:///d:/search/esp/Keys/EKeys/src/main.cpp#L222-L235) 每帧末尾 `saveBleProfileToNvs()`。
- **注意**：`BLE_KEY_MAP[10]` 等 3 个数组**必须可写**（不能 `static constexpr`），通过 `extern` 声明 + `.cpp` 单一定义。

---

## 9. 编码约定（强制）

- 命名空间：`namespace ekeys { ... }`，**禁止污染全局**。
- 类成员后缀 `_`（`currentEffect_`、`enabled_`）。
- 头文件用 `#pragma once`。
- C++11 兼容：不要用 `inline` 变量等 C++17 特性（见 [10-input-mapping-rule.md §10.1](file:///d:/search/esp/Keys/EKeys/src/docs/10-input-mapping-rule.md)）。
- 常量集中放在对应模块的 `.h`（如 `KeyScanConfig.h`），**不允许散落 magic number**。
- 屏幕尺寸只用 [Pages.h](file:///d:/search/esp/Keys/EKeys/src/ui/Pages.h) 的 `SCREEN_W_PX/SCREEN_H_PX`，**不要在 UI 代码里写 428/142 字面量**。
- UI 布局围绕 428×142，竖屏设计，分辨率极低，注意字号与控件密度。
- 错误日志统一走 `SERIAL_PRINTF()`（带 mutex），不要直接 `Serial.printf`。

---

## 10. 编译与烧录

- **不要主动编译**，除非用户明确要求（见 `.trae/rules/rule.md`）。
- 烧录：`platformio run -t upload`（已配置 `upload_protocol=esptool`, `upload_speed=921600`）。
- 串口监视：`platformio device monitor`（115200，已禁用 DTR/RTS 复位）。
- 关闭 BLE：拷贝 [platformio.ini](file:///d:/search/esp/Keys/EKeys/platformio.ini) 注释里的 `esp32-s3-devkitc-1-no-ble` 环境。

---

## 11. 常见陷阱（Agent 必读）

1. **不要改 `BLE_KEY_MAP` 为 `inline` 变量**——C++17 特性，本工程 Arduino 默认 C++11，会 ODR。
2. **不要在 UI 里直接 `lv_scr_act()` 后改背景**——必须在 `lv_disp_drv_register()` 之后立刻改（见 [main.cpp](file:///d:/search/esp/Keys/EKeys/src/main.cpp#L168-L174)）。
3. **不要用 `portENTER_CRITICAL` 包 `Serial.printf`**——会触发 Interrupt wdt timeout。用 `SERIAL_PRINTF`。
4. **不要在旋钮 BLE 按键路径上调用 `releaseAll()`**——会把矩阵键的 HID 报告一起清掉。
5. **不要把矩阵扫描 GPIO 和编码器 GPIO 混用**——扫描结束后必须释放为高阻，避免编码器读到错误电平。
6. **新建 Page 必须声明 `kind()`**——基类按 PageKind 路由 KEY3..KEY9，不声明会编译失败。
7. **屏幕旋转后 X/Y 别搞反**——逻辑尺寸是 428×142（旋转 1 后）。
8. **BLE Keyboard 库的 `press(k)` 不接受裸 HID Usage ID**——必须走 `hidToLibKey()`。

---

## 12. 修改流程建议

接到任务时按下列顺序：

1. **读**对应模块的 `.h` + `.cpp` + `docs/` 下相关文档。
2. **定位**改动点（行号、符号）。
3. **改**：使用 `Edit`（不要 `Write` 整个文件，除非新建）。
4. **检查常量**：新引入的常量必须先在合适位置 `#define` 或 `constexpr`，并在文档/注释中说明。
5. **保存**（本环境自动，但确认无报错）。
6. **不编译**——等用户说"编译"再编译。

---

## 13. 相关文档导航

- 按键扫描总览 → [01-keyscan-overview.md](file:///d:/search/esp/Keys/EKeys/src/docs/01-keyscan-overview.md)
- BLE 键盘与 KeyMap → [09-ble-keyboard.md](file:///d:/search/esp/Keys/EKeys/src/docs/09-ble-keyboard.md)
- 输入分配规则 → [10-input-mapping-rule.md](file:///d:/search/esp/Keys/EKeys/src/docs/10-input-mapping-rule.md)
- MenuPage 高亮条 jitter → [08-menu-highlight-jitter.md](file:///d:/search/esp/Keys/EKeys/src/docs/08-menu-highlight-jitter.md)
- 接口契约 → [06-interface-contract.md](file:///d:/search/esp/Keys/EKeys/src/docs/06-interface-contract.md)
- 引脚分配 → [07-pinout.md](file:///d:/search/esp/Keys/EKeys/src/docs/07-pinout.md)

---

_文档版本：v1.0（2026-09-02）_
