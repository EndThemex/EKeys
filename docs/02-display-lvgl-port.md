# 阶段 02 — 显示迁移 + LVGL 最小主屏

> 状态：进行中（屏幕迁移代码已在阶段 01 落地，本阶段剩余：接入 `DisplayTask` 与最小主屏）
> 关联章节：[`FEATURE_DOC.md §8.1`](../FEATURE_DOC.md) / [`§16`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.9`](../ARCHITECTURE.md)

## 目标

> **阶段 01 已经预先把屏幕驱动迁移完成**：NV3007 / LVGL / 背光均已搬到 `src/display/`，原 `src/main.cpp` 中的屏幕代码只剩"调用"。
>
> 阶段 01 末 demo 主屏（NV3007 标题 + 428x142 + ESP32-S3 + LVGL + TEST 按钮）已按用户要求删除，`src/ui/ui_minimal.cpp` 现为空实现，屏幕默认显示纯黑（背光常亮）。
>
> 因此阶段 02 本阶段的剩余目标调整为：
>
> 1. 把 `LvglPort::tick()` 从 `loop()` 移交到 Core 0 的 `DisplayTask`。
> 2. 重新激活 `ui_minimal`，引入含标题 + 时间标签的主屏。
> 3. 引入 `DisplayMessage` 队列作为 MainTask → DisplayTask 的唯一通道。
> 4. 阶段 02 内不必再次迁移屏幕驱动代码。

## 范围

1. 引入 `include/lv_conf.h`，把 LVGL 关键开关集中到本地覆盖。
2. **新增 `src/tasks/DisplayTask.*`**，把 LVGL tick、屏幕刷新调度统一到此。
3. 引入 `src/message_types.h` 与 `MainTask` 中的 `DisplayMessage` 队列。
4. 主屏包含一个标题与**实时的时间标签**（替代阶段 01 的静态文本）。

## 前置条件

- 阶段 01 已完成：按键 → HID 已稳定；屏幕驱动代码已在 `src/display/`。
- `lib/GFX Library for Arduino/` 已存在并能初始化 NV3007。

## 任务清单

### 已在阶段 01 完成（无需重复实现）

- [x] **2.2 `src/hardware/PinMap.h`** —— 阶段 01 已引入（[01-minimal-hid.md §1.12](./01-minimal-hid.md)）。
- [x] **2.3 `src/display/Backlight.h/.cpp`** —— 阶段 01 已引入。
- [x] **2.4 `src/display/DisplayDriver.h/.cpp`** —— 阶段 01 已引入。
- [x] **2.5 `src/display/LvglPort.h/.cpp`** —— 阶段 01 已引入；当前仅由 `loop()` 调用 `LvglPort::tick()`。
- [x] **2.9 `src/ui/ui_minimal.h/.cpp`** —— 阶段 01 已引入静态标题屏；阶段 01 末因用户要求删除 demo UI 元素，`create()` 暂为空实现。本阶段需要重新启用并升级为含时间标签。

### 本阶段 02 仍需落地的任务

- [x] **2.1 `include/lv_conf.h`**：覆盖 `LV_COLOR_DEPTH=16`、`LV_COLOR_16_SWAP=0`、`LV_MEM_CUSTOM=0`（48 KB）、`LV_FONT_MONTSERRAT_20/28` 启用、关闭 5 个未用 demo、`LV_USE_LOG=0`。`platformio.ini` 同步追加 `-DLV_CONF_INCLUDE_SIMPLE` 与 `-I include`。**文件名固定为 `lv_conf.h`**（见 [`01-minimal-hid.md §1.17`](./01-minimal-hid.md)）。
- [ ] **2.6 `src/message_types.h`**：定义 `enum class DisplayMessageType : uint8_t { SETTING_UPDATE, TIME_UPDATE, ... }` + `union` payload。本期仅声明两种。
- [ ] **2.7 `src/tasks/DisplayTask.h/.cpp`**：在 Core 0 创建 `xTaskCreatePinnedToCore`；循环 `lv_tick_inc` / `lv_timer_handler`；阻塞 `DisplayMessage` 队列。
- [ ] **2.8 `src/tasks/MainTask`**：增加 `xQueueHandle displayQueue_`；每秒推送 `TIME_UPDATE`。
- [ ] **2.9 `src/ui/ui_minimal`**：重新激活 `ui_minimal::create()`，加入标题 + 时间标签（含 NTP 时间戳）；`DisplayTask` 根据 `TIME_UPDATE` 更新。
- [ ] **2.10 `src/main.cpp`**：移除直接调用 `LvglPort::tick()`；改为 `DisplayTask::begin()` 后，`loop()` 只跑 `MainTask`。
- [ ] **2.11 编译验证**：`pio run` 通过；上电屏幕显示标题与秒级跳动的时间。
- [ ] **2.12 自检记录**：LVGL 缓冲区大小与 `LV_COLOR_16_SWAP` 设置。

## 验收标准

- 上电 1.5s 内屏幕显示主屏；时间标签每 1s 更新。
- 主任务按键 HID 不受显示任务影响；两个任务运行 5 分钟无看门狗复位。
- `src/main.cpp` 行数 ≤ 60。
- `LvglPort::tick()` 不在 `loop()` 中调用（仅 DisplayTask 内部调用）。
- 编译产物（`.pio/build/esp32-s3-wroom-1-n16r8/firmware.bin`）生成成功。

## 变更记录

- **阶段 01 前提起完成的内容**：2.2~2.5、2.9 占位主屏骨架已先在本文件之前由阶段 01 落地；详见 [`01-minimal-hid.md` §1.12](./01-minimal-hid.md) "阶段 01 同步落地的屏幕代码"。
- **2.7 DisplayTask**：`xTaskCreatePinnedToCore(..., 0, ...)`；栈 8192；优先级 1。
- **2.6 DisplayMessage**：union 预留扩展键映射 / 状态条 / HA / 音乐等消息，本期仅 `SETTING_UPDATE / TIME_UPDATE`。
- **2.11 时间刷新**：`MainTask` 每 1000ms 投递 `TIME_UPDATE`；`DisplayTask` 收到后调用 `ui_minimal::setTimeLabel()`。

## 备注

- 阶段 02 不引 RGB / 音频 / 协议 / 配置，刻意保持极简。
- `Backlight` 的 PWM 占位是为了与阶段 05 的 `tft_brightness` 设置项对齐；当前写默认值即可。
- 由于阶段 01 已经预先落出屏幕驱动，阶段 02 实质工作集中在：① 队列 + 任务 ② 时间 UI；其它项不再做迁移。
- 若 LVGL 提示 `lv_conf.h not found`，检查 `include/lv_conf.h` 是否被 `build_flags` 中的 `-I include` 暴露。
