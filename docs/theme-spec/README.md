# 阶段 09 — UI 主题系统 规格说明

> 状态：方案评审通过（2026-09-16 按代码实测修订），待编码
> 关联：[`../05-ui-screens.md`](../05-ui-screens.md) / [`../PROJECT_LAYOUT.md`](../PROJECT_LAYOUT.md)
>
> 决策记录：
>
> 1. 主题套数 = 不预设，`kThemes[]` 表驱动
> 2. 切屏动画 = 跟主题走（每套主题自带 `anim_fademode/spd/delay`）
> 3. 应用策略 = 后序遍历挂载/卸载**共享 `lv_style_t`**（非逐对象 local style 覆写），SquareLine 屏文件零改动
> 4. 动画实现点 = **双收口**：屏内事件切换改 `ui_helpers.c::_ui_screen_change` 内部（31 处调用不动）+ DisplayTask 发起切换在 `navigateNow` 覆写 anim 入参
> 5. 重渲范围 = 13 屏全部参与覆写；采用**懒应用**（切主题只刷当前屏，其余屏进入时在 `navigateNow` 补刷）
> 6. **紧凑单页主题 = 紧凑主页（页名选择器）**：主页替换为手写 `ui_CompactMain`（左时间 / 中间当前页名 / 右状态灯），旋钮切换页名，ENTER **真实跳转**到对应二级页（走 `navigateNow`，tag 正常同步）；ESC / 自动回主页回紧凑主页。13 屏对象保留、正常参与主题覆写
> 7. 单页紧凑主题与多页主题并存：互斥启用，由 `ui_theme_t.is_compact_home` 字段声明
> 8. 布局以 LVGL 实测分辨率 **428×142 横条屏**为准（[`../../src/display/LvglPort.cpp`](../../src/display/LvglPort.cpp) kScreenWidth/kScreenHeight），不做竖排菜单
> 9. 运行时状态色（WiFi/电量/READY/RECORDING 等）不走静态遍历，统一改引用主题取色 API

---

## 目录结构

原 `09-ui-theme-system.md`（617 行、10 大节）按"概述 / 架构 / 计划"三大块拆分到本目录：

| 文件 | 范围 | 对应原章节 |
| ---- | ---- | ---------- |
| [`1-overview.md`](./1-overview.md) | 目标、设计原则、状态色运行时收口、变更记录 | §1 / §2 / §3.7.3 / §10 |
| [`2-architecture.md`](./2-architecture.md) | 目录结构、公共类型、调色板注册表、动画双收口、紧凑主页、页名切换动画、共享样式与懒应用、入口接入、设置屏范围、旋钮输入路由、内存评估 | §3.1 ~ §3.10 / §4 |
| [`3-execution.md`](./3-execution.md) | 切换延迟评估、风险与对策、测试矩阵、实施步骤、主题候选总览 | §5 ~ §9 |

---

## 子目录（建议）

```
docs/theme-spec/
├── README.md              # 本文件：入口索引与导航
├── 1-overview.md          # 目标 / 原则 / 状态色 / 变更记录
├── 2-architecture.md      # 类型 / 注册表 / 收口 / 紧凑主页 / 样式 / 路由 / 内存
└── 3-execution.md         # 延迟 / 风险 / 测试 / 步骤 / 候选主题
```

不引入二级子目录；本目录只承载规格说明，不放代码与图片。

---

## 阅读建议

1. 先读 [`1-overview.md`](./1-overview.md) §1 ~ §2，建立整体目标与设计取舍。
2. 再读 [`2-architecture.md`](./2-architecture.md) §3.1 ~ §3.5，理解目录树、类型、注册表与紧凑主页形态。
3. 编码前补读 §3.7（共享样式与懒应用）和 §3.8（入口接入），这是改动面最大的两块。
4. 验收与排期参考 [`3-execution.md`](./3-execution.md)。

---

## 与其它文档的依赖

| 引用方 | 引用位置 |
| ------ | -------- |
| [`../05-ui-screens.md`](../05-ui-screens.md) | 11 屏 SquareLine 工程、状态条、`ui_settings_request_*`、SettingScreenSecondary 行为契约 |
| [`../03-config-persistence.md`](../03-config-persistence.md) | `tft_theme` 配置项持久化（默认值/落盘/clamp 在 §3.9） |
| [`../../PINOUT.md`](../../PINOUT.md) / [`../../src/display/LvglPort.cpp`](../../src/display/LvglPort.cpp) | 硬件无触屏、428×142 分辨率、横条屏布局约束 |
| 阶段 04 协议 `CMD_CONFIG_SET tft_theme` | 主题切换入口 |

---

## 变更记录

- 2026-09-17：文档拆分。原 `docs/09-ui-theme-system.md`（617 行）按"概述/架构/计划"三大块拆分到 `docs/theme-spec/`，原文件改写为跳转索引页。
