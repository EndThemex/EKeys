# 阶段 05 — UI 屏扩展

> 状态：代码完成（5.10 编译验证 / 硬件联调待执行）
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

- [x] **5.1 SquareLine 工程导入**：把 `src/ui/ui.cpp / ui.h / ui_events.cpp / ui_events.h` 作为 SquareLine Studio 生成文件；自定义代码放 `src/ui/ui_helpers.*`（StatusBar 更新 / 设置屏反向同步）、`src/ui/ui_StatusBar.*` 与 `src/ui/ui_bridge.*`。
- [x] **5.2 屏幕路由**：`DisplayTask` 持有当前屏幕 ID；提供 `navigateTo(UI_SCREEN_xxx)` API；接收来自 MainTask 的 `DisplayMessage::NAVIGATE`（本阶段可占位）。
- [x] **5.3 `ui_StatusBar`**：实现 `updateStatusBar(DeviceSettings, HaStatusInfo, MusicPlayerInfo, AsrRecordingState, ModuleStatus)`，在 `DisplayTask::loop()` 中按需刷新。
- [x] **5.4 键映射二级屏**：显示 11 个键的当前 `function_key` / `normal_key`；按应用键 1~11 切换选中。
- [x] **5.5 音乐屏**：标题 / 艺人 / 当前歌词 / 进度条；订阅 `MusicPlayerInfo`。
- [x] **5.6 PC 状态屏**：按 `PcStatusInfo` 渲染锁键、网络、电源、性能四项。
- [x] **5.7 HA 屏**：按 `HaStatusInfo` 渲染 WiFi / TCP / 模块 / 语音状态。
- [x] **5.8 设置屏**：`LVGL` 控件修改后调用 `ui_settings_request_apply()` 投递到 `g_ui_settings_lock`；`MainTask` 通过 `consumeUiSettingsRequest()` 写回 `DeviceSettings`。
- [x] **5.9 设置屏二级页**：Profile 选择、WiFi 配置、RGB 调色板。
- [ ] **5.10 编译验证**：`pio run` 通过；11 屏通过旋钮或应用键可达。
- [x] **5.11 自检记录**：记录每个屏使用的字体、控件 ID 命名、刷新周期。

## 验收标准

- 旋钮在每屏都能导航（左 / 右 / 进入）。
- 主屏时间跳动；状态条 WiFi / TCP / 模式指示与日志输出一致。
- 设置屏修改亮度，UI 立即刷新（无需重启）。
- Profile 切换图标可显示内置 LVGL 符号（PNG 上传阶段 06 之后启用）。

## 变更记录

- 2026-08-31：阶段 05 代码完成。
  - **5.1**：从参考工程 `FunModularKeyboard`（SquareLine Studio 1.5.3 / LVGL 8.3.11）整体导入 54 个 `src/ui/*.c|h` 文件（11 屏 + ui_comp + ui_StatusBar + ui_helpers + ui_settings_types + 字体；该 UI 无图片资源引用）。`platformio.ini` 按参考工程裁剪未引用大字体（BebasNeue 32/64/80、FontCKJGT 32/40/48/64/80，`build_src_filter`）。
  - **5.2**：`DisplayTask::navigateTo(ui_screen_tag_t)` → `DisplayMessageType::Navigate` 消息 → `navigateNow()`（`lv_scr_load_anim` 无动画直切；占位实现，暂无调用方）。
  - **5.3**：状态条在 `ui_init()` 后置默认值，`SettingUpdate` 时刷新工作模式 / 音量；录音 / 模块状态消息路径就绪（数据源阶段 06）。
  - **5.4**：`MainTask::sendKeymapProfileUi()` 组装 11 键标签（`"K{i}:function"` 或 `"K{i}:a+s"`，空映射为 `--`）+ Profile 名 / 图标符号，经 `KEYMAP_PROFILE_UPDATE` 投递；图标走内置 `LV_SYMBOL_SETTINGS` 回退（PNG 上传阶段 06 后启用）。
  - **5.5~5.7**：`PcStatusInfo` / `HaStatusInfo` / `MusicPlayerInfo` 渲染路径就绪（消息类型与字段对齐参考工程），阶段 06 协议 / 网络模块接入后即可投递。
  - **5.8**：`ui_settings_request_apply()/save()`（C 链接，定义在 `MainTask.cpp`，`g_ui_settings_lock` spinlock 临界区）→ `MainTask::loop()` 消费 → `applyUiSettingsSnapshot()`：与 `parseConfigSetCommand` 相同的取值约束，经 `Configuration::mutateSettings()` 原子写入，`persist=true` 时逐键 `saveSetting()` 持久化；work_mode 变更走 `AppContext::applyWorkMode()`，Profile 变更走 `reloadKeymap()`，随后投递全量 `SETTING_UPDATE`（背光 / 状态条 / 设置屏快照立即刷新）。
  - **5.9**：设置屏二级页随 SquareLine 文件导入，Profile / WiFi / RGB 调色板控件与 `ui_settings_snapshot_t` 双向绑定（RGB 调色板提交索引字符串，与 `DeviceSettings.rgb_single_colar`（uint8 索引）对齐）。
  - **旋钮**：新增 `src/input/RotaryEncoder.{h,cpp}`（ESP32Encoder PCNT 半四分 + OneButton；引脚 `kPinEc11*`：SW=5 / A=6 / B=7），单击→ENTER、双击→ESC、旋转→LEFT/RIGHT（≥2 步去抖）；`platformio.ini` 新增 `ESP32Encoder@^0.10.2`、`OneButton@^2.6.1`。
  - **消息层**：`message_types.h` 载荷由 union 改为平铺结构（各 Info 结构带 NSDMI，放入 union 在 C++ 中非法）；`fillSettingPayload()` 统一 DeviceSettings → `ui_settings_snapshot_t` 转换（cmd_config / DisplayTask / MainTask 共用）。`DisplayMessage` 约 1.2KB，队列长度 10。
  - **时间**：主屏时间仍为 MainTask 1s 投递的 millis() 推算值（"HH:MM:SS"），DisplayTask 拆分到 `ui_LabelTime`（HH:MM）+ `ui_LabelSecond`（SS）；日期 / 周待阶段 06 NTP。
  - **5.11 自检**：字体使用 = `ui.h` 声明集（BebasNeue 14/16/24/28/36/48/86 + FontCKJGT 16/24/28，其余被 build*src_filter 裁剪）；控件 ID 命名沿用 SquareLine 约定（`ui_LabelXxx` / `ui_ButtonXxx` / `ui*<Screen>`）；刷新周期 = 状态条随消息即时刷新、主屏时间 1s、设置屏 apply 有 ~300ms 防抖定时器（生成代码内置）。
  - **注意**：`ui_minimal.{h,cpp}` 已删除（SquareLine UI 接管）；5.10 编译验证待执行。

## 备注

- SquareLine Studio 生成的 UI 文件不要手工编辑；如需修改，在 SquareLine 中改后重新导出。
- 屏幕切换动效由 LVGL 内置提供，本阶段不引入自定义动画。
- `ui_settings_request_*` 是 LVGL → MainTask 的唯一通道，避免在事件回调中直接调用 `Configuration`。
