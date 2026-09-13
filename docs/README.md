# EKeys 文档索引

本目录汇总 `EKeys` 项目的专题文档，按主题分类。功能总览见 [`../FEATURE_DOC.md`](../FEATURE_DOC.md)；项目结构见 [`../ARCHITECTURE.md`](../ARCHITECTURE.md)；引脚定义见 [`../PINOUT.md`](../PINOUT.md)。

## 文档列表

| 文档                                                       | 主题                                                                 |
| ---------------------------------------------------------- | -------------------------------------------------------------------- |
| [desktop-app-protocol.md](./desktop-app-protocol.md)       | 桌面 App 通信协议（命令清单、字段、报文示例、连接流程、重连状态机）  |
| [COMPILING.md](./COMPILING.md)                             | 编译、烧录、SPIFFS 上传、串口监视、擦除 Flash                        |
| [PROJECT_LAYOUT.md](./PROJECT_LAYOUT.md)                   | 仓库目录速览 / 关键文件 / 文档体系                                   |
| [TROUBLESHOOTING.md](./TROUBLESHOOTING.md)                 | 硬件 / 软件注意事项与常见问题速查                                    |

## 项目规则

长期约束（如引脚定义、UI 输入语义、DisplayTask 路由、SettingScreenSecondary 行为契约、跨任务 `g_active_screen_tag` 原子性等）见 [`../.trae/rules/rules.md`](../.trae/rules/rules.md)。

## 阶段任务文档（已归档）

阶段 01 ~ 09 的实施计划与变更记录已并入 [`../FEATURE_DOC.md`](../FEATURE_DOC.md) / [`../ARCHITECTURE.md`](../ARCHITECTURE.md) 的"已完成项"小节，原 `docs/01-` ~ `docs/08-` 阶段文件已归档删除。

## 文档维护约定

- 改代码后**先**同步对应文档章节，再提交。
- 新增专题文档：在 `docs/` 添加并在此索引登记。
- 项目规则变更：仅修改 `../.trae/rules/rules.md`，不在专题文档中重复。
