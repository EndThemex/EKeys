# 宏通道播放（录制间隔）+ 文本 20ms 字符间隔 实施方案

## Context

现状两个缺口：

1. **宏通道有协议无实现**：`KeyMapping::macros_key`（5 步，"+" 分隔，[keymap_types.h](../../../src/utils/keymap_types.h)）在协议 0x05/0x06 与 keymap{N}.ini 中已透传存储，但 `KeyResolver` 从不播放（firePress 无宏分支）；App 端 `from_firmware_entry` 把宏串透传为 `Function` 仅为防丢失。
2. **文本通道无字符间隔**：`USBKeyboardImpl::type()` / `BLEKeyboardImpl::type()` 整串 `print()`，逐字符报告背靠背发送，主机吞键。

目标（用户已确认）：
- App 端录制宏（按键序列 + 每步间隔），经 0x06 下发，固件存储并回放；
- 启用现有 macro 通道（每键最多 5 步），播放逐步 press → 间隔 → release；
- 文本逐字符 20ms 间隔，**阻塞式可接受**（128 字符约 2.5s，RGB/扫描暂停已确认）。

## 协议编码（两端约定）

宏字符串 = 步骤串，步骤间 `+` 分隔（沿用现有 splitPlus/joinPlus，**协议字段与存储格式零改动**）。
每步 = `键名[:间隔ms]`：

- 键名只允许单键 token：`0xNN` 字面量 / 单字符 / 具名键（Enter 等）；**不能是 `Ctrl+c`**（`+` 是步骤分隔符，修饰键只能单独成步或经大写字母机制）；
- 间隔：无 `:ms` 后缀 → 默认 20ms；`0~60000` ms，越界视为畸形；
- Shift+字母（0x04~0x1D）编码为大写字母名（`A`），固件 `resolveSingleSegment`（KeyNameTable.cpp L174-189）对 `A`~`Z` 自动附 Shift；
- 示例：`0x04:80+A:120+0x28:50`。

解析规则（**严格解析**）：任一步畸形（空段、步数>5、间隔非数字/越界、键名不认识）→ 整体回退 `Function(原文)`，保证 GET→SET 往返不静默清设备配置。旧串 `Ctrl+c` 因 `Ctrl` 非单键名 → 回退 Function，与现状一致。

---

## 固件侧（EKeys）

### 1. 文本 20ms 字符间隔

- [IKeyboard.h](../../../src/output/IKeyboard.h)：新增 `inline constexpr uint8_t kTypeCharIntervalMs = 20;`
- [USBKeyboardImpl.cpp](../../../src/output/USBKeyboardImpl.cpp) `type()`（L164-171）：`print(text)` 改为逐字符 `g_usb_keyboard.write(c)`，字符间 `delay(kTypeCharIntervalMs)`（最后一字符后不加）；
- [BLEKeyboardImpl.cpp](../../../src/output/BLEKeyboardImpl.cpp) `type()`（L235-242）：同法改 `s_ble->write(c)`（BleKeyboard.h L162 已有 `write(uint8_t)`）。

### 2. 宏播放（KeyResolver）

[KeyResolver.cpp](../../../src/keymap/KeyResolver.cpp)：

- `firePress`（L124-162）：在 normal 分支之后新增宏分支（优先级 function > text > normal > macro）：`fk/tk/normal 全空且宏非空` 时逐步播放：
  - 每步解析：取最后一个 `':'`，右侧纯数字为间隔（clamp 0~60000），左侧为键名；无 `':'` 用默认间隔；
  - `resolveKeyWithModifier(键名)` → `keyboard.press(keycode, modifier)` → `delay(间隔)` → `keyboard.release(keycode)`；keycode=0 跳过该步；
  - 阻塞式 delay，与文本通道同一取舍（用户已确认）。
- `fireRelease`（L164-198）：宏分支 → 无 HID 动作，仅 `notifyLedEdge(keyId, false)`（播放已在 press 完成，与 text 通道同模式）。
- 层选择逻辑（press/release 的 fire_layer_ 分派）无需改动：宏只在单击层（FUN 组合层无宏通道，App 端禁选）。

### 3. 类型与注释

- [keymap_types.h](../../../src/utils/keymap_types.h)：`macros_key` 注释更新为"已实现播放：每步 `键名:间隔ms`，`+` 分隔"，新增 `inline constexpr uint32_t kMacroStepDefaultMs = 20;`
- [cmd_keymap.cpp](../../../src/protocol/commands/cmd_keymap.cpp)：文件头注释补宏格式说明（逻辑零改动，splitPlus/joinPlus 对 `:` 透明）；
- [KeymapRepository.cpp](../../../src/services/KeymapRepository.cpp)：仅文件头注释更新。

### 4. 设备 UI 标签（小改动）

[MainTask.cpp](../../../src/tasks/MainTask.cpp) `sendKeymapProfileUi` 的 layerSummary 目前不含宏通道，绑定宏的键在键映射屏显示"--"。补：宏通道按 normal 同款方式追加显示（`keyDisplayName` 逐步 + "+" 连接），不含间隔（屏宽有限）。

