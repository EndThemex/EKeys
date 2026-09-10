# 组合键触发功能实现计划（双 FUN 键 + 组合输出）

## Context

用户需要「两键组合触发」：按住 FUN 键再按其它键时，触发该键配置的**组合输出**；FUN 键本身不产生任何 HID 输出；其它键在 FUN 未按住时走现有的**单击输出**。单击输出已有 function/text/normal 三种通道，用户确认组合输出也要**全通道**支持。FUN 键的指定方式用户确认为**设备端设置二级页配置**，并追加要求**支持配置两个 FUN 键**（任一按住即激活组合态）；同时按用户原始要求，App 协议（0x05/0x06）同步支持新的配置方式（组合通道 + fun_key1/fun_key2 读写）。

触发语义（写进协议文档）：
- 任一 FUN 键**先按住**，其它键在 FUN 按住期间按下 → 触发该键组合输出；松开该键 → 释放组合输出。
- 其它键先按下（已触发单击输出）后再按 FUN → 不追加组合，保持单击状态。
- FUN 键单独按下/松开无任何 HID 输出（RGB 点击高亮仍生效，走 KeyEventDispatcher，不受影响）。
- `fun_key1=0 且 fun_key2=0` 表示关闭组合功能，所有键按单击行为，完全向后兼容。
- **FUN1 与 FUN2 是两个独立的组合层**：FUN1 按住时其它键触发该键的 combo1 通道输出，FUN2 按住时触发 combo2 通道输出；两者同时按住时 FUN1 层优先。
- FUN 键为**全局设置**（config.ini [system] fun_key1/fun_key2，各 0~11，0=未配置），不按 Profile 区分——与设置二级页的全局设置快照管道一致，改动最小；两个 FUN 键允许配同一个键（运行时 held 标志独立，行为无害）。

## 数据模型

### `src/utils/keymap_types.h` — KeyMapping 增加组合通道

```cpp
struct KeyMapping
{
    String function_key;
    String text_key;
    std::array<String, kKeyMappingNormalCount> normal_key;
    std::array<String, kKeyMappingMacrosCount> macros_key;
    /* FUN1 组合层：fun_key1 按住时触发，优先级 function > text > normal */
    String combo1_function_key;
    String combo1_text_key;
    std::array<String, kKeyMappingNormalCount> combo1_normal_key;
    /* FUN2 组合层：fun_key2 按住时触发，同上优先级 */
    String combo2_function_key;
    String combo2_text_key;
    std::array<String, kKeyMappingNormalCount> combo2_normal_key;
    bool valid;
};
```

`kDefaultKeyMapping` / `keymapFillDefaults` 不变（组合通道默认空）。

### `src/config/DeviceSettings.h` — 新增全局字段

```cpp
/* FUN 组合键 1/2：0=未配置，1~11=对应物理键作为 FUN 键（system 节持久化） */
uint8_t fun_key1;
uint8_t fun_key2;
```

## 存储层

### `src/services/KeymapRepository.cpp`

- `loadProfile`：`[keyN]` 段新增读取 `combo_function_key` / `combo_text` / `combo_normal_key`（"+" 分隔，复用 splitPlusImpl）；`m.valid` 计算加入组合通道非空判断；段缺失=默认的判定条件保持「四个旧键全缺失」，若段内只有组合键也视为已配置（不回落默认）。
- `saveKeys`：写出三个新键（空串也写，保持显式清空语义）。

### `src/config/Configuration.cpp`

- `loadGlobalSettings`：`GetLongValue("system", "fun_key1", 0)` / `GetLongValue("system", "fun_key2", 0)`（沿用既有「不提前 return」修复模式，缺失时默认 0 生效）。
- `sectionOfKey`：`"fun_key1"` / `"fun_key2"` → `"system"`。

## 运行时

### `src/keymap/KeyResolver.h/.cpp` — 核心触发逻辑

新增状态与接口：

```cpp
uint8_t funKey1() const;         // 读 Configuration settings().fun_key1
uint8_t funKey2() const;         // 读 Configuration settings().fun_key2
void resetState();               // 两个 fun held 标志、fire_layer_ 全清（reloadKeymap 时调用）
bool fun1_held_;                 // FUN 键 1 当前按住
bool fun2_held_;                 // FUN 键 2 当前按住
uint8_t fire_layer_[kMatrixKeyCount + 1]; // 该键本次按下走的层：0=单击 1=FUN1 组合 2=FUN2 组合
```

