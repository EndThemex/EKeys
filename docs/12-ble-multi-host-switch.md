# 阶段 12 — BLE 多主机（多配对 + 单活动链路 + 切换 + 三端管理）

> 关联代码：[`src/output/BLEKeyboardImpl.cpp`](../src/output/BLEKeyboardImpl.cpp) /
> [`src/output/BleHostManager.cpp`](../src/output/BleHostManager.cpp) /
> [`src/keymap/KeyEventDispatcher.cpp`](../src/keymap/KeyEventDispatcher.cpp) /
> [`src/tasks/MainTask.cpp`](../src/tasks/MainTask.cpp) /
> [`src/rgb/RGBLightControl.cpp`](../src/rgb/RGBLightControl.cpp) /
> [`src/ui/ui_StatusBar.c`](../src/ui/ui_StatusBar.c)
>
> 三端：固件 `EKeys`、桌面 App `../EKeysApp`（Rust + egui）、Web 配置器 `../EKeysWeb`（React + Vite）

## 1. 目标与边界

**目标**

1. BLE 模式下保存最多 **4 个已配对主机**（槽位），同一时刻只保持 **1 条活动链路**，按键只发往活动主机。
2. **App / Web 新增「蓝牙设备管理」页**：列出连接过的设备、**改名**、**一键切换活动链路**、**展示连接状态**、重置槽位。
3. **每个槽位一个独立按键功能串**（`KEY_FUNCTION_HOST_1` ~ `KEY_FUNCTION_HOST_4`），按键即切到该设备。
4. **槽位状态灯**：配了 `KEY_FUNCTION_HOST_n` 的物理键，当槽位 n **已配对但未连接**时该键 **白色呼吸闪烁**；槽位 n 尚未配对时该键 **不亮**；连接成功后停止闪烁、交回常规灯效。

**边界（本阶段不做）**

