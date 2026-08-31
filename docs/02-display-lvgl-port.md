# 阶段 02 — 显示迁移 + LVGL 最小主屏

> 状态：未开始
> 关联章节：[`FEATURE_DOC.md §8.1`](../FEATURE_DOC.md) / [`§16`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.9`](../ARCHITECTURE.md)

## 目标

把 `src/main.cpp` 中现有的 NV3007 / LVGL 初始化代码迁移到 `src/display/`，并接入第二颗 FreeRTOS 任务 `DisplayTask`（Core 0）。屏幕上呈现一个最小主屏（标题 + 时间），验证双任务与队列通路。

## 范围

1. 引入 `include/lv_conf_local.h`，把 LVGL 关键开关集中到本地覆盖。
2. 新增 `src/display/` 与 `src/tasks/DisplayTask.*`。
3. 引入 `src/message_types.h` 与 `src/tasks/MainTask` 中的 `DisplayMessage` 队列。
4. 主屏只包含一个标题与时间标签；不接 SquareLine 输出。

## 前置条件

- 阶段 01 已完成，按键 → HID 已稳定。
- `lib/GFX Library for Arduino/` 已存在并能初始化 NV3007。

## 任务清单

- [ ] **2.1 `include/lv_conf_local.h`**：覆盖 `LV_COLOR_DEPTH=16`、`LV_MEM_CUSTOM=0`、`LV_FONT_MONTSERRAT_20` / `LV_FONT_MONTSERRAT_28` 启用；关闭未用 demo。
- [ ] **2.2 `src/hardware/PinMap.h`**：从 `PINOUT.md §1.1` 抽出全部 IO 定义到单一头文件，供后续模块引用。
- [ ] **2.3 `src/display/Backlight.h/.cpp`**：封装 `LCD_BL`（`PINOUT §2.6`）的 PWM 输出；默认 duty = 80%。
- [ ] **2.4 `src/display/DisplayDriver.h/.cpp`**：把当前 `main.cpp` 中 `gfx / bus` 创建逻辑搬入；提供 `begin()` / `fillScreen(uint16_t)` / `draw16bitRGBBitmap()`。
- [ ] **2.5 `src/display/LvglPort.h/.cpp`**：搬迁 `lvgl_display_init` / `lvgl_tick_cb` / `my_disp_flush`；提供 `lvglPortInit(Arduino_GFX*, uint16_t w, uint16_t h)`。
- [ ] **2.6 `src/message_types.h`**：定义 `enum class DisplayMessageType : uint8_t { ... }` 与 `struct DisplayMessage { DisplayMessageType type; union { ... }; }`，仅声明本期需要的 `SETTING_UPDATE` 与 `TIME_UPDATE`。
- [ ] **2.7 `src/tasks/DisplayTask.h/.cpp`**：在 Core 0 创建 `xTaskCreatePinnedToCore`；循环执行 `lv_tick_inc` / `lv_timer_handler`；阻塞等待 `DisplayMessage` 队列。
- [ ] **2.8 `src/tasks/MainTask`**：增加 `xQueueHandle displayQueue_`；每 1000ms `xQueueSend` `TIME_UPDATE`。
- [ ] **2.9 `src/ui/ui_minimal.h/.cpp`**：编写一个最小主屏（`lv_obj_t * ui_scr_main()`），含标题标签 + 时间标签；不引入 SquareLine。
- [ ] **2.10 `src/main.cpp`**：移除所有 LVGL / 显示屏相关代码；改为 `DisplayDriver::begin()` → `LvglPort::init()` → `ui_minimal::create()` → 创建 `MainTask` / `DisplayTask`。
- [ ] **2.11 编译验证**：`pio run` 通过；上电屏幕显示标题与秒级跳动的时间。
- [ ] **2.12 自检记录**：在 `变更记录` 记录 LVGL 缓冲区大小与 `LV_COLOR_16_SWAP` 设置。

## 验收标准

- 上电 1.5s 内屏幕显示主屏；时间标签每 1s 更新。
- 主任务按键 HID 不受显示任务影响；两个任务运行 5 分钟无看门狗复位。
- `src/main.cpp` 行数 ≤ 60。
- 编译产物（`.pio/build/esp32-s3-wroom-1-n16r8/firmware.bin`）生成成功。

## 变更记录

- _暂无_

## 备注

- 本阶段不接 RGB / 音频 / 协议 / 配置，刻意保持极简。
- `Backlight` 的 PWM 占位是为了与阶段 05 的 `tft_brightness` 设置项对齐；当前写默认值即可。
- 若 LVGL 提示 `lv_conf.h not found`，检查 `include/lv_conf_local.h` 是否被 `build_flags` 中的 `-I include` 暴露。