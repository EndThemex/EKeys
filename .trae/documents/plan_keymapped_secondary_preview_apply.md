# 键映射二级页：Profile 预览 → 单击应用 改造计划

## Context

当前在 KEYMAPPED_SECONDARY 屏旋钮转动会立即切换并持久化 profile（switchActiveProfile → SPIFFS 原子写 + reloadKeymap），链路同步阻塞导致界面响应慢。改为三段式交互：

1. **旋钮旋转** → 只"预览"目标 profile（键位标签 / 名称 / 图标 / 序号），不应用不落盘
2. **旋钮单击（ENTER）** → 应用当前预览的 profile，先显示"应用中..."等待遮罩，applied 消息到达后消失
3. **已应用标记**：显示的 profile == 已应用 profile 时，序号 "N/8" 格子红框(0xD33A31)加粗 + 序号/名称文字红色；预览未应用的显示默认灰色

状态单一事实源：MainTask 侧 `g_preview_profile_index`（预览目标）；UI 侧 `s_applied_profile_index`（已应用标记）。消息协议复用 `KeymapProfileInfo`，新增字段区分预览/已应用。

## 改动步骤（按实施顺序）

### 1. `src/message_types.h`
`KeymapProfileInfo` 增加字段：
- `uint8_t profile_index{0};` —— 本消息描述的 profile（0~7）；applied 消息中恒等于 active_profile
- `bool is_preview{false};` —— true=预览消息；默认 false=已应用语义（漏设字段时退化为现状行为，更安全）

