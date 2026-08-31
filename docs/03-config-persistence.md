# 阶段 03 — 配置持久化

> 状态：未开始
> 关联章节：[`FEATURE_DOC.md §3.2`](../FEATURE_DOC.md) / [`§3.3`](../FEATURE_DOC.md)
> 关联目录：[`../ARCHITECTURE.md §3.7`](../ARCHITECTURE.md)

## 目标

引入 SPIFFS + SimpleIni，把 `KeyResolver` 的硬编码映射改为从 `/config.ini` 与 `keymap{N}.ini` 加载；并实现 `Configuration::mutex_` 保护的读写接口。

## 范围

1. 引入 `lib/SimpleIni`（PlatformIO lib_deps）。
2. 新增 `src/services/ConfigStore` 与 `src/services/KeymapRepository`。
3. 引入 `data/config.ini` 与 `data/keymap1.ini` 示例。
4. 提供 `load()` / `SaveKeyMapping()` / `SaveSetting()` / `loadActiveProfileKeyMapping()` / `switchActiveProfile()`。

## 前置条件

- 阶段 01 已完成，`KeyResolver` 与 `IKeyboard` 已稳定。
- 阶段 02 已完成，`DisplayTask` 已可显示文本。

## 任务清单

- [ ] **3.1 `lib_deps` 追加 `SimpleIni@4.19`**：在 [`../platformio.ini`](../platformio.ini) 中加入 `SimpleIni`。
- [ ] **3.2 `data/config.ini`**：示例文件，至少包含 `[system] active_keymap_profile=0`、`[wifi] wifi_switch=0 wifi_ssid= wifi_password=`、`[rgb] rgb_mode=0`。
- [ ] **3.3 `data/keymap1.ini`**：示例文件，键 1~11 各自 `function_key=` 或 `normal_key=a+b` 等。
- [ ] **3.4 `src/services/ConfigStore.h/.cpp`**：封装 SPIFFS 挂载与 SimpleIni 加载/保存；提供 `loadGlobal(path)` / `saveGlobal(path)` / `exists(path)`。
- [ ] **3.5 `src/services/KeymapRepository.h/.cpp`**：按 `FEATURE_DOC §3.2` API；互斥量 `Configuration::mutex_` 由本类持有。
- [ ] **3.6 `src/config/Configuration.h/.cpp`**：单例，提供 `load()`、`saveSetting()`、`loadActiveProfileKeyMapping()`、`switchActiveProfile(uint8_t)`、`getProfileConfigPath(uint8_t)` / `getProfileDisplayName(uint8_t)` / `getProfileIconPath(uint8_t)`。
- [ ] **3.7 `src/config/DeviceSettings.h`**：POD，字段对齐 `FEATURE_DOC §6` 列表（`wifi_* / work_mode / rgb_* / tft_* / device_volume / voice_* / pc_status_mask / active_keymap_profile`），先全部 `= 0` 占位。
- [ ] **3.8 `src/keymap/KeyResolver`**：构造函数接收 `Configuration&`；`begin()` 时调用 `loadActiveProfileKeyMapping()`；每次按键边沿结束后回写 LED 状态（占位即可）。
- [ ] **3.9 `src/app/AppContext.h/.cpp`**：持有 `Configuration` 与 `KeymapRepository` 指针；`MainTask::begin()` 中调用 `Configuration::load()`。
- [ ] **3.10 `src/main.cpp`**：新增 SPIFFS 挂载步骤；启动失败则进入 `LOG_ERROR` 死循环（参考阶段 02 自检标准）。
- [ ] **3.11 编译验证**：`pio run -t uploadfs` 把 `data/` 上传到 SPIFFS 分区；按键映射与示例 `keymap1.ini` 一致。
- [ ] **3.12 自检记录**：在 `变更记录` 记录 SimpleIni 版本与 SPIFFS 分区大小。

## 验收标准

- 第一次上电：SPIFFS 中无 `config.ini`，使用默认 `DeviceSettings`；`LOG_INFO` 输出 `Using default config`。
- 手动 `data/config.ini` 中修改 `active_keymap_profile=1` 后上传：下次上电即加载 `keymap2.ini`。
- 运行时调用 `Configuration::switchActiveProfile(2)` 后再 `Configuration::saveSetting("active_keymap_profile", 2)`，重启后仍是 profile 2。
- `Configuration::mutex_` 在两个并发调用下不发生死锁（提供最小验证脚本或注释说明）。

## 变更记录

- _暂无_

## 备注

- 本阶段不实现 Profile 图标上传（PNG base64）；保留接口但暂不引用。
- `SimpleIni` 在 PSRAM 上跑很慢，避免在 `loop()` 中每次按键都重新加载。
- 若 SPIFFS 挂载失败（`SPIFFS.begin(true)` 返回 false），需 `format` 后重试，本阶段允许直接 `while(true)` 报警。
