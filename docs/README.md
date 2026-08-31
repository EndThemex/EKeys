# EKeys 任务计划索引

本目录存放从 [`../ARCHITECTURE.md`](../ARCHITECTURE.md) 拆分的实施任务计划。每个阶段对应一份 `NN-xxx.md`，按顺序推进；每完成一项即在该文件中勾选状态。

## 阶段列表

| 阶段 | 文档                                                             | 目标简述                               | 依赖  |
| ---- | ---------------------------------------------------------------- | -------------------------------------- | ----- |
| 01   | [01-minimal-hid.md](./01-minimal-hid.md)                         | 按键矩阵 → USB HID 键盘                | —     |
| 02   | [02-display-lvgl-port.md](./02-display-lvgl-port.md)             | 把 NV3007 + LVGL 初始化迁出 `main.cpp` | 01    |
| 03   | [03-config-persistence.md](./03-config-persistence.md)           | SPIFFS + SimpleIni 持久化键映射        | 01    |
| 04   | [04-protocol-config-sync.md](./04-protocol-config-sync.md)       | 私有协议 `CMD_CONFIG_SET` 同步         | 03    |
| 05   | [05-ui-screens.md](./05-ui-screens.md)                           | 音乐 / PC 状态 / HA / 设置屏           | 02/04 |
| 06   | [06-network-voice-rgb-audio.md](./06-network-voice-rgb-audio.md) | WiFi / BLE / 语音 / RGB / 音频         | 04    |
| 07   | [07-placeholder-completion.md](./07-placeholder-completion.md)   | 2.4G / 频谱 / OTA / 占位命令补齐       | 06    |

## 使用方式

1. 阅读 `../FEATURE_DOC.md` 了解整体功能背景。
2. 阅读 `../ARCHITECTURE.md` 了解目录结构与模块边界。
3. 从 `01-` 开始按顺序实现，每完成一项修改对应文件中的复选框 `- [ ]` → `- [x]`。
4. 阶段性产出（接口变更、行为差异、新增配置项）在该阶段文档的 `变更记录` 小节追加。

## 文件命名规范

- `NN-阶段名.md`：`NN` 为两位阶段序号；阶段名使用英文短横线连接，全小写。
- 每个文档内部固定使用以下小节：`目标`、`范围`、`前置条件`、`任务清单`、`验收标准`、`变更记录`、`备注`。
- 不在本目录放置实现代码或二进制产物，仅保留规划与进度。
