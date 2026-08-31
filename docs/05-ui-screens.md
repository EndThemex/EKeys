# 阶段 05 — UI 屏扩展

> 状态：未开始
> 关联章节：[`FEATURE_DOC.md §8.1`](../FEATURE_DOC.md) / [`§12`](../FEATURE_DOC.md) / [`§13`](../FEATURE_DOC.md) / [`§14`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.9`](../ARCHITECTURE.md)

## 目标

补齐 11 屏 LVGL UI（`ui_SCREEN_MAIN / KEYMAPPED / KEYMAPPED_SECONDARY / MUSIC / MUSIC_SECONDARY / PC_STATUS / PC_STATUS_SECONDARY / HA / HA_SECONDARY / SETTING / SETTING_SECONDARY`），并实现状态条 `ui_StatusBar` 与设置反向同步通道 `ui_settings_request_*`。

## 范围

1. 接入 SquareLine Studio 工程（`src/ui/ui.cpp` / `ui.h` / `ui_events.cpp` 等），保证不再手工改动由生成器管理的文件。
2. 引入状态条组件 `ui_StatusBar`，订阅 `DisplayMessage::SETTING_UPDATE / MODULE_STATUS / HA_STATUS_UPDATE / MUSIC_PLAYER_UPDATE / ASR_RECORDING_STATE / PC_STATUS_UPDATE`。
3. 引入 `src/ui/ui_helpers.cpp`，暴露 `ui_settings_request_apply()` / `ui_settings_request_save()` 供 LVGL 事件回调调用。

## 前置条件

- 阶段 02 已完成，DisplayTask 与 LVGL 端口可用。
- 阶段 04 已完成，`SETTING_UPDATE` 可被 DisplayTask 接收。

## 任务清单

- [ ] **5.1 SquareLine 工程导入**：把 `src/ui/ui.cpp / ui.h / ui_events.cpp / ui_events.h` 作为 SquareLine Studio 生成文件；自定义代码放 `src/ui/ui_helpers.*`（StatusBar 更新 / 设置屏反向同步）、`src/ui/ui_StatusBar.*` 与 `src/ui/ui_bridge.*`。
- [ ] **5.2 屏幕路由**：`DisplayTask` 持有当前屏幕 ID；提供 `navigateTo(UI_SCREEN_xxx)` API；接收来自 MainTask 的 `DisplayMessage::NAVIGATE`（本阶段可占位）。
- [ ] **5.3 `ui_StatusBar`**：实现 `updateStatusBar(DeviceSettings, HaStatusInfo, MusicPlayerInfo, AsrRecordingState, ModuleStatus)`，在 `DisplayTask::loop()` 中按需刷新。
- [ ] **5.4 键映射二级屏**：显示 11 个键的当前 `function_key` / `normal_key`；按应用键 1~11 切换选中。
- [ ] **5.5 音乐屏**：标题 / 艺人 / 当前歌词 / 进度条；订阅 `MusicPlayerInfo`。
- [ ] **5.6 PC 状态屏**：按 `PcStatusInfo` 渲染锁键、网络、电源、性能四项。
- [ ] **5.7 HA 屏**：按 `HaStatusInfo` 渲染 WiFi / TCP / 模块 / 语音状态。
- [ ] **5.8 设置屏**：`LVGL` 控件修改后调用 `ui_settings_request_apply()` 投递到 `g_ui_settings_lock`；`MainTask` 通过 `consumeUiSettingsRequest()` 写回 `DeviceSettings`。
- [ ] **5.9 设置屏二级页**：Profile 选择、WiFi 配置、RGB 调色板。
- [ ] **5.10 编译验证**：`pio run` 通过；11 屏通过旋钮或应用键可达。
- [ ] **5.11 自检记录**：记录每个屏使用的字体、控件 ID 命名、刷新周期。

## 验收标准

- 旋钮在每屏都能导航（左 / 右 / 进入）。
- 主屏时间跳动；状态条 WiFi / TCP / 模式指示与日志输出一致。
- 设置屏修改亮度，UI 立即刷新（无需重启）。
- Profile 切换图标可显示内置 LVGL 符号（PNG 上传阶段 06 之后启用）。

## 变更记录

- _暂无_

## 备注

- SquareLine Studio 生成的 UI 文件不要手工编辑；如需修改，在 SquareLine 中改后重新导出。
- 屏幕切换动效由 LVGL 内置提供，本阶段不引入自定义动画。
- `ui_settings_request_*` 是 LVGL → MainTask 的唯一通道，避免在事件回调中直接调用 `Configuration`。