- 不做「同时保持多条链路」。原因：[`BLECharacteristic::notify()`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/libraries/BLE/src/BLECharacteristic.cpp#L523-L556) 会遍历所有已连接 peer 逐个发送，多链路的默认后果是**按键同时打进所有主机**；要选择性发送必须裸调 `esp_ble_gatts_send_indicate(conn_id)`，改造量与风险显著更高。
  > 但架构上**不关门**：槽位表常驻 `conn_id`、连接准入策略留 `LinkPolicy` 开关（默认 `Single`），将来要多链路只需替换发送路径，槽位/配对/UI/协议均不用改（见 §9）。
- 不做「循环切换键」`KEY_FUNCTION_HOST_SWITCH`（第 5 个功能串）。本期只做 4 个直选键；循环键列为可选扩展。
- 不迁移 NimBLE，不引入新库，不改 `usageToLibKeycode()` 的键值域映射。
- 不做 A2DP / 多设备音频。

## 2. 可行性依据（已核对底层）

| 事实 | 位置 | 结论 |
| --- | --- | --- |
| Bluedroid 已启用；`CONFIG_BT_CTRL_BLE_MAX_ACT=6`、`CONFIG_BT_SMP_MAX_BONDS=15` | `C:\Users\Shane.Xia\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32s3\sdkconfig`（framework 包内，项目无 `tools/` 目录） | 链路数与绑定数都够，槽位上限 4 是产品取舍而非能力限制 |
| **没有** `esp_ble_gap_get_bonded_devices()`，但有 `esp_ble_get_bond_device_num()` / `esp_ble_get_bond_device_list()`（[esp_gap_ble_api.h#L1872-L1888](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/include/bt/host/bluedroid/api/include/api/esp_gap_ble_api.h#L1872-L1888)） | `esp_gap_ble_api.h` | bond 表**可枚举**（bond 记录存配对时交换的身份地址），用于 RPA 槽位对账（见 §8 风险 2）；但 label / active 拿不到，槽位表仍自管 + 自持久化 |
| `esp_ble_adv_params_t` 含 `peer_addr` / `peer_addr_type` | [`esp_gap_ble_api.h#L346-L361`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/include/bt/host/bluedroid/api/include/api/esp_gap_ble_api.h#L346-L361) | 定向广播可行（但受 1.28s 规范超时约束，见 §8） |
| `BLEAdvertising` 包装类不暴露 `peer_addr`，`start()` 固定用内部 `m_advParams` | [`BLEAdvertising.cpp#L247`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/libraries/BLE/src/BLEAdvertising.cpp#L247) | 定向广播需裸调 `esp_ble_gap_start_advertising()`；白名单过滤可直接用 `setScanFilter()` |
| `setScanFilter(false, true)` → `ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST` | [`BLEAdvertising.cpp#L123-L145`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/libraries/BLE/src/BLEAdvertising.cpp#L123-L145) | **只允许白名单设备建连**，无需定向广播 |
| 连接/断开事件同时调用 1 参和 2 参回调，`param` 带 `remote_bda` / `ble_addr_type` / `conn_id` | [`BLEServer.cpp#L142-L179`](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/libraries/BLE/src/BLEServer.cpp#L142-L179)、`#L198-L221` | 能拿到对端地址做槽位匹配 |
| t-vk 库 `onConnect` / `onDisconnect` 为 `protected virtual` | `.pio/libdeps/.../ESP32 BLE Keyboard/BleKeyboard.h#L174-L178` | **可以子类化接管事件**，不必改库源码 |
| 库的关键成员 `inputKeyboard` / `inputMediaKeys` / `_keyReport` 为 **private** | `BleKeyboard.h#L133-L150` | 子类拿不到句柄 → 多链路定向发送必须自管报告路径（见 §9） |
| 点击高亮映射 `led = key_id - 1`，11 灯 ↔ 11 键一一对应 | [`ClickHighlight.cpp#L23-L36`](../src/rgb/ClickHighlight.cpp#L23-L36)、[`RGBDriver.cpp#L24-L47`](../src/rgb/RGBDriver.cpp#L24-L47) | 槽位状态灯可直接复用按键→LED 映射 |
| 已有呼吸灯实现 `RGB_PULSE_MODE`（`cosf(elapsed_ms_*0.006f)`，~1s 周期） | [`RGBLightControl.cpp#L318-L324`](../src/rgb/RGBLightControl.cpp#L318-L324) | 白色呼吸闪烁照抄即可，无需新动画框架 |
| RGB 动画由 DisplayTask 渲染循环约 30ms 驱动（`RGBLightControl::instance().tick(delta)`）；`highlight_[11]` 为按 LED 独立 override | [`DisplayTask.cpp#L220`](../src/tasks/DisplayTask.cpp#L220)、[`RGBLightControl.h#L81-L85`](../src/rgb/RGBLightControl.h#L81-L85) | 提示灯作为第二个 override 层并入渲染，无需新任务 |
| 三端 cmd id 已对齐到 `0x17` | [`SerialProtocol.h#L26-L48`](../src/protocol/SerialProtocol.h#L26-L48)、`EKeysApp/src/protocol.rs#L123-L167`、`EKeysWeb/src/protocol/commands.ts#L8-L31` | 新命令可直接取 `0x18` |

## 3. 总体结构

```
   ┌─ Web (EKeysWeb/React) ─┐   ┌─ App (EKeysApp/Rust egui) ─┐
   │ BluetoothPanel.tsx     │   │ panel_bluetooth.rs         │
   └───────────┬────────────┘   └────────────┬───────────────┘
               │  同一 JSON 协议 (CMD_BLE_HOST 0x18)          │
               └───────────────┬─────────────────────────────┘
                               ▼
  功能键 HOST_1..4   ┌──────────────────────────┐
  ─────────────────►│ BleHostManager           │
                    │  Idle / Adv / Switching  │
                    │  active_slot, pending    │
                    └───┬──────────┬───────────┘
                        │          │
         ┌──────────────▼──┐   ┌───▼──────────────────┐
         │ BleHostSlots    │   │ EKeysBleKeyboard     │
         │ 槽位表+持久化    │   │ (继承 BleKeyboard)    │
         │ config.ini      │   │ onConnect/onDisconnect│
         └─────────────────┘   └──────────────────────┘
                        │
              ┌─────────▼──────────┐        ┌───────────────────────┐
              │ MainTask 拦截点     │───────►│ RGBLightControl       │
              │ 功能串 + 掩码计算   │ 投递   │ 提示灯 override 层     │
              └────────────────────┘ 消息   └───────────────────────┘
```

## 4. 固件新增模块

### 4.1 `src/output/BleHostSlots.h/.cpp` — 槽位表与持久化

```cpp
struct BleHostSlot {
    bool    used;
    uint8_t addr[6];
    uint8_t addr_type;      // BLE_ADDR_TYPE_PUBLIC / BLE_ADDR_TYPE_RANDOM
    uint16_t conn_id;       // 当前连接 id，0xFFFF = 未连接（为多链路预留）
    char    label[32];      // UTF-8，App/Web 可改名；空串用 "主机 N"
};
inline constexpr uint8_t kMaxBleHostSlots = 4;
```

- 持久化到 `config.ini` 新段 `[bluetooth]`：`host1_addr`（`"AA:BB:CC:DD:EE:FF"`）、`host1_type`、`host1_label`，共 4 组；另存 `active_host`（0 = 未确定，1..4）。
- 读写统一走 `Configuration`，新增 `loadBleHostSlots()` / `saveBleHostSlots()`。
  > **强制约束**：新增 `DeviceSettings` 字段时 save 路径与 load 路径必须成对补齐（项目已因只补一边导致重启丢配置）。
- 改名入口：`bool setLabel(uint8_t slot_1based, const char *utf8)`，**按 UTF-8 字符边界截断**到 `label[32]`（中文最多 10 字），截断后立即持久化。
- 提供 `formatBdAddr()` / `parseBdAddr()` 两个小工具。
- `clearSlot()` 额外调 `esp_ble_remove_bond_device(addr)`（单参数，[esp_gap_ble_api.h#L1862](file:///C:/Users/Shane.Xia/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/include/bt/host/bluedroid/api/include/api/esp_gap_ble_api.h#L1862)；注意 API 名不带 `gap`，不存在 `esp_ble_gap_remove_bond_device()`）清掉 Bluedroid 侧 bond，否则主机仍会认为已配对。

### 4.2 `src/output/BleHostManager.h/.cpp` — 状态机 + 查询接口

```cpp
enum class BleSlotState : uint8_t { Unused, Disconnected, Switching, Connected };

class BleHostManager {
public:
    static BleHostManager &instance();

    void begin();                                   // 载入槽位表 + 启动全放行广播
    void reset();                                   // 切出 BLE 模式时调用

    void onConnected(const uint8_t addr[6], uint8_t addr_type, uint16_t conn_id);
    void onDisconnected(uint16_t conn_id);

    bool switchTo(uint8_t slot_1based);             // 按键 / App / Web 统一入口
    void clearSlot(uint8_t slot_1based);            // 重置配对（换机/排障）
    bool renameSlot(uint8_t slot_1based, const char *label);
    void tick(uint32_t now_ms);                     // 切换超时轮询（MainTask 5ms tick 驱动）

    /* —— 查询（UI / 协议 / 提示灯共用，只读，无副作用）—— */
    uint8_t     activeSlot() const;                 // 0 = 未连接
    uint8_t     slotCount() const;
    BleSlotState slotState(uint8_t slot_1based) const;
    const char *slotLabel(uint8_t slot_1based) const;
    uint16_t    hostHintMask(const KeyResolver &resolver) const;  // 见 §6.2
};
```

关键行为：

- `onConnected()`：匹配槽位——先按 `remote_bda` 直配（静态地址主机的快路径）；未命中且对端为随机地址（可能 RPA）时，用 `esp_ble_get_bond_device_list()` 与 bond 表**对账**（bond 记录存配对时交换的身份地址）。命中 → 记 `conn_id`、`active_slot = 该槽`；仍未命中 → 占一个空槽、写默认 label 并**立即持久化**（首次配对）。状态变化统一由 MainTask 边沿投递（见跨任务约束）。
  > **禁止**「匹配不上就占空槽」作为常规路径：Windows / macOS 默认 RPA，回连地址每次都变，直接占槽会让同一台主机刷爆 4 个槽位。对账不上的陌生地址才占槽。
- `onDisconnected()`：
  - 清 `active_slot` / `conn_id`（active_slot 单一写者在本回调，`switchTo` 不越权改）；
  - 有 `pending_target`（切换场景）→ 白名单只放目标地址 + 启动广播（`setScanFilter(false, true)` + `esp_ble_gap_update_whitelist()`）；
  - 无（常规掉线）→ 清空白名单，全放行广播，等任意已配对主机回连。
- `switchTo(slot)`：
  1. 守卫：非 BLE 模式 / 槽位未用 / `slot == active_slot` / 已在切换中 → 返回 false；
  2. **`keyboard.releaseAll()`**（硬性要求，防主机侧卡键）；
  3. 置 `pending_target = 目标地址` 与 `pending_deadline`（**不改 `active_slot` / `conn_id`**，由断开事件统一清，见跨任务约束）；
  4. `esp_ble_gap_disconnect(当前地址)`；
  5. 启动 10s 超时（由 `MainTask` 5ms tick 调 `tick()` 驱动，不新建任务）：超时 → 清 `pending_target`、清白名单、全放行广播 + 通知 UI 提示「请在主机蓝牙设置里点一下 EKeys」。**超时回退后连上的任意已配对主机都正常接入**（含被切走的原主机），走 `onConnected()` 正常流程并回推 list；UI 文案区分「已连上目标」与「已回连 <label>（非目标）」。
     > 白名单对 RPA 主机可能失效（§8 风险 2）：若实测目标主机在白名单窗口内连不上，降级为「全放行 + 先连上者裁决」——连上的已配对主机即接入并回推 list，不反复断连；用户再按槽位键重切。
- **跨任务约束**：`pending_target` / `pending_deadline` 只由 MainTask（`switchTo` / `tick`）写；`active_slot` / `conn_id` 只由 BT 任务（`onConnected` / `onDisconnected`）写；跨任务读 `uint8_t` / `uint16_t` 天然原子。`tick()` 检测到 `active_slot` 命中 `pending_target` 或超时才清 pending 并投递 UI/灯效消息——**所有 UI / 协议 / 提示灯投递统一发生在 MainTask 上下文**，BT 任务回调只改状态不投递（与 `g_active_screen_tag` 单写单读同款纪律）。
- 全部接口在 USB / 2.4G 模式下 `switchTo` 返回 false（由调用方先判断工作模式）；`hostHintMask()` 在非 BLE 模式返回 0。

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
    /* 1 参 onConnect 不要覆盖：基类实现负责置 BLE2902 通知位与 connected */
    void onDisconnect(BLEServer* s) override {
        /* BLEServer 会先后调用 1 参与 2 参回调（BLEServer.cpp L209-L210），
           只覆盖 2 参拦不住基类。1 参基类实现置 connected=false + 关 BLE2902，
           但末尾会无条件 advertising->start()（全放行，BleKeyboard.cpp
           L517-L530），与白名单策略冲突——因此先调基类再立即停广告
           （Bluedroid 命令队列顺序执行，最终态为停止）。
           注意：connected / advertising 均为库 private 成员（BleKeyboard.h
           L138-L144），子类无法自行置位，必须借道基类实现。
           广告启停统一由 BleHostManager 决定。 */
        BleKeyboard::onDisconnect(s);
        s->getAdvertising()->stop();
    }
    void onDisconnect(BLEServer* s, esp_ble_gatts_cb_param_t* p) override {
        BleHostManager::instance().onDisconnected(p->disconnect.conn_id);
    }
};
```

要点：

- `begin()` 的幂等逻辑（`s_ble_started`）、`esp_bt_controller_mem_release()`、F12 的 `ESP_LE_AUTH_BOND` 覆盖 **全部保留不动**。
- 拿到 `BLEAdvertising` 句柄的方式：在 `onDisconnect(pServer, param)` 里用 `pServer->getAdvertising()` 取，无需访问库私有成员。
- `press/release/type/releaseAll` 行为不变；新增对外方法 `bool switchHostTo(uint8_t)` / `bool setHostLabel(uint8_t, const char*)` / `uint16_t hostHintMask()`，内部转发给 `BleHostManager`，保持 output 层是唯一 BLE 依赖点。
- `isConnected()` 在切换窗口内会短暂 `false`，属预期。

## 5. 按键触发

### 5.1 功能串常量

在 [`src/utils/keymap_types.h#L77`](../src/utils/keymap_types.h#L77) 的功能串常量区新增：

```cpp
inline constexpr const char *kFunctionKeyHost1 = "KEY_FUNCTION_HOST_1";
inline constexpr const char *kFunctionKeyHost2 = "KEY_FUNCTION_HOST_2";
inline constexpr const char *kFunctionKeyHost3 = "KEY_FUNCTION_HOST_3";
inline constexpr const char *kFunctionKeyHost4 = "KEY_FUNCTION_HOST_4";
```

命名规则：后缀数字 **1 基**，与槽位号（1..4）一致，不再做 0 基/1 基换算。

### 5.2 拦截点

放在 [`MainTask.cpp#L667-L680`](../src/tasks/MainTask.cpp#L667-L680)，与现有 `functionKeyJumpTarget` **并列**：

```
命中 KEY_FUNCTION_HOST_n 且 pressed → BleHostManager::switchTo(n) → 跳过 resolver_.press()（不走 HID）
```

理由与注意事项：

- **不放在 `KeyEventDispatcher::onKeyEdge()`**：那里只能拿到 `key_id` + `resolver`，取「当前活动 FUN 层」的取层代码与 `functionKeyJumpTarget`（[MainTask.cpp#L381-L388](../src/tasks/MainTask.cpp#L381-L388)）重复。与它并排放可让一套 `activeFunLayer()` 取层逻辑两用。
- 拦截实现为在现有 if/else 链（[MainTask.cpp#L667-L680](../src/tasks/MainTask.cpp#L667-L680)）中并列一个 `functionKeyJumpTarget` 同款分支，命中时**不调 `resolver_.press()`**——这是循环体内的分支跳过，不是函数级 return。循环尾部的矩阵键 ActionInput 投递（[MainTask.cpp#L688-L695](../src/tasks/MainTask.cpp#L688-L695)）照常执行：非 KEYMAPPED / 二级屏对矩阵键 ActionInput 丢弃（项目路由规则 §3），无副作用。
- `onKeyEdge(pressed[i], true)` 在 [MainTask.cpp#L661](../src/tasks/MainTask.cpp#L661) 已先于 `resolver_.press()` 调用，命中分支跳过 `resolver_.press()` 即天然不发 HID，**MainTask 主循环结构无需改动**。
- release 无需处理：未知功能串经 `resolveKeyWithModifier()` 解析为 0，[KeyResolver.cpp#L249-L265](../src/keymap/KeyResolver.cpp#L249-L265) 无副作用。
- **必须覆盖 FUN 组合层**：按 `g_resolver->activeFunLayer()` 取 `combo1_function_key` / `combo2_function_key` / `function_key`（项目已有教训：只查单击层会导致 FUN+按键 配置失效）。
- 功能键「是否发 HID」的语义在 USB 模式下必须同样被拦截（否则 USB 模式按下会打出无意义字符）；`switchTo*` 内部返回 false 即可，但命中分支仍要跳过 `resolver_.press()` 不派发 HID。
- 无已配对主机 / 目标槽位为空时 `switchTo` 返回 false，**日志要节流**（按住不放会每 5ms 触发一次）。

## 6. 交互反馈：状态栏 + 槽位状态灯

### 6.1 状态栏槽位

- [`ui_StatusBar.h#L22-L26`](../src/ui/ui_StatusBar.h#L22-L26) 新增：

```c
extern void status_bar_set_host_slot(uint8_t active, uint8_t total);
```

在现有「主机连接图标」旁显示 `2/4` 小字，`total == 0` 时不显示；切换窗口显示 `…/4`，超时显示 `-/4`。
- **跨任务禁止直接调 LVGL**：`BleHostManager` 运行在 MainTask 上下文，槽位变化时投递 `DisplayMessage`（`message_types.h` 的 `DisplayMessageType` 新增 `HostSlotChanged`，复用既有队列），由 `DisplayTask` 调 `status_bar_set_host_slot()`。同款做法已用于工作模式、日期、连接状态。
- **设置二级页不加入口**：与 FUN 键一致，主机槽位键位仅 App / Web 可配。

### 6.2 槽位状态灯（白色呼吸闪烁）

**规则**

| 条件 | 该键 LED 表现 |
| --- | --- |
| `work_mode != Bluetooth` | 不参与提示灯（交回 `rgb_mode` 常规渲染） |
| 槽位 n 未配对 / 未使用（`Unused`） | **不亮**（即使该键配了 `KEY_FUNCTION_HOST_n`） |
| 槽位 n 已配对且 `state == Disconnected` | **白色呼吸闪烁**（`kPalette24[0]` = 255,255,255） |
| 槽位 n 已配对且 `state == Switching`（切换窗口内） | **白色呼吸闪烁**（目标尚未连上，与 `Disconnected` 同类处理） |
| 槽位 n 已配对且 `state == Connected`（即当前活动链路） | 停止闪烁，该 LED 交回常规灯效 |
| 该键此刻被按下（点击高亮） | 点击高亮优先，覆盖提示灯 |

> 判定依据是**槽位状态**而非「键是否配了功能串」：`Unused` 与 `Connected` 都不亮，仅 `Disconnected` / `Switching` 闪。所以「配了 4 个槽位键但只配对过 1 台」时只会有 1 颗灯参与提示（且那台若已连接则不闪）。

**参数**：呼吸周期 ~1.2s，亮度在 `brightness_` 的 10% ~ 100% 之间（不全黑，否则像"灯坏了"）。

**实现（三个落点）**

1. `RGBLightControl` 新增第二个 override 层：

```cpp
/* 槽位提示灯掩码：位 i = LED i 需要白灯提示；blink=true 时按行呼吸 */
void setHostHintMask(uint16_t mask, bool blink);
```

   渲染优先级：`点击高亮 > 槽位提示灯 > rgb_mode 常规`。
   呼吸相位直接复用 `elapsed_ms_`，照抄 `RGB_PULSE_MODE` 的 `cosf` 写法（[RGBLightControl.cpp#L318-L324](../src/rgb/RGBLightControl.cpp#L318-L324)）。

   > **实现坑（必须处理）**：`RGB_NONE_MODE` 分支有短路 `if (mask == last_none_mask_) return;`（[RGBLightControl.cpp#L224-L227](../src/rgb/RGBLightControl.cpp#L224-L227)）。闪烁需要**每帧重绘**，必须把「提示灯是否处于闪烁态」纳入短路判据（例如判据改为 `mask == last_none_mask_ && !host_blink_active_`），否则关灯模式下提示灯会一动不动。

2. 掩码由 `BleHostManager::hostHintMask(resolver)` 计算：遍历 `key_id` 1..11，取其**活动 FUN 层**的 function 串，命中 `KEY_FUNCTION_HOST_n` **且** `slotState(n)` 为 `Disconnected` 或 `Switching` → 置位 `led = key_id - 1`；`Unused` / `Connected` 一律不置位（未配对不亮）。

3. **跨任务投递**：`RGBLightControl::tick()` 由 `DisplayTask` 驱动，而 `BleHostManager` 在 MainTask。所以掩码不能每帧查询，改为 MainTask 在下列时机重算 + 经 `DisplayMessageType::HostSlotChanged` 投递掩码到 DisplayTask：
   - 槽位表变化（首配 / clear / 改名）
   - 连接 / 断开 / 进入或退出切换窗口
   - `reloadKeymap()`（键映射变化后功能串变了）
   - `applyWorkMode()`（BLE ↔ USB 互切）
   - `activeFunLayer()` 变化（按下/松开 FUN 键，见 MainTask.cpp#L723-L730 同款触发点）

## 7. 协议与 App / Web

### 7.1 新命令 `CMD_BLE_HOST = 0x18`（三端同步）

`SerialProtocol.h` / `protocol.rs` / `commands.ts` 三处枚举同时追加 `0x18`。风格对齐 `CMD_AUDIO_PAD`：**一个命令 + `op` 分发**。

**App/Web → 设备**

```json
{"cmd":24,"seq":1,"data":{"op":"list"}}
{"cmd":24,"seq":2,"data":{"op":"rename","slot":2,"label":"办公台式机"}}
{"cmd":24,"seq":3,"data":{"op":"select","slot":2}}
{"cmd":24,"seq":4,"data":{"op":"clear","slot":2}}
```

**设备 → App/Web（响应 = `0x18|0x80`；状态变化时也主动推同一结构）**

```json
{
  "cmd":152, "seq":1, "status":0, "data":{
    "op":"list",
    "active": 2,
    "slots":[
      {"index":1,"used":true,"label":"MacBook","addr":"AA:BB:CC:DD:EE:01","type":"public","state":"disconnected"},
      {"index":2,"used":true,"label":"办公台式机","addr":"AA:BB:CC:DD:EE:02","type":"public","state":"connected"},
      {"index":3,"used":false,"label":"","addr":"","type":"","state":"unused"},
      {"index":4,"used":false,"label":"","addr":"","type":"","state":"unused"}
    ]
  }
}
```

`state` 取值固定为 `"connected" | "disconnected" | "switching" | "unused"`（对应 `BleSlotState`）。
主动推送不带 `seq`（对齐 `CMD_VOICE_TEXT` 风格）。**改名/切换/重置成功后必须回推一次全量 `list`**，避免三端各自维护增量状态。

### 7.2 `cmd_config`（0x07）快照扩展

新增两个字段（静态信息，供三端首屏渲染，避免必须发一次 `list` 才有内容）：

- `ble_host_active`：1 基，`0` = 未连接
- `ble_host_slots`：`[{index, label, addr, used}]`（**不含**实时 `state`，实时状态走 7.1）

> App 侧 `EKeysApp/src/protocol.rs` 的 `FIELD_COUNT`/`F_*` 增量字段编号表需同步追加，否则 `diff` / `merge_push` 会漏字段。

### 7.3 固件落点

| 动作 | 文件 |
| --- | --- |
| cmd id 枚举追加 `CMD_BLE_HOST = 0x18` | [`src/protocol/SerialProtocol.h#L26-L48`](../src/protocol/SerialProtocol.h#L26-L48) |
| 新增 `registerBleHostHandlers()` / `handleBleHost()`（`op` 分发） | 新增 `src/protocol/commands/cmd_ble_host.h/.cpp` |
| 注册进总表 | [`src/protocol/registration.cpp#L25-L36`](../src/protocol/registration.cpp#L25-L36) |
| 主动推送槽位状态 | 复用 `SerialProtocol` 现有推送写法（参考 `pushVoiceText`，[SerialProtocol.cpp#L172](../src/protocol/SerialProtocol.cpp#L172)） |
| `cmd_config` 快照补 `ble_host_*` | [`src/protocol/commands/cmd_config.cpp`](../src/protocol/commands/cmd_config.cpp) |
| `[bluetooth]` 段 load/save | `src/config/DeviceSettings.h`、`Configuration.h/.cpp`、`parseConfigSetCommand.cpp` |

### 7.4 App（EKeysApp / Rust + egui）落点

| 动作 | 文件 |
| --- | --- |
| `Page` 枚举加 `Bluetooth` | [`src/state/mod.rs#L100-L110`](file:///d:/search/esp/Keys/EKeysApp/src/state/mod.rs#L100-L110) |
| 侧边栏加 `NavItem`（`nav.bluetooth` + 新图标 `NAV_BLUETOOTH`） | [`src/ui/sidenav.rs#L19-L79`](file:///d:/search/esp/Keys/EKeysApp/src/ui/sidenav.rs#L19-L79) |
| 按 `Page` 路由到新面板 | [`src/app.rs#L389-L479`](file:///d:/search/esp/Keys/EKeysApp/src/app.rs#L389-L479) |
| 新增设备管理面板（列表 / 改名输入框 / 切换按钮 / 状态徽标 / 重置） | 新增 `src/ui/panel_bluetooth.rs` |
| `CMD_BLE_HOST = 0x18` 常量 + 槽位结构体 + list/rename/select/clear 编解码 | [`src/protocol.rs#L117-L167`](file:///d:/search/esp/Keys/EKeysApp/src/protocol.rs#L117-L167)、`#L307-L366` |
| `function_label()` 增加 4 条友好名（如「切换主机：MacBook」用槽位 label 渲染） | [`src/protocol.rs#L1644`](file:///d:/search/esp/Keys/EKeysApp/src/protocol.rs#L1644) |
| 固件功能下拉常量表追加 4 个 `KEY_FUNCTION_HOST_n` | [`src/ui/panel_keymap.rs#L2811`](file:///d:/search/esp/Keys/EKeysApp/src/ui/panel_keymap.rs#L2811) |
| i18n 文案（`nav.bluetooth*` / `ble.*` / `proto.function.host1..4`） | `src/i18n/`（按 `keymap.rs` 同款注册） |

> 键映射面板当前在侧边栏被注释停用（[sidenav.rs#L27-L34](file:///d:/search/esp/Keys/EKeysApp/src/ui/sidenav.rs#L27-L34)），但 `panel_keymap.rs` 仍在维护——下拉项照加，待入口回滚即可生效。

### 7.5 Web（EKeysWeb / React + Vite）落点

| 动作 | 文件 |
| --- | --- |
| `CMD` 常量加 `BLE_HOST: 0x18`，同步文件头「23 条命令」注释与 `IMPLEMENTED_CMDS` | [`src/protocol/commands.ts#L8-L31`](file:///d:/search/esp/Keys/EKeysWeb/src/protocol/commands.ts#L8-L31) |
| 新增编解码模块 `bleHost.ts`（list/rename/select/clear） | 新增 `src/protocol/bleHost.ts`，在 `src/protocol/index.ts` 导出 |
| 新增配置面板（设备列表 / 改名 / 切换 / 状态） | 新增 `src/components/BluetoothPanel.tsx` |
| 面板注册进配置区导航 | [`src/App.tsx#L27-L51`](file:///d:/search/esp/Keys/EKeysWeb/src/App.tsx#L27-L51) 的 lazy import + `components/ConfigLayout` 分区表 |
| i18n 文案 | `src/i18n/`（与 App 端 key 命名保持一致，便于对照） |

> **两端功能串字符串与协议字段名必须逐字节一致**（项目已踩过不一致导致功能失效的坑）。建议把 4 个 `KEY_FUNCTION_HOST_n` 与 `state` 字面量在 App / Web 各写一处常量表，不要散落。

## 8. 风险与对策

| # | 风险 | 对策 |
| --- | --- | --- |
| 1 | 旧主机抢连（断开后先连上的是刚断的那台） | 切换场景一律走白名单广播，只允许目标地址建连（对 RPA 主机白名单可能失效，降级策略见风险 2）；常规掉线场景才全放行 |
| 2 | 主机使用**可解析私有地址（RPA）**（Windows / macOS 默认），每次连接 `remote_bda` 都变。影响两层：① 槽位按 `remote_bda` 直配失效，「按连接顺序占空槽」会让同一台主机刷爆 4 槽；② **白名单只放目标地址对 RPA 主机无效**（`esp_ble_gap_update_whitelist` 不做 IRK 解析），目标主机可能永远连不进来 | 槽位匹配改为与 `esp_ble_get_bond_device_list()` 的 bond 表**对账**（bond 记录存配对时交换的身份地址），`remote_bda` 直配只作静态地址主机的快路径；若实测 bond 对账仍对不上 RPA 回连，切换降级为「全放行 + 先连上者裁决」（§4.2 switchTo 第 5 步）；提供 `clearSlot()` 供重配对。**实施顺序第 3 步必须先拿 Windows（默认 RPA）实测**，实测结果决定白名单方案去留 |
| 3 | 主机不自动回连（部分 Android 机型需手动点） | 10s 超时后回退全放行广播 + UI 明确提示；不重试死循环 |
| 4 | 断开瞬间主机侧卡键 | `switchTo` 内第一件事就是 `releaseAll()` |
| 5 | 切换中切到 USB 模式（`applyWorkMode`） | `BleHostManager::reset()`：清 pending、停超时定时器、不再动广告；提示灯掩码同步清零 |
| 6 | 定向广播 1.28s 规范超时 | **默认不使用**；仅在白名单方案在某主机实测不通时作为备选：`adv_type = ADV_TYPE_DIRECT_IND_LOW` + `peer_addr` + 裸调 `esp_ble_gap_start_advertising()`，并在 `ESP_GAP_BLE_ADV_TERMINATED_EVT` 里重挂 |
| 7 | 存储：NVS 仅 20KB（`partitions-16MB.csv`） | 槽位表放 `config.ini`（SPIFFS），与现有配置一致；NVS 只留给 Bluedroid 存 bond（4 条无压力） |
| 8 | BLE + WiFi 共存内存 | 运行时链路数仍为 1，内存/功耗与现状基本一致；多 bond 只占 NVS |
| 9 | 提示灯在关灯模式（`RGB_NONE_MODE`）不闪 | `RGB_NONE_MODE` 的掩码短路判据必须并入"闪烁态"（见 §6.2 实现坑） |
| 10 | 多颗「已配对未连接」的键同时呼吸，观感吵 | 呼吸相位按 LED 索引错开（`+ i * 0.35f`），避免整块同步闪；亮度下限 10% 保证可见但不刺眼。**未配对槽位不参与提示**，所以实际同闪数量取决于已配对台数（≤4） |
| 11 | 改名 label 含中文被截断成半个字 | 按 UTF-8 字符边界截断，`label[32]` 上限（中文 10 字）；App/Web 输入框同步 `maxlength` 提示 |
| 12 | 三端状态不一致（App 切了，Web 仍显示旧状态） | 改/切/清成功后固件**回推全量 list**；三端收到推送即整表覆盖，不做增量合并 |
| 13 | 槽位键按住不放导致日志刷屏 | `switchTo` 失败路径日志按 1s 节流 |
| 14 | 断链后灯先亮后灭的时序抖动 | 提示灯掩码以 `BleHostManager` 状态为准，且只在状态**边沿**投递消息，不每 tick 投递 |

## 9. 多链路能力预留（本期不启用）

按 §1 边界，本期只做单链路，但保留三个后门，将来启用多链路**不需要重构**：

1. `BleHostSlot` 常驻 `conn_id`（本期只用于断开定位，多链路时即链路标识）。
2. `BleHostManager` 内留 `LinkPolicy { Single, Multi }`（默认 `Single`）；`Single` 的语义是「新连接进来时断开非活动 peer / 停止广播」。
3. 发送路径抽象：将来把 `BLEKeyboardImpl` 的 `press/release/type` 从「库的 `_keyReport` + `notify()`」改为「自管报告 + `esp_ble_gatts_send_indicate(gatts_if, conn_id, handle, ...)`」，即成为 `Multi` 的实现。
   > 注意 `BleKeyboard.h#L133-L150` 的 `inputKeyboard` / `_keyReport` 为 private，子类无法复用库的状态机；多链路时必须自管 6 键报告与 modifier（现有 `tracked_` / `s_mod_count` 已是一半基础）。句柄可用 `BLEDevice::getServer()->getServiceByUUID(0x1812)->getCharacteristic(BLEUUID(0x2A4D))` 取（`BLEService::getCharacteristic` 与 `BLECharacteristic::getHandle` 均为 public），但键盘报告与多媒体报告**同 UUID 0x2A4D**，需靠 0x2908 descriptor 的 report ID 区分。

## 10. 实施顺序（每步可独立验证）

1. `BleHostSlots` 槽位表 + 持久化 + 改名：串口日志验证「首配写入 → 改名 → 重启后保留」。
2. `EKeysBleKeyboard` 子类接管连接/断开：日志打印 `remote_bda` / `addr_type` / `conn_id`（先不改广告策略）。
3. 白名单广播 + `switchTo`：先用临时串口命令触发，实测断开 → 目标回连 → 超时回退。**首测主机必须包含 Windows（默认 RPA）**：验证回连地址能否与 bond 表对账、白名单能否让目标建连；任一不成立则按 §8 风险 2 降级为「全放行 + 裁决」，确认后再继续后续步骤。
4. 功能串 `KEY_FUNCTION_HOST_1..4` + MainTask 拦截（含 FUN 层通道）；USB 模式验证不发 HID。
5. 槽位状态灯：`RGBLightControl::setHostHintMask()` + 掩码计算 + 消息投递；重点验证 `RGB_NONE_MODE` 下能闪。
6. 状态栏槽位显示（走 DisplayMessage）。
7. 协议 `CMD_BLE_HOST` + `cmd_config` 扩展 + 状态主动推送；用串口 JSON 手测四种 `op`。
8. App 设备管理页 + 键映射下拉项 + i18n。
9. Web 设备管理页 + 协议模块 + i18n。

## 11. 文件清单

**固件 — 新增**

- `src/output/BleHostSlots.h` / `.cpp`
- `src/output/BleHostManager.h` / `.cpp`
- `src/protocol/commands/cmd_ble_host.h` / `.cpp`

**固件 — 修改**

- `src/output/BLEKeyboardImpl.h` / `.cpp`（子类 + 事件接管 + `switchHostTo` / `setHostLabel` / `hostHintMask` 转发）
- `src/utils/keymap_types.h`（4 个功能串常量）
- `src/keymap/KeyEventDispatcher.h` / `.cpp`（如需在 Dispatcher 侧同步功能串识别）
- `src/tasks/MainTask.cpp`（功能串拦截 + 掩码重算与投递 + `BleHostManager::tick()` 轮询）
- `src/rgb/RGBLightControl.h` / `.cpp`（提示灯 override 层 + 短路判据修正）
- `src/config/DeviceSettings.h`、`Configuration.h` / `.cpp`、`parseConfigSetCommand.cpp`（`[bluetooth]` 段 load/save）
- `src/protocol/SerialProtocol.h` / `.cpp`（`CMD_BLE_HOST` + 状态推送）
- `src/protocol/registration.cpp`（注册新 handler）
- `src/protocol/commands/cmd_config.cpp`（`ble_host_active` / `ble_host_slots` 上报）
- `src/message_types.h`（`DisplayMessageType::HostSlotChanged`，载荷掩码 + 槽位状态）
- `src/ui/ui_StatusBar.h` / `.c`、`src/tasks/DisplayTask.cpp`（槽位显示 + 调 `setHostHintMask`）

**App — 新增/修改**

- 新增 `src/ui/panel_bluetooth.rs`
- `src/state/mod.rs`、`src/ui/sidenav.rs`、`src/app.rs`、`src/ui/icons.rs`
- `src/protocol.rs`、`src/ui/panel_keymap.rs`、`src/i18n/`

**Web — 新增/修改**

- 新增 `src/components/BluetoothPanel.tsx`、`src/protocol/bleHost.ts`
- `src/protocol/commands.ts`、`src/protocol/index.ts`、`src/App.tsx`、`src/components/ConfigLayout.tsx`、`src/i18n/`

**协议文档**

- [`docs/desktop-app-protocol.md`](desktop-app-protocol.md)（固件侧权威）：追加 `0x18 CMD_BLE_HOST` 与 `0x07` 新增字段说明

## 12. 验收清单

- [ ] 首次连接 2 台及以上主机后重启设备，槽位表与「上次活动主机」保留。
- [ ] App / Web 能列出全部已配对设备，改名后重启两端仍显示新名称；另一端打开时也能看到同一名称。
- [ ] App / Web 点「切换」与按物理槽位键效果一致：旧主机不再收到按键，新主机正常响应，无卡键。
- [ ] **Windows 主机（默认 RPA）配对后多次断开重连，槽位不重复占用**（bond 表对账命中同一槽）。
- [ ] 切换到 Windows RPA 主机能成功建连；若白名单失效走降级裁决，仍能在超时窗口内连上目标。
- [ ] 切换超时后回连的是非目标主机（含原主机）时，正常接入并回推 list，状态栏 / App / Web 文案明确「已回连 <label>（非目标）」，不停留在「连接中」。
- [ ] 连接状态展示与实际一致，覆盖 `connected` / `disconnected` / `switching` / `unused` 四态；设备侧状态变化后两端自动刷新。
- [ ] 配了 `KEY_FUNCTION_HOST_n` 的键：槽位 n **已配对但未连接**时该键白色呼吸闪烁；连接成功后立即停止闪烁。
- [ ] 配了 `KEY_FUNCTION_HOST_n` 但槽位 n **尚未配对**时，该键**不亮**（不闪烁、也不被提示灯点亮）。
- [ ] 多颗键同时闪烁时相位错开，不同步整块闪。
- [ ] 关灯模式（`rgb_mode = 0`）下提示灯仍能正常呼吸。
- [ ] 按住该键的瞬间点击高亮覆盖提示灯，松开后恢复。
- [ ] 同一功能串配置在 FUN1 / FUN2 层同样生效（含提示灯）。
- [ ] USB 模式下按该键不产生 HID，也不闪烁。
- [ ] 切换窗口内按任何键不产生 HID 输出到任何主机（`releaseAll` + 未连接守卫）。
- [ ] 切换超时后回退全放行广播，状态栏与 App/Web 都不停留在「连接中」。
- [ ] 命名含中文被正确截断，不出现半个汉字导致 App 显示乱码。
- [ ] 无已配对主机时按槽位键不崩溃、无日志刷屏。
