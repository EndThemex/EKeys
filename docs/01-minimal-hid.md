# 阶段 01 — 最小 HID 跑通

> 状态：未开始
> 关联章节：[`FEATURE_DOC.md §2.1`](../FEATURE_DOC.md) / [`§4`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.3 / §3.4 / §3.5`](../ARCHITECTURE.md)

## 目标

打通"按键矩阵 → USB HID 键盘"最小链路，验证基础任务与目录结构能落地，并保证后续阶段可以增量叠加而不破坏既有功能。

## 范围

1. 建立 `src/input/`、`src/keymap/`、`src/output/`、`src/tasks/` 目录与最小组件。
2. 在 `setup()` 中挂载 `MainTask`（Core 1，单任务即可），暂不引入 `DisplayTask`。
3. 不引入 SPIFFS / 配置 / 协议；键映射直接硬编码默认值。
4. 不引入 LVGL；NV3007 屏幕在本阶段保持关闭。

## 前置条件

- [`../platformio.ini`](../platformio.ini) 已包含 `lvgl/lvgl@8.3.11` 与 `GFX Library for Arduino@1.6.0`。
- [`../partitions-16MB.csv`](../partitions-16MB.csv) 已就绪。
- 硬件：`PINOUT.md §2.3 EC11` / `§2.5 按键矩阵` 引脚未冲突。

## 任务清单

- [ ] **1.1 目录骨架**：创建 `src/app/`、`src/tasks/`、`src/input/`、`src/keymap/`、`src/output/`、`src/utils/`、`src/config/`、`src/logging/`。
- [ ] **1.2 `src/logging/LogManager.h/.cpp`**：占位实现，导出 `LOG_DEBUG/INFO/WARNING/ERROR` 宏到 `Serial.printf`。本期只输出到 UART0。
- [ ] **1.3 `src/utils/event_types.h`**：仅声明 `enum class InputSource { MatrixKey, Ec11Knob, Slider, ModBKnob, AsrTrigger }` 与 `struct InputEvent { uint8_t id; InputSource src; bool pressed; uint32_t ts_ms; }`。
- [ ] **1.4 `src/utils/keymap_types.h`**：声明 `struct KeyMapping { String function_key; std::array<String,6> normal_key; std::array<String,5> macros_key; bool valid; }`（`String` 在本期临时使用，后续替换为 `BoundedString`）。
- [ ] **1.5 `src/input/MatrixScanner.h/.cpp`**：按 `FEATURE_DOC §2.1` 实现 `scan()` / `getStableState()` / `getPressedKeys()` / `getReleasedKeys()`。引脚采用 `PINOUT.md §2.5`。
- [ ] **1.6 `src/keymap/KeyNameTable.h/.cpp`**：实现字符/键名字符串 → HID keycode 映射（仅本期必需的 `a-z / 0-9 / Enter / Backspace / Space`），常量集中定义。
- [ ] **1.7 `src/keymap/KeyResolver.h/.cpp`**：持有 `std::array<KeyMapping,11>` 默认值；提供 `get(uint8_t keyId)` / `press(uint8_t keyId)` / `release(uint8_t keyId)`。
- [ ] **1.8 `src/output/IKeyboard.h`**：纯虚接口 `begin / press(uint8_t) / press(const String&) / release / releaseAll / isConnected / send`。
- [ ] **1.9 `src/output/USBKeyboardImpl.h/.cpp`**：基于 TinyUSB HID + Consumer Control 实现；依赖 `platformio.ini` 已启用 `-DARDUINO_USB_MODE=1` / `-DARDUINO_USB_CDC_ON_BOOT=1`，使 USB HID 键盘与 USB CDC 串口同时可用。本期暂用 `ESP32-tinyUSB-HID-Keyboard` 或内建 `USBHIDKeyboard`，二选一在实现时确认。
- [ ] **1.10 `src/output/KeyboardFactory.h/.cpp`**：本期只返回 `USBKeyboardImpl`；`Configuration::WORK_MODE` 暂时写死 0。
- [ ] **1.11 `src/tasks/MainTask.h/.cpp`**：单线程（不创建 FreeRTOS 任务），含 `begin()` / `loop()`；`loop()` 中按 5ms 周期 `MatrixScanner::scan()` + 处理边沿 + `KeyResolver` + `IKeyboard`。
- [ ] **1.12 `src/main.cpp`**：删掉 NV3007 / LVGL 初始化，保留 `Serial.begin(115200)` → `MainTask::begin()` → `MainTask::loop()`。
- [ ] **1.13 编译验证**：`pio run -e esp32-s3-wroom-1-n16r8` 通过；按住任一物理按键时主机上能观察到对应字符/键。
- [ ] **1.14 自检记录**：在 `变更记录` 记录所使用的 USB HID 库及版本。

## 验收标准

- 11 个物理按键按 1~11 顺序触发 `a b c d e f g h i j k`（默认映射）；松开后主机无残留按键状态。
- `Serial` 输出每秒 ≤ 1 行（避免噪声）。
- 代码量：`src/output/` ≤ 200 行；`src/keymap/` ≤ 250 行；`src/input/MatrixScanner.cpp` ≤ 150 行。
- 没有出现跨文件全局变量；所有外部依赖通过 `extern` 或头文件引用。

## 变更记录

- _暂无_

## 备注

- 本阶段不创建 `DisplayTask`，LVGL 全部关闭；屏幕黑屏不影响功能验证。
- `KeyMapping` 暂时使用 `String`，便于后续替换为固定容量版本。
- TinyUSB 的引入会改变 USB 栈，需在 `platformio.ini` 的 `build_flags` 中追加对应定义（如 `-DARDUINO_USB_MODE=1` / `-DARDUINO_USB_CDC_ON_BOOT=1`），并在本阶段 `变更记录` 中记录实际使用的定义。
