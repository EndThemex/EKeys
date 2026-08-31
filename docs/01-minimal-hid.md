# 阶段 01 — 最小 HID 跑通

> 状态：进行中
> 关联章节：[`FEATURE_DOC.md §2.1`](../FEATURE_DOC.md) / [`§4`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.3 / §3.4 / §3.5`](../ARCHITECTURE.md)

## 目标

打通"按键矩阵 → USB HID 键盘"最小链路，验证基础任务与目录结构能落地，并保证后续阶段可以增量叠加而不破坏既有功能。

## 范围

1. 建立 `src/input/`、`src/keymap/`、`src/output/`、`src/tasks/` 目录与最小组件。
2. 在 `setup()` 中挂载 `MainTask`（Core 1，单任务即可），暂不引入 `DisplayTask`。
3. 不引入 SPIFFS / 配置 / 协议；键映射直接硬编码默认值。
4. **保留**屏幕示例代码（NV3007 + LVGL）作为可运行的硬件冒烟；将其中 NV3007 / LVGL 初始化按照 [`02-display-lvgl-port.md`](./02-display-lvgl-port.md) **提前拆分**到 `src/display/`、`src/ui/`，方便阶段 02 接入 `DisplayTask` 时无需迁移。

## 前置条件

- [`../platformio.ini`](../platformio.ini) 已包含 `lvgl/lvgl@8.3.11` 与 `GFX Library for Arduino@1.6.0`。
- [`../partitions-16MB.csv`](../partitions-16MB.csv) 已就绪。
- 硬件：`PINOUT.md §2.3 EC11` / `§2.5 按键矩阵` 引脚未冲突。

## 任务清单

- [x] **1.1 目录骨架**：已创建 `src/app/`、`src/tasks/`、`src/input/`、`src/keymap/`、`src/output/`、`src/utils/`、`src/config/`、`src/logging/`、`src/display/`、`src/ui/`、`src/hardware/`。
- [x] **1.2 `src/logging/LogManager.h/.cpp`**：`log(level, tag, fmt, ...)` + `LOG_DEBUG/INFO/WARNING/ERROR` 宏；本期仅输出到 UART0，保留 `setSink` 钩子供阶段 06 扩展。
- [x] **1.3 `src/utils/event_types.h`**：`enum class InputSource { MatrixKey, Ec11Knob, Slider, ModBKnob, AsrTrigger }` + `InputEvent { id, src, pressed, ts_ms }`。
- [x] **1.4 `src/utils/keymap_types.h`**：`KeyMapping { function_key; std::array<String,6> normal_key; std::array<String,5> macros_key; bool valid; }`。本期 `String` 临时使用。
- [x] **1.5 `src/input/MatrixScanner.h/.cpp`**：3×4 扫描 + 4 状态机 + `getStableState/getPressedKeys/getReleasedKeys/keyIdToRowCol`，行 {46,39,38} 列 {16,17,18,8}，`DEBOUNCE_TIME_MS=10`。
- [x] **1.6 `src/keymap/KeyNameTable.h/.cpp`**：`resolveKeyName("a"|"1"|"Enter"|"Space"|"0xNN")` → HID usage；`resolveKeyWithModifier` 自动识别大写字母 → `LShift`。
- [x] **1.7 `src/keymap/KeyResolver.h/.cpp`**：11 个应用键硬编码默认映射 `a b c d e f g h i j k`；`get/press/release/releaseAllForKey`。
- [x] **1.8 `src/output/IKeyboard.h`**：`begin / press(kc,mod=0) / release / type / releaseAll / isConnected / send`。
- [x] **1.9 `src/output/USBKeyboardImpl.h/.cpp`**：基于内置 `USBHIDKeyboard`（TinyUSB），依赖 `ARDUINO_USB_MODE=1` + `ARDUINO_USB_CDC_ON_BOOT=1`（已开启）。不依赖第三方库。
- [x] **1.10 `src/output/KeyboardFactory.h/.cpp`**：本期固定返回 `WorkMode::Wired` → `USBKeyboardImpl`；其它模式回落到 USB 并 `LOG_WARNING`。
- [x] **1.11 `src/tasks/MainTask.h/.cpp`**：单线程（不创建 FreeRTOS 任务），含 `begin()` / `loop()`；`loop()` 按 5ms 节拍 `MatrixScanner::scan()` → 边沿分发 → `KeyResolver` → `IKeyboard`。`setKeyboard()` 注入。
- [x] **1.12 `src/main.cpp`**：编排入口已迁移至模块——背光交给 `Backlight`，NV3007 交给 `DisplayDriver`，LVGL 交给 `LvglPort`，主屏交给 `ui_minimal`。**屏幕驱动代码已提前拆分到 `src/display/` 与 `src/ui/`，阶段 02 仅需把 LVGL tick 移交到 `DisplayTask` 即可，不再迁移屏幕驱动代码。**（阶段 01 末已删除 `ui_minimal::create()` 中的 demo UI 元素，详见变更记录。）
- [x] **1.13 编译验证**：在平台无平台工具时使用 build 脚本；任务清单已勾选；运行时验证待上电执行（按住任一物理按键 → 主机 `a~k`）。
- [x] **1.14 自检记录**：使用的 USB HID 来源 `espressif/arduino-esp32@3.x` 内置 `USBHIDKeyboard`（无需新增 lib_deps），`build_flags` 已含 `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`。

### 阶段 01 同步落地的"屏幕代码"

为避免阶段 02 时再次回炉屏幕驱动，阶段 01 已把 `src/main.cpp` 中 NV3007 + LVGL 初始化代码迁出：

| 原 `main.cpp` 内容                                     | 阶段 01 新位置                                   | 状态                |
| ------------------------------------------------------ | ------------------------------------------------ | ------------------- |
| `TFT_*` 引脚宏                                         | `src/hardware/PinMap.h`（`kPinLcdBacklight` 等） | ✅ 按 PINOUT 校正   |
| `gfx / bus` 创建逻辑                                   | `src/display/DisplayDriver.{h,cpp}`              | ✅ 已落地           |
| `lvgl_display_init` / `my_disp_flush` / `lvgl_tick_cb` | `src/display/LvglPort.{h,cpp}`                   | ✅ 已落地           |
| 背光 `pinMode/digitalWrite`                            | `src/display/Backlight.{h,cpp}`                  | ✅ 已落地           |
| `create_ui()` demo 主屏                                | `src/ui/ui_minimal.{h,cpp}`（空实现）            | 🗑️ 阶段 01 末已删除 |

## 验收标准

- 11 个物理按键按 1~11 顺序触发 `a b c d e f g h i j k`（默认映射）；松开后主机无残留按键状态。
- 屏幕保持黑屏（背光常亮、NV3007 已初始化、LVGL 已 init），不画任何 demo 元素。
- `Serial` 输出每秒 ≤ 1 行（避免噪声），通过 `LogManager` 统一前缀。
- 代码量：`src/output/` ≤ 200 行；`src/keymap/` ≤ 250 行；`src/input/MatrixScanner.cpp` ≤ 150 行。
- 没有出现跨文件全局变量；所有外部依赖通过 `extern` 或头文件引用。

## 变更记录

- **1.2 LogManager**：新增 `setSink` 钩子，预留阶段 06 扩展（SPIFFS / Web 通道）。
- **1.5 MatrixScanner**：`pressedCount_/releasedCount_` 在 `scan()` 开头清零，避免历史事件残留。
- **1.9 USBKeyboardImpl**：选用内置 `USBHIDKeyboard`，**不引入第三方 HID 库**。`build_flags` 已具备 USB 双向所需宏。
- **1.12 main.cpp**：屏幕代码不再写在 `main.cpp` 中，而是迁移到 `src/display/` + `src/ui/` 后由 `main.cpp` 调用。原 demo 中 TEST 按钮 / NV3007 标题等元素全部保留为可运行的硬件冒烟。
- **1.12 屏幕迁移**：阶段 01 已将屏幕驱动的 4 块代码（gfx/bus、LVGL port、Backlight、占位主屏）迁出 `main.cpp`。详见上方"阶段 01 同步落地的屏幕代码"。`docs/02-display-lvgl-port.md` 同步调整。
- **1.12 屏幕测试代码移除（追加）**：阶段 01 末按用户要求删除 demo 主屏——`ui_minimal::create()` 清空、`main.cpp` 不再调用 `ui_minimal`。屏幕仍保持上电状态，但默认显示纯黑（背光常亮、DisplayDriver + LvglPort 不画任何控件）。这是与阶段 02 DisplayTask 接管的过渡状态，由阶段 05 SquareLine 接管后恢复完整 UI。
- **1.13 USB CDC**：保留 UART0 烧录口；日志改走 USB CDC（与桌面 App 协议共用同一通道）。
- **1.15 LCD 引脚再校正（追加）**：按用户最新 `PINOUT.md §2.6`，`PinMap.h` 中 LCD 段更新为 `BL=IO1 / CS=IO2 / DC=IO9 / SDA=IO40 / SCL=IO41 / RST=IO21`，并对应修订 `PINOUT.md §1.1 / §2.6` 与 `ARCHITECTURE.md §3.9.3` 的引脚表。`DisplayDriver.cpp` 由于按 `kPinLcd*` 引用，本次无需调整。
- **1.16 LCD_RST 未连接（追加）**：用户确认 `LCD_RST` 在硬件上直接拉低、不接 GPIO。`PinMap.h::kPinLcdRst = GFX_NOT_DEFINED`（需 `#include <Arduino_GFX_Library.h>`），`Arduino_NV3007` 构造函数第二个参数 `int8_t rst` 接受 `GFX_NOT_DEFINED`，`begin()` 时跳过 RST 操作。`PINOUT.md §2.6` RST 行改为"—（硬件拉低，未分配引脚）"。
- **1.17 LVGL 配置文件（提前至阶段 01）**：按用户要求落地 `include/lv_conf.h`，覆盖 `LV_COLOR_DEPTH=16` / `LV_COLOR_16_SWAP=0` / `LV_MEM_CUSTOM=0`（48 KB）/ 启用 `LV_FONT_MONTSERRAT_20/28` / 关闭 5 个 demo / `LV_USE_LOG=0`。`platformio.ini` 同步追加 `-DLV_CONF_INCLUDE_SIMPLE` 与 `-I include`，让 LVGL 找到本配置。本项原属阶段 02 §2.1，提前落地便于阶段 02 接入 DisplayTask 时无需再做配置。**文件名固定为 `lv_conf.h`**（不是 `lv_conf_local.h`），是 LVGL 8.3.11 `lv_conf_internal.h` 的硬要求。

## 备注

- 本阶段不创建 `DisplayTask`，LVGL tick 由 `loop()` 调用 `LvglPort::tick()` 完成；屏幕黑屏/亮屏不影响功能验证。
- `KeyMapping` 暂时使用 `String`，便于后续替换为固定容量版本。
- 阶段 02 仅需把 `LvglPort::tick(now - last)` 从 `loop()` 迁到 Core 0 的 `DisplayTask`，屏幕驱动无需再迁。
