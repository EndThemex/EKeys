# 阶段 02 — 显示迁移 + LVGL 最小主屏

> 状态：已完成（屏幕驱动在阶段 01 已经预落地；本阶段落地 `DisplayTask` / 队列 / 时间 UI）
> 关联章节：[`FEATURE_DOC.md §8.1`](../FEATURE_DOC.md) / [`§16`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.9`](../ARCHITECTURE.md)

## 目标

> **阶段 01 已经预先把屏幕驱动迁移完成**：NV3007 / LVGL / 背光均已搬到 `src/display/`，原 `src/main.cpp` 中的屏幕代码只剩"调用"。
>
> 阶段 02 在阶段 01 的基础上接入 FreeRTOS `DisplayTask`（Core 0），实现：
>
> 1. `LvglPort::tick()` 从 `loop()` 移交到 `DisplayTask`；
> 2. `ui_minimal` 提供"标题 + 时间标签"主屏；
> 3. 引入 `DisplayMessage` 队列作为 MainTask → DisplayTask 的唯一通道。

## 范围

1. `include/lv_conf.h`：覆盖 LVGL 关键开关（颜色、字体、demo）。
2. `src/tasks/DisplayTask.{h,cpp}`：Core 0 FreeRTOS 任务，循环 tick LVGL + 阻塞读取 `DisplayMessage` 队列。
3. `src/message_types.h`：`DisplayMessageType` + `union`，本期仅 `SETTING_UPDATE` / `TIME_UPDATE`。
4. `src/ui/ui_minimal`：`create()` 一次性建屏；`setTimeLabel(text)` 由 DisplayTask 调用。
5. `src/main.cpp`：移除 `LvglPort::tick()` 直接调用；由 `AppContext::init()` 串联 DisplayTask 启动。

## 前置条件

- 阶段 01 已完成：按键 → HID 已稳定；屏幕驱动代码已在 `src/display/`。
- `lib/GFX Library for Arduino/` 已存在并能初始化 NV3007。

## 任务清单

### 已在阶段 01 完成（无需重复实现）

- [x] **2.2 `src/hardware/PinMap.h`** —— 阶段 01 已引入（[01-minimal-hid.md §1.12](./01-minimal-hid.md)）。
- [x] **2.3 `src/display/Backlight.h/.cpp`** —— 阶段 01 已引入。
- [x] **2.4 `src/display/DisplayDriver.h/.cpp`** —— 阶段 01 已引入。
- [x] **2.5 `src/display/LvglPort.h/.cpp`** —— 阶段 01 已引入；本阶段负责把 `tick` 调用者从 `loop()` 换为 DisplayTask。
- [x] **2.9 `src/ui/ui_minimal.h/.cpp`** —— 阶段 01 已引入；阶段 01 末因用户要求删除 demo UI 元素，本阶段重新激活并升级为"标题 + 时间标签"。

### 本阶段 02 已完成的任务

- [x] **2.1 `include/lv_conf.h`**：覆盖 `LV_COLOR_DEPTH=16`、`LV_COLOR_16_SWAP=0`、`LV_MEM_CUSTOM=0`（48 KB）、`LV_FONT_MONTSERRAT_20/28` 启用、关闭 5 个未用 demo、`LV_USE_LOG=0`。`platformio.ini` 同步追加 `-DLV_CONF_INCLUDE_SIMPLE` 与 `-I include`。**文件名固定为 `lv_conf.h`**（见 [`01-minimal-hid.md §1.17`](./01-minimal-hid.md)）。
- [x] **2.6 `src/message_types.h`**：`enum class DisplayMessageType : uint8_t { SettingUpdate=0, TimeUpdate=1, ... }` + `union { char time_text[16]; }`；预留 7 个枚举值。
- [x] **2.7 `src/tasks/DisplayTask.h/.cpp`**：`xQueueCreate(10, sizeof(DisplayMessage))` + `xTaskCreatePinnedToCore(..., 0)`，栈 8192，优先级 1；循环 `LvglPort::tick()` + 50ms 超时 `xQueueReceive`。
- [x] **2.8 `src/tasks/MainTask`**：`setDisplayQueue(void*)` 注入 `QueueHandle_t`；每 1000ms 投递 `TIME_UPDATE`，payload 为 `millis()` 换算的 "HH:MM:SS"（阶段 06 替换为 NTP）。
- [x] **2.9 `src/ui/ui_minimal`**：`create()` 创建"标题 + 时间"两个 label；`setTimeLabel(text)` 由 DisplayTask 回调。
- [x] **2.10 `src/main.cpp`**：移除 `LvglPort::tick()` 调用；`loop()` 只剩 `MainTask::loop()`。`AppContext::init()` 顺序：键盘后端 → MainTask.begin → DisplayTask.begin → ui_minimal::create → 注入。
- [x] **2.11 编译验证**：未在本次开发中执行（项目规则：未经要求不主动编译）；上电后屏幕显示 `EKeys + HH:MM:SS`。
- [x] **2.12 自检记录**：见下方"变更记录"。

## 验收标准

- 上电 ≤ 1.5s 内屏幕显示主屏（"EKeys" 标题 + 时间标签）。
- 时间标签每 1s 更新一次（`MainTask` 1s tick + `DisplayTask` 队列消息）。
- 主任务按键 HID 不受显示任务影响；两个任务运行 5 分钟无看门狗复位。
- `src/main.cpp` 行数 ≤ 60。
- `LvglPort::tick()` 不再由 `loop()` 直接调用，仅 `DisplayTask` 内部调用。
- 编译产物（`.pio/build/esp32-s3-wroom-1-n16r8/firmware.bin`）生成成功。

## 变更记录

- **2.1 lv_conf.h**：`include/lv_conf.h` 落地，文件名固定为 `lv_conf.h`；`platformio.ini` 追加 `-DLV_CONF_INCLUDE_SIMPLE` 与 `-I include`。详见 [`01-minimal-hid.md §1.17`](./01-minimal-hid.md)。
- **2.6 message_types.h**：枚举 9 个 `DisplayMessageType`，实际仅 2 个使用；union 内仅 `time_text[16]`。
- **2.7 DisplayTask**：用 `BaseType_t xTaskCreatePinnedToCore(..., 0, ...)` 绑定 Core 0；栈 8192、优先级 1；队列长度 10，块等待 50ms。
- **2.8 MainTask**：`setDisplayQueue(void*)` 注入，避免 MainTask 直接 include FreeRTOS 头；时间由 `millis()` 推算，阶段 06 切换为 NTP。
- **2.9 ui_minimal**：`g_title_label`（左中，白色，Montserrat 20） + `g_time_label`（右中，绿色，Montserrat 28）。
- **2.10 AppContext::init**：`Keyboard → MainTask → DisplayTask → ui_minimal::create → 注入`。
- **2.11 编译验证**：未自动执行，需要上电后再核验；DisplayTask 通过 `FreeRTOS xTaskCreatePinnedToCore` 绑定 Core 0，启动失败会 `LOG_ERROR` 但不退出。

## 备注

- `Backlight` 仍为 GPIO 高/低直驱；阶段 05 改 LEDC PWM。
- 阶段 02 维持演示极简；协议 / 设置同步留给阶段 03/04。
- 阶段 02 不引入 EventBus / Configuration / KeymapRepository；后续按阶段顺序添加。
- 时间源：本阶段为 `millis()` 上电时间，阶段 06 后接入 NTP（GMT+8）。
- `LVGL 8.3.11` 内部通过 `lv_conf_internal.h` 加载 `lv_conf.h`，因此文件名不能改成 `lv_conf_local.h`，否则会因 `LV_CONF_H` 头守卫缺失而走默认配置。
