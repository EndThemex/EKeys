# 1. 概述

> 本文为 [`./README.md`](./README.md) 拆分三块之一：目标 + 设计原则 + 状态色运行时收口 + 变更记录。
> 目录结构、类型、注册表、双收口、紧凑主页、样式应用、入口接入、设置屏范围、旋钮路由、内存评估 → [`2-architecture.md`](./2-architecture.md)。
> 切换延迟、风险、测试矩阵、实施步骤、主题候选 → [`3-execution.md`](./3-execution.md)。

---

## 1.1 目标

把当前散落在 16 个 `src/ui/*.c` 中约 200 处硬编码颜色 (`lv_color_hex(0x…)`，实测 198 处)、硬编码切屏动画集中到一张**主题注册表**中。新增 / 修改主题只改一张表，不动任何屏文件、不动 `ui_settings_types.h`、不动 `LV_EVENT_KEY` 路由。

支持两种根本不同的交互形态并存：

- **多页主题**（如默认暗夜玻璃）：13 屏全开放，旋钮=屏间导航，菜单项 = 屏幕标签
- **紧凑主页主题**：主页（`ui_MainScreen`）被替换为紧凑主页 `ui_CompactMain`——左时间/日期/星期，中间显示当前页名（一次一项），右状态灯区；旋钮顺/逆时针切换页名，ENTER 直达该页名对应的**二级页**（真实切屏），二级页内 ESC / 5s 自动回主页回到紧凑主页。一级屏（KeyMapped/Music/Audio/PcStatus/Ha/Setting）在紧凑主题下成为"只过渡不驻留"的屏：用户不经过它们，但对象保留并参与主题覆写

---

## 1.2 设计原则

| 原则                    | 含义                                                                                                                                                      |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SquareLine 屏文件零侵入 | 13 个 `ui_*.c` 一字不改；主题 boot 挂在 [`../../src/tasks/DisplayTask.cpp`](../../src/tasks/DisplayTask.cpp) 的 `ui_init()` 之后（项目自有代码，SquareLine 永不重写）     |
| 数据驱动                | 加主题只改 `kThemes[]` 一张表 + `ui_theme_id_t` enum                                                                                                      |
| 动画跟主题走            | 多页主题 `anim_*` 三字段切屏；紧凑主题另有 `page_anim_*` 页名切换动画                                                                                     |
| 共享样式挂载/卸载       | 每主题构造一组共享 `lv_style_t`，遍历屏对象树 `lv_obj_add_style` / `lv_obj_remove_style`；切主题=卸旧挂新，**天然无残留**，且内存远小于逐对象 local style |
| 懒应用                  | 切主题只刷当前屏（`navigateNow` 收口），后台屏打脏标记，进入时补刷                                                                                        |
| 只覆写颜色类样式        | v1 只覆写 bg / text / border / radius；**不覆写 pad**——428×142 上布局逐像素调过（翻页钟卡片、80×80 键位格），统一 pad 会破版                              |
| 状态色运行时化          | WiFi/电量/READY/RECORDING/PC 指示条等运行时 setter 改引用 `ui_theme_color_*` API，不依赖静态遍历                                                          |
| 单页/多页互斥           | 一台设备同一时刻只能激活紧凑或多页之一；切换时创建/销毁 `ui_CompactMain`                                                                                  |
| tag 路由不变            | 紧凑主页复用 `UI_SCREEN_MAIN` tag（g_active_screen_tag 仍为 MAIN），`is_compact_home` 区分两种主页；不加新枚举、不动矩阵键路由                            |

---

## 1.3 状态色运行时收口（必要配套改动）

静态遍历管不住、且会反向覆盖静态遍历的运行时 setter，全部改为引用主题取色 API：

| 文件                                                                     | 硬编码点                                 | 改为                                                                                     |
| ------------------------------------------------------------------------ | ---------------------------------------- | ---------------------------------------------------------------------------------------- |
| [`../../src/ui/ui_StatusBar.c`](../../src/ui/ui_StatusBar.c)                             | WiFi 灰/白/橙、电量红/灰（L172-L218）    | `ui_theme_color_*`：灰=`text_dim`、白=`text_primary`、橙=`state_warn`、红=`state_danger` |
| [`../../src/ui/ui_HaScreenSecondary.c`](../../src/ui/ui_HaScreenSecondary.c)             | WiFi/TCP/Voice/ModuleA/B 状态值 L81-L125 | 同上（`state_ok`/`state_warn`/`state_danger`）                                           |
| [`../../src/ui/ui_PcStatusScreenSecondary.c`](../../src/ui/ui_PcStatusScreenSecondary.c) | 网络点、CPU/磁盘指示条 L310-L432         | 同上                                                                                     |
| [`../../src/ui/ui_MainScreen.c`](../../src/ui/ui_MainScreen.c)                           | profile 名橙色 L194                      | `ui_theme_color_accent()`（映射 accent_yellow）                                          |