重构 press/release：抽出私有辅助 `pressChannels(keyId, fk, tk, nk, kb)` / `releaseChannels(...)`（现 press/release 中 function > text > normal 的循环逻辑原样搬入，含既有 LED notify 行为，单击/两个组合层复用）。

- `press(keyId)`：
  - `funKey1()!=0 && keyId==funKey1()` → 置 `fun1_held_=true`，直接 return（无 HID）；FUN 键 2 同理置 `fun2_held_`；
  - `fun1_held_` 且该键 combo1 层非空 → `pressChannels(combo1 三通道)`，`fire_layer_[keyId]=1`；
  - 否则 `fun2_held_` 且该键 combo2 层非空 → `pressChannels(combo2 三通道)`，`fire_layer_[keyId]=2`（FUN1 层优先）；
  - 否则走原单击逻辑（`fire_layer_[keyId]=0`）。
- `release(keyId)`：
  - FUN 键 1/2 → 清对应 held 标志，return；
  - 按 `fire_layer_[keyId]` 释放对应层的三通道并清标志（组合触发后即使 FUN 先松开，仍等该键松开时释放，配对正确）；
  - 否则原单击释放逻辑。
- 对同一物理键的 FUN press 幂等（重复置位无害），支持同一 5ms tick 预处理。

### `src/tasks/MainTask.cpp`

- 5ms tick 按键段（约 L321）：pressed 循环前加预扫描——若 `resolver_.funKey1()` 或 `funKey2()`（非 0）在本轮 pressed 列表中，先对命中的 FUN 键调 `resolver_.press(funKey, *keyboard_)` 置位 FUN 状态，解决「FUN 与其它键同一 tick 按下、扫描顺序靠后」时组合不触发的问题。
- `reloadKeymap()`：追加 `resolver_.resetState()`，避免改配置时 FUN/组合状态卡住。
- 键映射屏标签（`sendKeymapProfile` 约 L391-422）：构建完单击摘要后，若该键组合通道非空，追加 `|<组合摘要>`（组合摘要同样按 function > text > normal 优先级拼串，snprintf 截断保护，缓冲 24 字节）。

### `src/tasks/MainTask.cpp` — `applyUiSettingsSnapshot`

- `s.fun_key1 = clampInt(s.fun_key1, 0, 11)`，fun_key2 同理；`mutateSettings` 中写入并参与 changed 判断（持久化走既有 saveSetting 流程）。

## 设置二级页 UI

### `src/ui/ui_settings_types.h`

`ui_settings_snapshot_t` 增加 `int32_t fun_key1; int32_t fun_key2;`。

### `src/message_types.h`

`fillSettingPayload` 增加 `out.fun_key1 = s.fun_key1; out.fun_key2 = s.fun_key2;`。

### `src/ui/ui_SettingScreenSecondary.c`

按既有 14 项的模式新增两项 `SETTING_ITEM_FUN_KEY1` / `SETTING_ITEM_FUN_KEY2`（插在 `SETTING_ITEM_PROFILE` 之后）：

1. 枚举两项（`SETTING_ITEM_UI_LANG` 之前插入，`SETTING_ITEM_COUNT` 自动 +2，滚动/焦点逻辑已按 COUNT 计算）；
2. `setting_secondary_item_name`：中英文案（"FUN键1"/"FUN键2" / "Fun Key 1"/"Fun Key 2"，跟该函数既有双语风格）；
3. `setting_secondary_item_value_text`：0 显示"关闭"/"Off"，1~11 显示"按键N"/"Key N"；
4. `setting_secondary_adjust_value`：各 0~11 循环步进（两项独立调整，不做互斥校验，运行时相同值无害——fun1/fun2 held 标志独立）；
5. `setting_secondary_snapshot_equal` 与 `s_setting_edit` 初始化补字段。

## App 协议（`docs/desktop-app-protocol.md` §7.1 + `src/protocol/commands/cmd_keymap.cpp`）

