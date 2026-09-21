# 3. 计划

> 本文为 [`./README.md`](./README.md) 拆分三块之三：切换延迟评估 + 风险与对策 + 测试矩阵 + 实施步骤 + 主题候选总览。
> 目标、原则、状态色运行时收口、变更记录 → [`1-overview.md`](./1-overview.md)。
> 目录结构、类型、注册表、双收口、紧凑主页、样式应用、入口接入、设置屏范围、旋钮路由、内存评估 → [`2-architecture.md`](./2-architecture.md)。

---

## 3.1 切换延迟评估

| 动作                                     | 耗时                         |
| ---------------------------------------- | ---------------------------- |
| 多页内切主题（单屏 apply，懒应用）       | ~10ms                        |
| 紧凑 ↔ 多页互斥切换（建/销 CompactMain） | ~50ms                        |
| 紧凑主页页名切换（改文本 + 130ms 淡入）  | 130ms（动画时长，非阻塞）    |
| 进后台屏补刷（navigateNow 顺带）         | ~10ms，用户不可感知          |
| `lv_refr_now` 重绘                       | ~30ms                        |
| **用户感知**                             | 全部路径 < 100ms（动画除外） |

---

## 3.2 风险与对策

| 风险                                           | 触发条件                           | 对策                                                                                                                                                                                                      |
| ---------------------------------------------- | ---------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SquareLine 重导出覆盖 `ui_helpers.c`           | 用户在 SquareLine 中修改并重新导出 | 改动只有 §2.4.1 一段（顶部 include + 函数体前 10 行），文件头注释 `// STAGE_09_CUSTOM: 主题动画接管点，重导时合并此段`；`ui.c`/其余屏文件零改动（相比原方案取消 ui.c 追加，重导出面从 2 个文件缩到 1 个） |
| 卡片判定误伤（透明遮罩、状态栏底板等）         | 遍历按 bg_opa 判定                 | `bg_opa != LV_OPA_COVER` 跳过；实施时逐屏目检截图                                                                                                                                                         |
| label 统一 text_primary 后个别对象对比度不足   | 深底上的 dim 文本被刷成白色        | 接受（v1 取舍）；个别对象由运行时 setter 用 `text_dim` 显式覆写（[`1-overview §1.3`](./1-overview.md#13-状态色运行时收口必要配套改动) 表扩展）                                                                                                                          |
| 状态色 setter 替换遗漏                         | 实施时搜索不全                     | 全局搜 `0x22C55E|0xF59E0B|0xEF4444|0xFF0000` 清单化替换（§1.3），验收时再搜一遍确认清零 |
| `navigateNow` 时序变化引入回归                 | ensure_applied 插在 load_anim 前   | ensure_applied 只做样式挂载，不碰 tag/timer；测试矩阵覆盖矩阵键进二级页 / 自动回主页路径                                                                                                                  |
| `tft_theme` 历史 config.ini 存有裸 id（0/2/3） | 老设备升级                         | 加载后 clamp：`ui_theme_get` 回落 UI_THEME_DEFAULT；默认值 0 改 1（[`2-architecture §2.9`](./2-architecture.md#29-设置屏范围)）                                                                                                                                 |
| 紧凑主页时间列与 ui_FlipClock 更新路径分叉     | TimeUpdate 双入口                  | applyMessage 单点分发：`is_compact_home ? ui_compact_main_set_time() : ui_FlipClock_update()`                                                                                                             |
| 紧凑主页布局与硬件 LCD 分辨率强耦合            | 换屏                               | 三栏比例按 `LV_HOR_RES/LV_VER_RES` 动态取值；本项目锁定 428×142                                                                                                                                           |
| BebasNeue 无 CJK 字形，页名中文渲染空白        | 页名 label 误用 BebasNeue          | 页名 label 固定 `FontCKJGT28`（教训：BebasNeue 仅 0x20-0x7f）                                                                                                                                             |

---

## 3.3 测试矩阵

| 用例                                  | 预期                                                                                    |
| ------------------------------------- | --------------------------------------------------------------------------------------- |
| 启动默认主题（多页）                  | 13 屏按 `kThemes[UI_THEME_DEFAULT]` 渲染；旋钮导航动画为 FADE_IN 200ms（原来恒为 NONE） |
| 设置屏改主题（多页内）                | 当前屏立即变色；切到其它屏时补刷变色；切回旧主题无残留色                                |
| 上位机 `CMD_CONFIG_SET tft_theme=100` | `ui_theme_apply_now(100)`：建 CompactMain 并加载；旋钮切页名；ENTER 直达对应二级页      |
| 紧凑主页旋钮顺/逆时针                 | 页名下/上切换（130ms 淡入），首尾钳位                                                   |
| 紧凑主页 ENTER                        | 直达 `kCompactPages[s_page_idx].target` 二级屏，tag 同步为对应 `UI_SCREEN_*_SECONDARY`  |
| 紧凑主题二级页 ESC                    | `_ui_screen_change` 重定向回紧凑主页（不是一级屏）                                      |
| 紧凑主题 5s 无操作                    | 自动回紧凑主页（非 MainScreen）                                                         |
| 紧凑主题下设置二级页改主题为多页主题  | CompactMain 销毁、回 MainScreen；主题入口可用，无需工厂重置                             |
| 紧凑主题下矩阵键 1~11                 | HID 输出正常；UI 侧无导航副作用（MAIN tag 落"其它屏丢弃"分支）                          |
| 持久化                                | `tft_theme` 写入 `config.ini`，重启后形态与配色恢复；历史非法 id 回落默认               |
| 内存水位                              | `DisplayTask` 池监控无 OOM；峰值 < 140KB                                                |
| 状态色跟主题                          | 改主题后 WiFi/电量/HA 状态值/PC 指示条颜色随之变化                                      |

---

## 3.4 实施步骤

| 步骤 | 内容                                                                                                                                           | 风险           |
| ---- | ---------------------------------------------------------------------------------------------------------------------------------------------- | -------------- |
| 1    | 建 `src/ui/theme/` 8 文件 + `kThemes[]`（UI_THEME_DEFAULT + UI_THEME_COMPACT_DARK）+ 稀疏 id 遍历工具                                          | 无             |
| 2    | `ui_theme_style.c` 共享样式构建 + 遍历挂载/卸载 + 懒应用脏标记                                                                                 | 中（卡片判定） |
| 3    | 写 `ui_compact_main.c`（428×142 三栏页名选择器）+ `ui_compact_menu.c` 页名切换                                                                 | 中（手写布局） |
| 4    | 改 `ui_helpers.c::_ui_screen_change`：主题 anim 覆写 + 紧凑主题一级屏重定向（唯一 SquareLine 改动）                                            | 低             |
| 5    | 改 `DisplayTask`：`navigateNow` anim 覆写 + MAIN 重定向 + ensure_applied；`applyMessage` 紧凑主页输入拦截 + ThemeSwitch 消息 + TimeUpdate 分发 | 中（路由改动） |
| 6    | `MainTask`：SettingUpdate 消费处 `tft_theme` 变更 → `ui_theme_request`；`DisplayMessage` 加 `theme_id` 字段                                    | 低             |
| 7    | 改 `ui_SettingScreenSecondary.c`：范围校验 + 名称查表（[`2-architecture §2.9`](./2-architecture.md#29-设置屏范围)）                                                                                  | 低             |
| 8    | 状态色 setter 清单化替换（[`1-overview §1.3`](./1-overview.md#13-状态色运行时收口必要配套改动) 表）                                                                                                          | 低（逐处替换） |
| 9    | `Configuration.cpp` tft_theme 默认值/clamp；`platformio.ini` `build_src_filter` 加 `+<theme/>`                                                 | 低             |
| 10   | 编译验证（按规则不主动编，需触发）                                                                                                             | —              |

每步独立可回滚。步骤 2（样式判定规则）与 3（手写紧凑主页）风险最高。

---

## 3.5 主题候选总览

| ID  | 名称             | 形态         | 调色板定位                | 动画（切屏/页名）           |
| --- | ---------------- | ------------ | ------------------------- | --------------------------- |
| 1   | 暗夜玻璃（默认） | 多页         | 深蓝黑 + 半透明卡片       | FADE_IN 200ms / —           |
| 2   | 暗夜纯净         | 多页         | 纯黑卡片，无边框          | NONE 0ms / —                |
| 3   | 浅色极简         | 多页         | 米白底 + 灰描边           | MOVE_LEFT 250ms / —         |
| 4   | 高对比无障碍     | 多页         | 黑底 + 亮黄/亮青          | FADE_OUT_TOP 150ms / —      |
| 5   | AMOLED 极黑      | 多页         | `#000000` 全黑 + 1px 分隔 | OVERVIEW 300ms / —          |
| 100 | 紧凑 暗夜        | **紧凑主页** | 左时间/中页名/右状态灯    | FADE_IN 200ms / 淡入 130ms  |
| 101 | 紧凑 浅色        | 紧凑主页     | 米白底 + 蓝选             | FADE_IN 250ms / 淡入 130ms  |
| 102 | 紧凑 高对比      | 紧凑主页     | 黑底 + 亮黄               | OVERVIEW 200ms / 淡入 100ms |

> 紧凑主题仅在 100 段位预留，区间 1~99 给多页主题。
> 字体不在主题差异范围内：当前 LVGL 编译进 Montserrat + 3 个 BebasNeue + 3 个 FontCKJGT（BebasNeue 仅 0x20-0x7f 无符号/CJK 字形），切主题不能换字体。
