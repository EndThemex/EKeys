# 项目规则

主控是esp32-s3 n16r8.
不主动编译项目，除非我明确要求。
引脚定义请参考 `PINOUT.md`。
引脚编号从 0 开始，与 ESP32-S3 原始引脚定义一致。
引脚编号与引脚功能对应关系请参考 `PINOUT.md`。
遇到经常出现的问题，请记录下来避免重复出现。
实现功能时，只做必要的修改，避免引入新的问题。
代码需要根据功能和层级进行组织，避免代码结构混乱。
代码需要考虑代码的可读性和可维护性，单个文件不要放置过多代码。
参数配置最好用单独的文件完成，方便后续检查和修改。
新增方法时，不要忘记在头文件中声明。
APP端项目在当前项目同级目录下，项目名称为 `EKeysApp`。

## 输入交互规则（旋钮 / 矩阵键 / 设置二级页）

硬件无触屏（[PINOUT.md](file:///d:/search/esp/Keys/EKeys/PINOUT.md) 无 TP*\* 引脚，[LvglPort.cpp](file:///d:/search/esp/Keys/EKeys/src/display/LvglPort.cpp) 未注册 `lv_indev*\*`），系统内仅两类物理输入。

### 1. 输入源语义

- **EC11 旋钮**：仅 UI 导航，不进 HID 键映射
  - 顺时针 → `LV_KEY_RIGHT`
  - 逆时针 → `LV_KEY_LEFT`
  - 单击 → `LV_KEY_ENTER`（进入 / 确认）
  - 双击 → `LV_KEY_ESC`（返回）
- **矩阵 11 键**（应用键 ID 1~11）：HID 输出专用，在 KEYMAPPED 屏被截胡为焦点跳转

### 2. LVGL 键值归一化

所有 UI 屏幕只识别 6 种语义，编码来自 `lv_group.h`：

| 语义     | LVGL 编码                         | 旋钮   | 矩阵键 1~11         | 触屏兜底按钮   |
| -------- | --------------------------------- | ------ | ------------------- | -------------- |
| LEFT     | `LV_KEY_LEFT` (20)                | 逆时针 | —                   | `ButtonLeft*`  |
| RIGHT    | `LV_KEY_RIGHT` (19)               | 顺时针 | —                   | `ButtonRight*` |
| UP       | `LV_KEY_UP` (17)                  | —      | key_id=7            | —              |
| DOWN     | `LV_KEY_DOWN` (18)                | —      | key_id=11           | —              |
| ENTER    | `LV_KEY_ENTER` (10)               | 单击   | —                   | `ButtonEnter*` |
| ESC      | `LV_KEY_ESC` (27)                 | 双击   | —                   | `ButtonExit*`  |
| 焦点跳转 | `action = 100 + key_id` (101~111) | —      | 全程以 101~111 编码 | —              |

矩阵键 `key_id`（1~11）以 `kMatrixKeyActionBase(100) + key_id` 编码进 `ActionInput.action`，与 `LV_KEY_*` 数值完全不重叠（2026-09-08 修复：原裸传 key_id 时 `LV_KEY_ENTER=10` 与矩阵键 10 冲突，主页按按键 10 会被当成旋钮单击进入键映射屏；2026-09-11 修复：进 UI 的 `LV_EVENT_KEY` 也统一携带 101~111 编码，UI 侧经 `UI_MATRIX_KEY_ACTION_BASE`（[ui_settings_types.h](file:///d:/search/esp/Keys/EKeys/src/ui/ui_settings_types.h)）解码，禁止裸传 key_id）。

### 3. DisplayTask 路由规则

[`DisplayTask::applyMessage(ActionInput)`](file:///d:/search/esp/Keys/EKeys/src/tasks/DisplayTask.cpp#L281-L334) 是旋钮 / 矩阵键的唯一改写点：

- **矩阵键（101~111）**：
  - **KEYMAPPED 屏**：截胡 → 跳 `KEYMAPPED_SECONDARY` 并把 `key_id` 作为焦点键传 UI
  - **KEYMAPPED_SECONDARY / SETTING_SECONDARY 屏**：编码后的 `action`（101~111）透传 `LV_EVENT_KEY`，UI 侧解码（焦点跳转 / 矩阵键 7、11 移焦点）
  - **其它屏**：丢弃，不触发 UI 导航（矩阵键为 HID 专用）
- **旋钮动作（`LV_KEY_*`）**：`lv_event_send(active_screen, LV_EVENT_KEY, action)` 透传；主页单击（ENTER）= 无操作
- 禁止在 UI 屏幕内部再把 `LV_KEY_LEFT/RIGHT` 改写为 `LV_KEY_UP/DOWN`，所有改写集中在 DisplayTask

### 4. SettingScreenSecondary 行为契约

[`setting_secondary_handle_key`](file:///d:/search/esp/Keys/EKeys/src/ui/ui_SettingScreenSecondary.c#L655-L695) 当前分发规则：

| 输入             | 行为                                         |
| ---------------- | -------------------------------------------- |
| 旋钮顺时针       | 当前项数值 +1（亮度+5%、模式递增、开关取反） |
| 旋钮逆时针       | 当前项数值 -1                                |
| 旋钮单击/双击    | ENTER/ESC                                    |
| 矩阵键 7         | 焦点上移一项（首项跳末项）                   |
| 矩阵键 11        | 焦点下移一项（末项跳首项）                   |
| 矩阵键 1~6、8~10 | no-op（不发 HID，不切焦点）                  |

`LV_KEY_UP/DOWN` 分支保留但当前无物理输入触发，等价于死代码，清理时一并删除。

### 5. 二级页 HID 屏蔽

[`MainTask::loop()` 5ms tick](file:///d:/search/esp/Keys/EKeys/src/tasks/MainTask.cpp#L251-L278) 在 `UI_SCREEN_SETTING_SECONDARY` 下屏蔽矩阵键 HID 派发：

- `KeyEventDispatcher::onKeyEdge(pressed, true)` 与 `resolver_.press(...)` 被跳过
- `ActionInput` 投递不屏蔽（UI 仍需 key_id 做焦点跳转）
- `released` 循环保持原样：二级页内被屏蔽的 press 对应的 release 派发到 HID 是无副作用的，离开二级页后所有 release 正常派发避免卡键

### 6. 触屏兜底按钮

SquareLine 生成的 `ButtonLeft* / ButtonRight* / ButtonEnter* / ButtonExit*` 在本硬件上**永远不会被触发**（无触屏驱动）。处置方案：

- **保留**：未来加触屏扩展时无需重写
- **移除**：精简代码体积，按上一轮讨论的"方案 A"（仅注释 `lv_obj_add_event_cb`）或"方案 B"（删除按钮对象）执行

按"只做必要修改"原则，**保留**更稳妥。

### 7. 跨任务读 `g_active_screen_tag`

`ui_get_active_screen_tag()` 由 DisplayTask 单写、MainTask 单读，类型 `ui_screen_tag_t`（uint8_t），8 位读写天然原子无锁安全。若后续改为双核同时读写，需加 atomic 或临界区。