### 0x05 GET 响应

```json
{
  "cmd": 133, "seq": 1, "status": 0,
  "fun_key1": 1, "fun_key2": 0,
  "keymap": [
    { "physical": 2, "normal": "b", "macro": "", "text": "", "function": "",
      "combo_function": "", "combo_text": "", "combo_normal": "Ctrl+Shift+c" }
  ]
}
```

- 顶层新增 `fun_key1` / `fun_key2`（各 0~11，0=未配置）；
- 每键新增 `combo_function` / `combo_text` / `combo_normal`（"+" 分隔，同 normal 规则）。

### 0x06 SET 请求

- `data.fun_key1` / `data.fun_key2`：**可选** int 0~11，出现时校验范围并经 `Configuration` 持久化（saveSetting("fun_key1", v) 等）；缺省保持现值；两者配相同非 0 值允许（运行时 held 标志独立，行为无害），不做互斥校验；
- 每键可选 `combo_function` / `combo_text` / `combo_normal`：组合通道内部优先级 function > text > normal（与单击通道同样的 SET 解析顺序），与单击通道**互相独立**（运行时按 FUN 是否按住选择，不做跨组互斥）；
- `combo_text` 同样 ≤128 字符截断。

### 协议文档更新要点（§7.1）

- 上述字段与示例；触发语义四条（FUN 先按、组合优先、FUN 键无输出、双 0 关闭兼容）；注明 FUN 键也可在设备端「设置二级页 → FUN键1/FUN键2」配置。

### 明确不做

- `fun_key1/fun_key2` 不接入 0x08 FieldMask（build_config_payload 等四处同步暂不动），App 经 0x05/0x06 读写即可；
- 组合不支持 macro 通道（macro 本身未实现）；
- 不主动编译（项目规则），需用户明确要求后再 `pio run`。

## 改动文件清单

| 文件 | 改动 |
| --- | --- |
| src/utils/keymap_types.h | KeyMapping 加组合三通道 |
| src/config/DeviceSettings.h | 加 fun_key1/fun_key2 字段 |
| src/config/Configuration.cpp | loadGlobalSettings 读双 fun_key + sectionOfKey |
| src/services/KeymapRepository.cpp | loadProfile/saveKeys 读写组合键 |
| src/keymap/KeyResolver.h/.cpp | 双 FUN 键 held 状态、pressChannels/releaseChannels 重构、组合触发 |
| src/tasks/MainTask.cpp | 同 tick 预扫描（双 FUN 键）、reloadKeymap 重置状态、键映射屏标签、applyUiSettingsSnapshot |
| src/ui/ui_settings_types.h | 快照加 fun_key1/fun_key2 |
| src/message_types.h | fillSettingPayload 加字段 |
| src/ui/ui_SettingScreenSecondary.c | 新增 FUN键1/FUN键2 设置项（名称/取值/步进/比较/初始化） |
| src/protocol/commands/cmd_keymap.cpp | 0x05 GET/0x06 SET 增字段 |
| docs/desktop-app-protocol.md | §7.1 协议与语义 |
| data/keymap1.ini | 头注释补组合键示例说明 |

## 验证方式（联调，用户执行）

1. 用户明确要求后编译 `pio run`，确认 0 诊断。
2. 设备端：设置二级页新增「FUN键1」「FUN键2」项，旋钮把 FUN键1 调到 1（键 1 为 FUN）；App 0x05 读回 fun_key1=1、fun_key2=0。
3. App 0x06 写 `{"physical":2,"normal":"b","combo_normal":"Ctrl+c"}` → 主机单击键 2 输出 b；按住键 1 再按键 2 触发复制；松开后单击恢复正常。
4. 双 FUN 键：fun_key2 配 3 后，按住键 3 同样触发键 2 的组合输出。
5. 组合文本：`combo_text` 写一段 ASCII，FUN+键整串输出。
6. 边界：双 fun_key=0 时全键单击正常；FUN 键本身按键无输出；设置二级页内矩阵键仍被截胡不触发 HID。
7. 旧配置兼容：不刷 data 分区直接升级固件后，原 keymap ini 缺组合键 → 单击行为不变。