---

## App 端（EKeysApp）

### 1. protocol.rs

- L1291 后新增 `pub struct MacroStep { code: u16, shift: bool, interval_ms: u32 }` + `MACRO_MAX_STEPS=5`、`MACRO_DEFAULT_INTERVAL_MS=20`；
- `KeyAction` enum（L1278）新增 `Macro(Vec<MacroStep>)`；
- `label()`（L1353）：宏摘要（步骤名 "→" 连接，超 12 字符截断），`macro_step_display`：shift+字母 → 大写字母，否则 `hid_key_label`；
- `to_firmware_entry`（L1406）：`Macro` → `e.macro_ = 步骤.map(编码).join("+")`，始终带 `:ms`（往返无损）；
- `from_firmware_entry`（L1455-1460）：宏分支改调新函数 `parse_macro_string`（放在 `parse_normal_string` 附近）：
  - 每段 `split_once(':')`，键名过 `name_to_hid`（L1683，天然拒绝 Ctrl 等修饰名）；单字符大写字母 → `shift=true`；间隔空 → 默认 20ms，`parse::<u32>()` 且 ≤60000；
  - 任一步失败 / 步数>5 / 空段 → `Function(原文)`；
- L2102 `to_firmware_entries` 注释补：FUN 层不拷宏（固件无此通道，正确行为）；
- 测试：改 L3005-3014（`"a+b"` 现解析为 Macro，不再是 Function；回退用例改用 `"Ctrl+c"`）；新增宏往返、默认间隔、畸形回退、KeymapData 整表往返（Base 层绑宏；FUN 层宏被丢弃锁行为）共 5 个用例。

### 2. panel_keymap.rs

- `KeymapPanelState`（L44）新增 `macro_recording: Option<MacroRecording>`（steps / last_press / last_release / pending_shift / prev_mods）；
- `action_kind_index`（L2076）加 `Macro(_) => 6`；kinds 数组（L2034）加 "宏序列"；ComboBox（L2043）对 kind 6 且 FUN 层 `add_enabled(false)`；
- `default_for_kind`（L2711）加 kind 6（Keyboard/Combo 主键迁移为单步宏）；
- `edit_action_params`（L2436）新增 `6 =>` 分支：
  - 步骤行：键名 + `⇧` toggle（仅字母键 0x04~0x1D 启用）+ 间隔 DragValue（0~60000ms）+ 删除按钮（至少留 1 步）；
  - 添加步骤：复用 Chord 的 IconTextButton+ComboBox 模式（L2547-2579），遍历 `HID_KEY_CHOICES`；
  - 录制条（新函数 `macro_record_bar_ui`，视觉仿 L2326 琥珀 Frame）：开始/结束按钮，录制期间行内控件全部禁用，`capture_keyboard=true` 复用全局快捷键抑制（app.rs L307 机制）；
  - 录制状态机（新函数 `macro_record_step`，仿 `capture_chord_keys` L2218）：
    - 过滤 `repeat: true`；Esc（0x29）= 结束录制且不录为步骤；
    - 修饰键边沿：Shift 上升沿 → `pending_shift` 挂起（等字母键合并为大写步骤；空按 Shift 落为独立 0xE1 步）；Ctrl/Alt 上升沿 → 独立 0xE0/0xE2 步；Win 键 Windows 下不可见（现状限制，仅可下拉添加）；
    - 非修饰键按下：间隔 = `Instant::now() - last_press`（egui 事件无时间戳，帧粒度够用），push 步骤；达 5 步结束；
    - 末步收尾：末键 release 事件有记录则末步间隔改为按住时长，否则保留 press 间隔；
- 新增 i18n 文案：`keymap.kind.macro`、`keymap.macro.*`（base_only/step_interval/add/record/record_hint/max_steps 等，[i18n/keymap.rs](../../EKeysApp/src/i18n/keymap.rs) 中英双语）+ `proto.action.macro`（i18n/proto.rs）。注意 i18n 合并重复 key 会 panic，不得与现有冲突。

---

## 风险与约束

- **阻塞窗口**：宏播放（最长 5×60s）与文本（最长 2.5s）期间 MainTask 停摆（RGB/扫描暂停）——用户已确认可接受；App 端间隔上限 60000ms 与固件 clamp 对齐；
- **FUN 层静默丢宏**：固件 FUN 层无宏通道，App UI 禁选 + protocol.rs 注释 + 测试锁行为，三重保险；
- **非字母 Shift 丢失**（Shift+数字实录只记数字）：固件通道表达能力上限，文案注明即可；
- 现有测试 `firmware_key_entry_uses_macro_field`（L2768）及 `"a+b"` 断言需同步更新（预期行为变化）。

## 验证

- 固件：**不主动编译**（项目规则）；改动后人工核对 KeyResolver 播放路径 press/release 配对、`delay` 引用；
- App：实现完成后运行 `cargo test`（protocol.rs 宏往返用例）；真机联调流程：App 录制宏 → 下发 0x06 → 重新上电 → 触发按键验证节奏复现；文本键验证逐字符 20ms 无吞键（USB/BLE 各测一次）。
