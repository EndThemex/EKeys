# 阶段 09 — UI 主题系统

> **本文件已迁移到 [`theme-spec/`](./theme-spec/) 子目录**（2026-09-17 拆分）。
>
> 拆分为 3 份规格说明 + 1 份入口索引，便于按"概述 / 架构 / 计划"分块阅读与维护。原文件 617 行 / 10 大节全部内容已迁移，下方仅为跳转导航。

| 文件                                                             | 内容                                                                                                         | 对应原章节             |
| ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ | ---------------------- |
| [`theme-spec/README.md`](./theme-spec/README.md)                 | 入口索引 / 决策记录 / 阅读建议 / 依赖                                                                        | —                      |
| [`theme-spec/1-overview.md`](./theme-spec/1-overview.md)         | 目标、设计原则、状态色运行时收口、变更记录                                                                   | §1 / §2 / §3.7.3 / §10 |
| [`theme-spec/2-architecture.md`](./theme-spec/2-architecture.md) | 目录结构、类型、注册表、动画双收口、紧凑主页、共享样式与懒应用、入口接入、设置屏范围、旋钮输入路由、内存评估 | §3.1 ~ §3.10 / §4      |
| [`theme-spec/3-execution.md`](./theme-spec/3-execution.md)       | 切换延迟、风险与对策、测试矩阵、实施步骤、主题候选总览                                                       | §5 ~ §9                |

---

## 快速跳转

- 阶段目标 / 设计取舍 → [`theme-spec/1-overview.md`](./theme-spec/1-overview.md)
- 目录、类型、注册表、双收口、紧凑主页、样式应用 → [`theme-spec/2-architecture.md`](./theme-spec/2-architecture.md)
- 延迟、风险、测试、步骤、主题候选 → [`theme-spec/3-execution.md`](./theme-spec/3-execution.md)

---

## 关联文档

- [`./05-ui-screens.md`](./05-ui-screens.md) — 11 屏 SquareLine 工程、状态条、`ui_settings_request_*`
- [`./03-config-persistence.md`](./03-config-persistence.md) — `tft_theme` 配置项持久化
- [`../PINOUT.md`](../PINOUT.md) — 硬件无触屏、428×142 横条屏约束