此表是**枚举清单**：实施时全局搜索 `lv_color_hex(0x22C55E)|0xF59E0B|0xEF4444|0xFF0000` 逐处替换，替换后这些对象的颜色自动跟主题走。其余约 150 处静态 `lv_color_hex`（创建时写死的底色/描边）由共享样式遍历接管，不改屏文件。

> 取色 API 列表见 [`2-architecture.md §3.2`](./2-architecture.md#32-公共类型) 末尾。

---

## 1.4 变更记录

- 2026-09-16：阶段 09 SPEC 创建。基于用户决策：
  - 主题套数 = 不预设
  - 动画 = 跟主题走
  - 覆盖策略 = 后序遍历
  - 接管点 = 改 `ui_helpers.c` 内部
  - 切换范围 = 默认 13 屏全参与
  - 单页紧凑主题 = 完全隐藏 13 屏 + 竖排菜单 + ENTER 展开详情面板
- 2026-09-16：**按代码实测修订（本轮）**。核对代码后修正以下与事实不符/不可行的设计：
  - **紧凑主题形态重定义**（用户拍板）：428×142 横条屏放不下竖排 5 行菜单（5×36=180px > 142px）；"ENTER 展开详情面板复用 SquareLine 屏对象"与 `g_active_screen_tag` 路由冲突（面板覆盖时 `lv_scr_act()` 仍是主页，键事件到不了面板）。改为**紧凑主页页名选择器**：中间一次一项页名，旋钮切换，ENTER 经 `navigateNow` 直达二级页（真实切屏），ESC/自动回主页重定向回紧凑主页；tag 复用 `UI_SCREEN_MAIN`，不加新枚举
  - **动画接管点补全**：原方案只改 `ui_helpers.c`，漏掉 `DisplayTask::navigateNow`（矩阵键进二级页/Navigate/自动回主页都直接 `lv_scr_load_anim`）——改为双收口（详见 [`2-architecture.md §3.4`](./2-architecture.md#34-动画接管双收口)）
  - **应用策略改为共享 `lv_style_t` 挂载/卸载**：原 local style 逐对象覆写在 ~500 对象上约 30~50KB，128KB pool 承受不住且与"样式池 6KB"的内存评估自相矛盾；共享样式 <15KB 且无残留。补懒应用（切主题只刷当前屏）
  - **删除 `lv_obj_get_name` / strcmp 文本匹配**：LVGL 8.3 无 `lv_obj_get_name` API，编译不过；文本匹配被运行时 setter 反向覆盖。改为状态色统一走 `ui_theme_color_*` API + setter 清单化替换
  - **取消 pad 覆写**：428×142 布局逐像素调过（翻页钟、80×80 键位格），统一 pad_all 会破版
  - **`ui_theme_count()` 修正**：稀疏 enum（100 段位）下 `sizeof/sizeof` 会算出 ~101 套，改为遍历统计 + `ui_theme_id_at/index_of` 双向映射
  - **入口从 `ui.c` 移到 `DisplayTask::run()`**：`ui.c` 是 SquareLine 整文件重写产物，"重导出不触碰 ui_init 末尾"不成立；boot 挂项目自有代码永不需合并
  - **紧凑主题下设置屏可达**：页名表含"设置"，原"单页主题只能工厂重置"约束取消
  - 其它：`DisplayMessage` 平铺加 `theme_id` 字段（无 union，`msg.payload.*` 写法错误）；config.ini `tft_theme` 默认 0（sentinel）改 1 并 clamp；紧凑主题切屏动画字段恢复有效；页名中文用 FontCKJGT（BebasNeue 无 CJK）
- 2026-09-17：文档拆分。原 `docs/09-ui-theme-system.md`（617 行）按"概述/架构/计划"三大块拆分到 `docs/theme-spec/`：
  - `1-overview.md`（本文件）：目标、原则、状态色收口、变更记录
  - `2-architecture.md`：目录结构、类型、注册表、双收口、紧凑主页、样式应用、入口接入、设置屏范围、旋钮路由、内存评估
  - `3-execution.md`：切换延迟、风险、测试矩阵、实施步骤、主题候选
  - 原 `docs/09-ui-theme-system.md` 改写为指向本目录的索引页