### 2. `src/config/Configuration.h/.cpp`
新增公开方法 `bool loadProfileKeyMapping(uint8_t idx, KeymapArray &out)`：`repo_` 空或 idx 越界返回 false，否则 `repo_->loadProfile(getProfileConfigPath(idx), out)`。注释注明：返回 false 时 out 未填充，调用方须用 `keymapFillDefaults(out)`（[utils/keymap_types.h](file:///d:/search/esp/Keys/EKeys/src/utils/keymap_types.h) L76）兜底，与 `KeyResolver::begin()` 回退语义一致。

### 3. `src/tasks/MainTask.h`
- `sendKeymapProfileUi(uint8_t)` 改返回 `bool`
- 新增 `bool sendKeymapProfilePreview(uint8_t fun_layer);`
- 新增 `void pushCurrentKeymapView(uint8_t fun_layer);`（tick 内按屏幕分流）
- 新增成员 `bool keymap_preview_ui_pending_{false};`

### 4. `src/tasks/MainTask.cpp`（核心状态机）

**匿名 namespace**：
- `PendingProfileSwitchRequest` 扩展为 `{pending, step, apply_pending, apply_target}`
- 新增 `uint8_t g_preview_profile_index = 0;`（extern "C" 桥接需直接读，与 `g_profile_switch_request` 同模式）
- 抽出单键格式化函数 `formatKeymapLabel(const KeyMapping &, uint8_t fun_layer, char *out, size_t cap)`（现 sendKeymapProfileUi 内 layerSummary lambda 逻辑迁入）。注意：**不要**按整表 `KeymapArray` 传参抽取——KeyMapping 含约 20 个 String，整表拷贝会放大堆分配

**桥接函数**：
- `ui_keymap_request_profile_switch(step)`：逻辑不变，注释语义改为"预览切换"
- 新增 `extern "C" bool ui_keymap_request_profile_apply(void)`：临界区内 `apply_pending = true; apply_target = g_preview_profile_index;`（目标在请求瞬间捕获，避免同窗口迟到的 step 干扰）

**`begin()`**：`config.load()` 后 `g_preview_profile_index = config.activeProfile();`

**`loop()` 消费段**（替换现 [MainTask.cpp L277-302](file:///d:/search/esp/Keys/EKeys/src/tasks/MainTask.cpp#L277-L302)）：
1. 临界区取走 `{step, apply_pending, apply_target}`
2. apply 分支：`apply_target != activeProfile()` → `switchActiveProfile(apply_target)` + `reloadKeymap()`；相等 → 仅置 `keymap_ui_pending_ = true`（省一次 SPIFFS 原子写，**applied 消息必须照发**供 UI 收尾遮罩）
3. applied 消费：`if (sendKeymapProfileUi(fun_ui_layer_)) keymap_ui_pending_ = false;`（post 失败不清标志，下轮重试）
4. step 分支：`g_preview_profile_index` 环绕 ±step → 置 `keymap_preview_ui_pending_ = true`（不直接 post）；加最小消费间隔节流（约 100ms 时间戳比较），限制连转时的 SPIFFS INI 解析频率
5. preview 消费：pending 且间隔到 → `sendKeymapProfilePreview(fun_ui_layer_)` 成功才清标志；每轮只发最新 idx，快速旋转天然合并

**`sendKeymapProfileUi` 改造**：返回 `postMessage` 结果；填 `p.profile_index = snap.active_keymap_profile`；标签循环改调 `formatKeymapLabel`；**尾部 resync `g_preview_profile_index = snap.active_profile`** —— 这是 App 端 cmd_config / 设置屏反向同步 / 0x06 applyKeymap 三条外部切 profile 路径的唯一兜底汇聚点（全部经 `keymap_ui_pending_` 进来）。

**新增 `sendKeymapProfilePreview`**：`loadProfileKeyMapping(g_preview_profile_index, map)` 失败则 `keymapFillDefaults(map)`；填 `profile_index` / `active_profile = config.activeProfile()` / `is_preview = true` / name=`getProfileDisplayName(idx)` / icon=`LV_SYMBOL_SETTINGS`；11 键循环 `formatKeymapLabel`。

**FUN 层推屏分流**（[MainTask.cpp L461-480](file:///d:/search/esp/Keys/EKeys/src/tasks/MainTask.cpp#L461-L480)）：两处 `sendKeymapProfileUi(fun_layer)` 改调 `pushCurrentKeymapView(fun_layer)`：
```
tag == UI_SCREEN_KEYMAPPED_SECONDARY → sendKeymapProfilePreview()（失败置 keymap_preview_ui_pending_）
其它 → sendKeymapProfileUi()
```
否则在二级页按 FUN 会推出 applied 消息冲掉预览态。

**`postMessage`** 返回 `void` → `bool`（透传 xQueueSend 结果，现有调用点不改）。

### 5. `src/tasks/DisplayTask.cpp` — `applyKeymapProfile` 分流

按 `p.is_preview` 分两条路径（[DisplayTask.cpp L451-512](file:///d:/search/esp/Keys/EKeys/src/tasks/DisplayTask.cpp#L451-L512)）：

- **预览消息**：fileName 用 `p.profile_index` 构造；`ui_KeyMappedSecondary_set_profile(icon, name, fileName, /*update_main_summary=*/false)`；`set_fun_keys` 照常；**跳过** SPIFFS.exists 图标检查与两个 `set_profile_icon_image_data` 调用（省最贵的 SPIFFS 探测，且避免一级屏图标被预览污染）；11 个 `set_key_label` + `lv_refr_now` 照常
- **已应用消息**：先 `ui_KeyMappedSecondary_hide_apply_waiting()` + `ui_KeyMappedSecondary_set_applied_index(p.profile_index + 1)`；fileName / icon path 改用 `p.profile_index`；其余不变

### 6. `src/ui/ui_KeyMappedSecondary.h`
```
void ui_KeyMappedSecondary_set_profile(const char *icon, const char *name,
                                       const char *file_name, bool update_main_summary);
void ui_KeyMappedSecondary_show_apply_waiting(void);
void ui_KeyMappedSecondary_hide_apply_waiting(void);
void ui_KeyMappedSecondary_set_applied_index(unsigned int index); /* 1~8，0=未知 */
bool ui_keymap_request_profile_apply(void);
```
`set_profile` 唯一调用方是 DisplayTask，签名变更安全。

### 7. `src/ui/ui_KeyMappedSecondary.c`

- 新增 static：`s_applied_profile_index`（1~8，0=未知，屏幕重建不清零）、`s_apply_in_progress`、遮罩对象指针、兜底 lv_timer 指针
- **已应用标记** `keymapped_secondary_apply_applied_marker()`：`s_applied_profile_index == s_keymapped_secondary_profile_index` 时 → `ui_KeyMappedSecondaryProfileIndex` 格子 border 红 0xD33A31 / 宽 2，序号 label 与 `ui_KeyMappedSecondaryProfileName` 文字红；否则回默认灰（格子 0x2B3442/宽 1，文字 0x8FA0B5）。**注意**：ProfileName 创建时硬编码红色（L636）需改为默认灰，红色只由 marker 动态设置。调用点：`apply_cached_profile()` 尾部、`screen_init` 末尾、`set_applied_index`
- **主屏 summary 抑制**：`apply_cached_profile()` 加 `bool sync_main` 参数，`s_keymapped_secondary_main_screen_*` 三段写入包进条件；`bind_main_screen_summary` 与 `screen_init` 传 true
- **等待遮罩**：`screen_init` 在所有子对象之后建全屏 `lv_obj`（bg 0x070A0F / opa LV_OPA_70 / 默认 HIDDEN），居中 label "应用中..." 用 `ui_font_FontCKJGT16`（该字体已在本屏使用）。参照 [ui_SettingScreenSecondary.c L200/L224](file:///d:/search/esp/Keys/EKeys/src/ui/ui_SettingScreenSecondary.c#L200-L224) 的 lv_timer 模式：show 时创建 3s 一次性兜底 timer，超时强制 hide（防 applied 消息丢失后遮罩永挂）；hide 时 `lv_timer_del`；timer cb 内 NULL guard；`screen_destroy` 补 timer del + 指针置空 + `s_apply_in_progress = false`
- **dispatch 修改**（`keymapped_secondary_dispatch_key`）：
  - `LV_KEY_ENTER`：`s_apply_in_progress` 防重入 → `show_apply_waiting()` + `ui_keymap_request_profile_apply()`；请求失败（MainTask 未就绪）则回滚隐藏
  - `LV_KEY_LEFT/RIGHT`：调用不变，注释改"预览切换"
  - `LV_KEY_ESC`：**不拦截**——apply 在 MainTask 后台必然完成，遮罩由 applied 消息或 3s 兜底关闭
  - 101~111 焦点跳转：不变

## 关键风险结论

- **外部切 profile 脱节**：三条外部路径全汇聚到 `keymap_ui_pending_` → `sendKeymapProfileUi` 尾部 resync 覆盖，无需改 parseConfigSetCommand
- **队列满丢消息**：applied 走 pending 重试、preview 合并发最新、UI 3s 兜底，三重防护
- **同帧 apply+step**：apply_target 请求时捕获 + applied 先入队、preview 后入队，终态 = 屏幕预览 == ENTER 将应用的目标
- **lv_timer 生命周期独立于对象**：destroy 必须 del

## 验证（手动，不主动编译）

1. 旋钮连转多格 → 只预览不落盘（config.ini 的 active_keymap_profile 不变），显示最终预览、灰色未应用样式
2. 单击 → "应用中..." 出现 → 消失 → 序号格子/名称变红；重复单击被拦截
3. 应用后 ESC 返回一级屏，主屏 summary 显示的是已应用 profile（预览不污染）
4. 预览中 App 下发 profile 切换 → 预览被取消，显示新激活 profile 并标红
5. 二级页按住 FUN → 组合层标签按预览中的 profile 显示
6. HID 回归：矩阵键 1~11 焦点跳转、键 7/11 移焦点、离开二级页无卡键
7. 等待遮罩 3s 兜底：临时注释 applied 消息发送验证超时消失后还原
