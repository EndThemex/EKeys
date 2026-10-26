# 阶段 12 — BLE 多主机（多配对 + 单活动链路 + 切换）

> 关联代码：[`src/output/BLEKeyboardImpl.cpp`](../src/output/BLEKeyboardImpl.cpp) /
> [`src/keymap/KeyEventDispatcher.cpp`](../src/keymap/KeyEventDispatcher.cpp) /
> [`src/tasks/MainTask.cpp`](../src/tasks/MainTask.cpp) /
> [`src/ui/ui_StatusBar.c`](../src/ui/ui_StatusBar.c)

## 1. 目标与边界

**目标**：BLE 模式下保存最多 4 个已配对主机（槽位），同一时刻只保持 **1 条活动链路**，按键只发往活动主机；用可配置功能键循环 / 直选切换槽位，状态栏显示当前槽位。

**边界（本阶段不做）**：

- 不做「同时保持多条链路」。原因：[`BLECharacteristic::notify()`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/libraries/BLE/src/BLECharacteristic.cpp#L523-L556) 会遍历所有已连接 peer 逐个发送，多链路的默认后果是**按键同时打进所有主机**；要选择性发送必须裸调 `esp_ble_gatts_send_indicate(conn_id)`，改造量与风险显著更高。
- 不迁移 NimBLE，不引入新库，不改 `usageToLibKeycode()` 的键值域映射。
- 不做 A2DP / 多设备音频。

## 2. 可行性依据（已核对底层）

| 事实 | 位置 | 结论 |
| --- | --- | --- |
| Bluedroid 已启用；`CONFIG_BT_CTRL_BLE_MAX_ACT=6`、`CONFIG_BT_SMP_MAX_BONDS=15` | `tools/sdk/esp32s3/sdkconfig` | 链路数与绑定数都够，槽位上限 4 是产品取舍而非能力限制 |
| **没有** `esp_ble_gap_get_bonded_devices()` | `esp_gap_ble_api.h` | IDF 4.4.4 未提供；槽位表必须自管 + 自持久化 |
| `esp_ble_adv_params_t` 含 `peer_addr` / `peer_addr_type` | [`esp_gap_ble_api.h#L346-L361`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/include/bt/host/bluedroid/api/include/api/esp_gap_ble_api.h#L346-L361) | 定向广播可行（但受 1.28s 规范超时约束，见 §7） |
| `BLEAdvertising` 包装类不暴露 `peer_addr`，`start()` 固定用内部 `m_advParams` | [`BLEAdvertising.cpp#L247`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/libraries/BLE/src/BLEAdvertising.cpp#L247) | 定向广播需裸调 `esp_ble_gap_start_advertising()`；白名单过滤可直接用 `setScanFilter()` |
| `setScanFilter(false, true)` → `ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST` | [`BLEAdvertising.cpp#L123-L145`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/libraries/BLE/src/BLEAdvertising.cpp#L123-L145) | **只允许白名单设备建连**，无需定向广播 |
| 连接/断开事件同时调用 1 参和 2 参回调，`param` 带 `remote_bda` / `ble_addr_type` / `conn_id` | [`BLEServer.cpp#L142-L179`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/libraries/BLE/src/BLEServer.cpp#L142-L179)、`#L198-L221` | 能拿到对端地址做槽位匹配 |
| t-vk 库 `onConnect` / `onDisconnect` 为 `protected virtual` | `.pio/libdeps/.../ESP32 BLE Keyboard/BleKeyboard.h#L174-L178` | **可以子类化接管事件**，不必改库源码 |

## 3. 总体结构

```
                    ┌──────────────────────────┐
  功能键 HOST_SWITCH │ BleHostManager          │
  ──────────────────►│  Idle / Adv / Switched   │
                     │  active_slot, pending    │
                     └───────┬──────────┬───────┘
                             │          │
              ┌──────────────▼──┐   ┌───▼──────────────────┐
              │ BleHostSlots    │   │ EKeysBleKeyboard     │
              │ 槽位表 + 持久化  │   │ (继承 BleKeyboard)    │
              │ config.ini      │   │ onConnect/onDisconnect│
              └─────────────────┘   └──────────────────────┘
                                             │
                              BLEKeyboardImpl（press/release/type 不变）
```

## 4. 新增模块

### 4.1 `src/output/BleHostSlots.h/.cpp` — 槽位表与持久化

```cpp
struct BleHostSlot {
    bool    used;
    uint8_t addr[6];
    uint8_t addr_type;      // BLE_ADDR_TYPE_PUBLIC / BLE_ADDR_TYPE_RANDOM
    char    label[24];      // 首次连接时写入 "Host 1" 之类的默认名
};
inline constexpr uint8_t kMaxBleHostSlots = 4;
```

- 持久化到 `config.ini` 新段 `[bluetooth]`：`host1_addr`（`"AA:BB:CC:DD:EE:FF"`）、`host1_type`、`host1_label`，共 4 组；另存 `active_host`（0 = 未确定，1..4）。
- 读写统一走 `Configuration`，新增 `loadBleHostSlots()` / `saveBleHostSlots()`。
  > **强制约束**：新增 `DeviceSettings` 字段时 save 路径与 load 路径必须成对补齐（项目已因只补一边导致重启丢配置）。
- 提供 `formatBdAddr()` / `parseBdAddr()` 两个小工具。

### 4.2 `src/output/BleHostManager.h/.cpp` — 状态机

```cpp
class BleHostManager {
public:
    static BleHostManager &instance();

    void begin();                                   // 载入槽位表 + 启动全放行广播
    void reset();                                   // 切出 BLE 模式时调用

    void onConnected(const uint8_t addr[6], uint8_t addr_type, uint16_t conn_id);
    void onDisconnected(uint16_t conn_id);

    bool switchToNext();                            // 循环切下一个已用槽位
    bool switchTo(uint8_t slot_1based);
    void clearSlot(uint8_t slot_1based);            // 重置配对（换机/排障）

    uint8_t     activeSlot() const;                 // 0 = 未连接
    uint8_t     slotCount() const;
    const char *slotLabel(uint8_t slot_1based) const;
};
```

关键行为：

- `onConnected()`：按 `remote_bda` 匹配槽位；命中 → `active_slot = 该槽`；未命中 → 占一个空槽并**立即持久化**（首次配对）。然后通知 UI 刷新槽位。
- `onDisconnected()`：
  - 有 `pending_target`（切换场景）→ 白名单只放目标地址 + 启动广播（`setScanFilter(false, true)` + `esp_ble_gap_update_whitelist()`）；
  - 无（常规掉线）→ 清空白名单，全放行广播，等任意已配对主机回连。
- `switchTo(slot)`：
  1. `slot == active_slot` → no-op 返回 false；
  2. **`keyboard.releaseAll()`**（硬性要求，防主机侧卡键）；
  3. `pending_target = 目标地址`，`active_slot = 0`；
  4. `esp_ble_gap_disconnect(当前地址)`；
  5. 启动 10s 超时定时器（由 `MainTask` tick 驱动，不新建任务）：超时 → 清 `pending_target`、清白名单、全放行广播 + 通知 UI 提示「请在主机蓝牙设置里点一下 EKeys」。
- `switchToNext()`：在已用槽位里找 `active_slot` 的下一个（环形）；无活动槽位时取第一个已用槽位。
- 全部接口在 USB / 2.4G 模式下直接返回 false（由调用方先判断工作模式）。

### 4.3 `BLEKeyboardImpl` 改造

把内部 `static BleKeyboard ble("EKeys", "EKeys", 100)` 换成子类，接管连接/断开事件：

```cpp
class EKeysBleKeyboard : public BleKeyboard {
public:
    using BleKeyboard::BleKeyboard;
protected:
    void onConnect(BLEServer* s, esp_ble_gatts_cb_param_t* p) override {
        BleHostManager::instance().onConnected(p->connect.remote_bda,
                                               p->connect.ble_addr_type,
                                               p->connect.conn_id);
    }
    /* 1 参版本不要覆盖：基类实现负责置 BLE2902 通知位 */
    void onDisconnect(BLEServer* s) override {
        /* 故意不调基类：基类 onDisconnect 会无条件 advertising->start()
           （BleKeyboard.cpp L517-L530），与白名单策略冲突。
           广告启停统一由 BleHostManager 决定。 */
    }
    void onDisconnect(BLEServer* s, esp_ble_gatts_cb_param_t* p) override {
        BleHostManager::instance().onDisconnected(p->disconnect.conn_id);
    }
};
```

要点：

- `begin()` 的幂等逻辑（`s_ble_started`）、`esp_bt_controller_mem_release()`、F12 的 `ESP_LE_AUTH_BOND` 覆盖 **全部保留不动**。
- 拿到 `BLEAdvertising` 句柄的方式：在 `onDisconnect(pServer, param)` 里用 `pServer->getAdvertising()` 取，无需访问库私有成员。
- `press/release/type/releaseAll/isConnected` 行为不变；`isConnected()` 在切换窗口内会短暂 `false`，属预期。
- BLE 新增对外方法：`bool switchHostToNext()` / `bool switchHostTo(uint8_t)`，内部转发给 `BleHostManager`，供 `KeyEventDispatcher` 使用（保持 output 层是唯一 BLE 依赖点）。

## 5. 按键触发

### 5.1 功能串常量

在 [`src/utils/keymap_types.h`](../src/utils/keymap_types.h#L70-L78) 的功能串常量区新增：

```cpp
inline constexpr const char *kFunctionKeyHostSwitch = "KEY_FUNCTION_HOST_SWITCH";
inline constexpr const char *kFunctionKeyHost1      = "KEY_FUNCTION_HOST_1";
/* HOST_2 / HOST_3 / HOST_4 同 */
```

### 5.2 拦截点

放在 [`KeyEventDispatcher::onKeyEdge()`](../src/keymap/KeyEventDispatcher.cpp#L57-L75)，与现有 `isVoiceTriggerKey` 完全同构：

```
命中 KEY_FUNCTION_HOST_* 且 pressed → BleHostManager::switchToNext()/switchTo(n) → return
（不走 ClickHighlight，更不会走 HID）
```

理由与注意事项：

- `onKeyEdge(pressed[i], true)` 在 [MainTask.cpp#L661](../src/tasks/MainTask.cpp#L659-L680) 已先于 `resolver_.press()` 调用，这里 `return` 即天然跳过 HID，**MainTask 主循环结构无需改动**。
- release 无需处理：未知功能串经 `resolveKeyWithModifier()` 解析为 0，[KeyResolver.cpp#L249-L265](../src/keymap/KeyResolver.cpp#L249-L265) 无副作用。
- **必须覆盖 FUN 组合层**：`g_resolver` 已在 `init()` 注入，按 `g_resolver->activeFunLayer()` 取 `combo1_function_key` / `combo2_function_key` / `function_key`（项目已有教训：只查单击层会导致 FUN+按键 配置失效）。
- 功能键「是否发 HID」的语义在 USB 模式下必须同样被拦截（否则 USB 模式按下会打出无意义字符）；`switchTo*` 内部返回 false 即可，但仍要 `return` 不派发 HID。

## 6. UI 与状态显示

- [`ui_StatusBar.h`](../src/ui/ui_StatusBar.h#L22-L26) 新增：
  ```c
  extern void status_bar_set_host_slot(uint8_t active, uint8_t total);
  ```
  在现有「主机连接图标」旁显示 `1/4` 小字（或 4 个小点），`total == 0` 时不显示。
- **跨任务禁止直接调 LVGL**：`BleHostManager` 运行在 MainTask 上下文，槽位变化时投递 `DisplayMessage`（在 `message_types.h` 的 `DisplayMessageType` 新增一个条目，如 `HostSlotChanged`，复用既有队列），由 `DisplayTask` 调 `status_bar_set_host_slot()`。同款做法已用于工作模式、日期、连接状态。
- 切换窗口内的提示（「正在连接 主机 2」/ 超时提示）优先复用现有临时提示控件；若无，则只做图标 + 数值状态，不新增弹窗组件。
- **设置二级页不加入口**：与 FUN 键一致，主机切换键位仅 App 端 0x06 可配。

## 7. 协议与 App

**设备 → App**

- `cmd_config` 上报新增：`ble_host_active`（1 基，0 = 未连接）+ `ble_host_slots`（数组：`{index, label, addr, used}`）。
- 新增一条 App 主动控制命令（建议 `CMD_BLE_HOST_ACTION`，需分配新 cmd id）：`{action: "next" | "select" | "clear", slot}`，用于 App 内直接切主机 / 重置槽位。

**App → 设备**

- 0x06 协议结构不变即可让用户把某个键的 function 通道设成 `KEY_FUNCTION_HOST_SWITCH`，但要补：
  - [`protocol.rs#L1644`](file:///d:/search/esp/Keys/EKeysApp/src/protocol.rs#L1644) 的 `function_label()` 增加 `KEY_FUNCTION_HOST_*` 友好名（如「切换主机：下一个」）。
  - [`panel_keymap.rs#L2818`](file:///d:/search/esp/Keys/EKeysApp/src/ui/panel_keymap.rs#L2818) 的固件功能下拉常量表追加 4 个 `KEY_FUNCTION_HOST_*` 项 + i18n 文案。
- **两端功能串字符串必须逐字节一致**（项目已踩过不一致导致功能失效的坑）。
- 切换主机按钮建议放在面板的「设备/连接」区域，与 host 槽位列表一起展示。

## 8. 风险与对策

| # | 风险 | 对策 |
| --- | --- | --- |
| 1 | 旧主机抢连（断开后先连上的是刚断的那台） | 切换场景一律走白名单广播，只允许目标地址建连；常规掉线场景才全放行 |
| 2 | 主机使用**可解析私有地址（RPA）**，每次连接 `remote_bda` 都变 | 槽位记录 `addr_type`；若 `random` 且匹配不上槽位，退化为「按连接顺序占空槽」；提供 `clearSlot()` 供重配对 |
| 3 | 主机不自动回连（部分 Android 机型需手动点） | 10s 超时后回退全放行广播 + UI 明确提示；不重试死循环 |
| 4 | 断开瞬间主机侧卡键 | `switchTo` 内第一件事就是 `releaseAll()` |
| 5 | 切换中切到 USB 模式（`applyWorkMode`） | `BleHostManager::reset()`：清 pending、停超时定时器、不再动广告；BLE 侧 `releaseAll()` 由既有析构逻辑兜底 |
| 6 | 定向广播 1.28s 规范超时 | **默认不使用**；仅在白名单方案在某主机实测不通时作为备选：`adv_type = ADV_TYPE_DIRECT_IND_LOW` + `peer_addr` + 裸调 `esp_ble_gap_start_advertising()`，并在 `ESP_GAP_BLE_ADV_TERMINATED_EVT` 里重挂 |
| 7 | 存储：NVS 仅 20KB（`partitions-16MB.csv`） | 槽位表放 `config.ini`（SPIFFS），与现有配置一致；NVS 只留给 Bluedroid 存 bond（4 条无压力） |
| 8 | BLE + WiFi 共存内存 | 运行时链路数仍为 1，内存/功耗与现状基本一致；多 bond 只占 NVS |

## 9. 实施顺序（每步可独立验证）

1. `BleHostSlots` 槽位表 + 持久化：串口日志验证「首配写入 → 重启后保留」。
2. `EKeysBleKeyboard` 子类接管连接/断开：日志打印 `remote_bda` / `addr_type` / `conn_id`（先不改广告策略）。
3. 白名单广播 + `switchTo`：先用临时串口命令/临时按键触发，实测断开 → 目标回连。
4. 功能串 + `KeyEventDispatcher` 拦截（含 FUN 层通道）。
5. 状态栏槽位显示（走 DisplayMessage）。
6. 协议上报 + App 下拉项与切换按钮。

## 10. 文件清单

**新增**

- `src/output/BleHostSlots.h` / `.cpp`
- `src/output/BleHostManager.h` / `.cpp`

**修改**

- `src/output/BLEKeyboardImpl.h` / `.cpp`（子类 + 事件接管 + `switchHostTo*` 转发）
- `src/utils/keymap_types.h`（功能串常量）
- `src/keymap/KeyEventDispatcher.h` / `.cpp`（拦截 + FUN 层取通道）
- `src/config/DeviceSettings.h`、`Configuration.h` / `.cpp`、`parseConfigSetCommand.cpp`、`src/protocol/commands/cmd_config.cpp`（槽位字段 load/save/上报）
- `src/message_types.h`（`DisplayMessageType::HostSlotChanged`）
- `src/ui/ui_StatusBar.h` / `.c`、`src/tasks/DisplayTask.cpp`（槽位显示）
- `src/tasks/MainTask.cpp`（槽位变化投递 + 切换超时轮询）
- EKeysApp：`src/protocol.rs`、`src/ui/panel_keymap.rs`、i18n 文案

## 11. 验收清单

- [ ] 首次连接 2 台及以上主机后重启设备，槽位表与「上次活动主机」保留。
- [ ] BLE 已连主机 A 时按键只影响 A；切到主机 B 后按键只影响 B，A 侧无残留按键（无卡键）。
- [ ] 切换窗口内按任何键不产生 HID 输出到任何主机（`releaseAll` + 未连接守卫）。
- [ ] 键盘侧 USB/BLE 模式互切后槽位信息不丢，且 USB 模式下 `HOST_SWITCH` 键不产生 HID。
- [ ] 功能键配置在 FUN1/FUN2 层同样生效。
- [ ] 无已配对主机时按 `HOST_SWITCH` 不崩溃、无日志刷屏。
- [ ] 状态栏槽位显示与实际连接一致（含未连接、连接中、超时三种状态）。