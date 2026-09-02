# EKeys 交互输入分配规则（v1）

> 本文档定义一套"页面 → 输入"的统一映射规则，目的是让任意页面的按键 / 旋钮行为**可预测、可复用、可扩展**。
> 适用范围：`src/ui/**` 所有 `Page` 子类；新增页面必须按本文档归类后填表实现。

---

## 1. 设计目标

1. **跨页一致**：用户在任意页都"知道 KEY3 是什么"，零学习成本。
2. **新页可模板化**：新增页面只需归类 + 填表，无须重新拍脑袋。
3. **不破坏既有路由**：KEY1/KEY2 的全局语义保持不变。

---

## 2. 输入资源总览

### 2.1 物理输入

| 输入                | 数量 | 来源                            |
| ------------------- | ---- | ------------------------------- |
| 矩阵按键 KEY1..KEY9 | 9    | [07-pinout.md §3](07-pinout.md) |
| EC11 编码器旋转     | 1    | [07-pinout.md §4](07-pinout.md) |
| EC11 编码器单击     | 1    | [07-pinout.md §4](07-pinout.md) |

合计 **11 个动作**。

### 2.2 全局固定常量（任何页不许重定义）

| 输入     | 行为                         | 出处                                                     |
| -------- | ---------------------------- | -------------------------------------------------------- |
| KEY1     | **Back**：弹栈；栈空则回主页 | [PageManager.cpp §handleKeyPress](../ui/PageManager.cpp) |
| KEY2     | **Confirm / 主动作**         | 同上                                                     |
| 旋钮单击 | **= KEY2**（同语义）         | 同上                                                     |

> 这三条是"硬常量"，新增页面**不得**为 KEY1/KEY2/旋钮单击重新指派含义。

---

## 3. 页面类型分类（PageKind）

把页面归为 5 类，每类直接绑定输入语义。**新增页面第一步就是归类**。

| 类型            | 标识       | 典型场景     | 主要交互模型                        |
| --------------- | ---------- | ------------ | ----------------------------------- |
| **L** List      | 列表选择   | Menu、KeyMap | 在 N 个条目中选 1 个                |
| **M** Mode      | 多模式控制 | RGB 控制     | 在多种"模式"间切换 + 当前模式内调值 |
| **S** State     | 流程状态机 | 番茄钟       | 状态机步进 + 当前态参数调节         |
| **A** Action    | 即时动作   | BLE 控制     | 1 个主动作 + 多个快捷动作           |
| **R** Read-only | 只读展示   | Mic / Status | 无交互                              |

> 同一页面只能是 1 种类型；如确实需要混合（例如"列表选 + 即时动作"），按"主交互"归类，副交互用 `onSelectKey` 单独处理，并写明理由。

---

## 4. 输入绑定表（核心规则）

### 4.1 KEY1 / KEY2 / 旋钮单击

| 类型 | KEY1         | KEY2 / 旋钮单击                        |
| ---- | ------------ | -------------------------------------- |
| L    | back（固定） | **进入当前选中项**                     |
| M    | back（固定） | **切换到下一个模式**                   |
| S    | back（固定） | **状态机步进**（启动/暂停/恢复/重置…） |
| A    | back（固定） | **主动作**（默认动作）                 |
| R    | back（固定） | 无                                     |

### 4.2 旋钮旋转

| 类型 | 旋钮旋转语义                                  |
| ---- | --------------------------------------------- |
| L    | 选中项**上下移动**（循环）                    |
| M    | 在当前模式内**调值**（增/减/子项切换）        |
| S    | 调节当前态的**关键参数**（如时间）            |
| A    | 调节**二级参数**（主动作的强度 / 细粒度选项） |
| R    | 无                                            |

### 4.3 KEY3..KEY9

含义对用户**始终一致**：KEY N = "第 N 个 / 直选 / 直发"，按类型自动绑定。

| 类型 | KEY3..KEY9 含义                                                |
| ---- | -------------------------------------------------------------- |
| L    | **直接跳到第 N 项**（1-indexed，KEY3 → 选第 3 项）             |
| M    | **直接进入第 N 个模式**（KEY3 → mode[0]，KEY4 → mode[1]…）     |
| S    | **直接进入第 N 个状态**（KEY3 → state[0]，KEY4 → state[1]…）   |
| A    | **直接触发第 N 个动作**（KEY3 → action[0]，KEY4 → action[1]…） |
| R    | 无                                                             |

> 槽位超过实际数量时，未用 KEY 视为"无操作"，但**不得**回卷到有效项（避免误触）。

---

## 5. 底部 hint 文案模板

每页底部 hint 按类型模板生成，避免散落与漂移。

| 类型 | hint 模板                               |
| ---- | --------------------------------------- |
| L    | `KNOB pick  K2 enter  K3..K9 jump`      |
| M    | `KNOB value  K2 next mode  K3..K9 mode` |
| S    | `KNOB adjust  K2 step  K3..K9 state`    |
| A    | `KNOB fine  K2 action  K3..K9 action`   |
| R    | `K1 back`                               |

> 实际页面可在模板后追加自定义片段（如"click=start"），但前缀必须匹配。

---

## 6. 决策清单（新增页必填）

```
页面名:        ___________
PageKind:      L / M / S / A / R
KEY1 作用:     back（固定）
KEY2 作用:     [enter / next mode / state step / action / none]
旋钮单击:      = KEY2
旋钮旋转:      [select / adjust / param / fine / none]
KEY3..KEY9:    [jump / mode N / state N / action N / none]
底部 hint:     按 §5 模板
```

---

## 7. 现有页面归类核对

| 页面       | 当前实现                                                | 应归类 | 一致性                                                |
| ---------- | ------------------------------------------------------- | ------ | ----------------------------------------------------- |
| MenuPage   | KEY2=进入、旋钮=上下选、KEY3..KEY9 未用                 | L      | 旋钮/KEY2 一致；建议**启用 KEY3..KEY9 直选**          |
| RgbPage    | KEY2=切模式、旋钮=调值、KEY3..KEY9 未用                 | M      | 一致；建议**启用 KEY3..KEY5 直选模式**                |
| TomatoPage | KEY2=状态机步进、旋钮=调时间、KEY3..KEY9 未用           | S      | 一致；建议**启用 KEY3..KEY6 直选状态**                |
| BlePage    | KEY2=进入 KeyMap、KEY3=toggle                           | A      | 一致；KEY4..KEY9 可继续扩展为 reconnect/clear bond 等 |
| KeyMapPage | KEY2=下一 profile、旋钮=选 keyId、KEY3..KEY9=跳选 keyId | L      | 一致（KEY3..KEY9 已是"直选"）                         |
| MicPage    | 无交互                                                  | R      | 一致                                                  |
| StatusPage | 无交互                                                  | R      | 一致                                                  |

---

## 8. 与全局路由的边界

| 全局职责（[PageManager.cpp](../ui/PageManager.cpp)） | 本规则职责                                         |
| ---------------------------------------------------- | -------------------------------------------------- |
| KEY1 = pop                                           | 一致                                               |
| KEY2 = `Page::onConfirm()`                           | 一致                                               |
| KEY3..KEY9 = `Page::onSelectKey(keyId)`              | **由 Page 内部按类型路由**，PageManager 不感知类型 |
| 旋钮单击 = `Page::onConfirm()`                       | 一致                                               |
| 旋钮旋转 = `Page::onEncoder(delta)`                  | 一致                                               |

> PageManager 保持"无类型"转发；类型识别与路由逻辑**只**存在于 `Page` 子类内部，避免 PageManager 越权。

---

## 9. 落地建议（代码侧）

1. 在 `Page.h` 增加：
   ```cpp
   enum class PageKind : uint8_t { List, Mode, State, Action, ReadOnly };
   virtual PageKind kind() const = 0;
   ```
2. 每个 `*Page.cpp` 实现 `kind()` 返回上表归类。
3. `Page` 基类提供默认实现：
   - `onSelectKey(keyId)`：按 `kind()` 默认路由到"直选第 N 项/模式/状态/动作"。
   - 子类只需声明"第 N 个是什么"，不必自己写 if-else。
4. 基类提供 `buildHintLabel(PageKind k)`，按 §5 模板生成 hint 前缀。
5. `PageManager::handleKeyPress` 维持当前逻辑（KEY1/KEY2 硬编码 + 其它转发），**不改动**。

---

## 10. 反模式（禁止）

- 在 KEY1 上绑定非"返回"动作。
- KEY2 在不同页指向完全不同的语义而没有"主动作"共性。
- 同一页内 KEY3..KEY9 同时出现多种含义（如 KEY3 = toggle、KEY4 = 直选、KEY5 = 调值）。
- 旋钮单击与 KEY2 语义不一致。
- 底部 hint 与实际行为不符。
- 把类型路由写进 PageManager。

---

## 10.1 工程陷阱记录

### 头文件 `inline` 变量陷阱（2026-09 修复）

**症状**：

```
src/ble/BleKeyMap.h:103:36: warning: inline variables are only available
with -std=c++17 or -std=gnu++17
     inline uint8_t BLE_ROTATE_MAP[3] = {
```

**根因**：

- `inline` 变量是 **C++17** 引入的特性，编译器在 C++14/11 下只能 warning + 退化为每个 TU 一份拷贝 → ODR 风险。
- 本项目 [platformio.ini](../../platformio.ini) **没有显式指定 C++ 标准**，arduino 框架默认走 C++11，所以三个"必须在头文件里定义 + 运行时可写"的数组（`BLE_KEY_MAP[10]` / `BLE_ENCODER_MAP[4]` / `BLE_ROTATE_MAP[3]`）一编译就触发警告。

**为什么不能直接升级到 C++17**：

- 升标准会影响全部依赖（`lvgl@8.3.11`、`FastLED@3.6.0`、`ESP32-BLE-Keyboard`、`arduinoFFT`、`ESP32Encoder`），任何一个不兼容都会连环爆。
- 本项目只是需要"头文件里声明一个全局数组并允许运行时覆写"，没必要为这个单独抬高一档标准。

**正确做法（已落地）**：

> 凡是"**头文件需要可见 + 运行时可写**"的全局数组，**必须用 `extern` 声明 + 在某个 .cpp 里给出唯一定义**。

| 模式                                         | 适用场景                     | 标准要求          |
| -------------------------------------------- | ---------------------------- | ----------------- |
| `static constexpr uint8_t arr[N] = {...}`    | 只读、所有元素都是常量表达式 | C++11             |
| `inline uint8_t arr[N] = {...}`              | 头文件可见 + 可写            | **C++17**（勿用） |
| `extern uint8_t arr[N];` + `.cpp` 内唯一定义 | 头文件可见 + 可写            | **C++11 即可** ✓  |

修改示例见 [ble/BleKeyMap.h](../../ble/BleKeyMap.h) 与 [ble/BleKeyMap.cpp](../../ble/BleKeyMap.cpp)：

- 头文件里改成 3 行 `extern uint8_t BLE_xxx[N];`；
- `.cpp` 里给出**唯一定义**，初值与原 inline 版本完全一致（Numpad profile 默认值）；
- 运行时仍由 `refreshMapsFromActiveProfile()` 按当前 profile 覆盖，行为不变。

**自检清单（提交前必查）**：

1. 在头文件搜 `inline\s+(uint|int|float|bool|char)\w+\s+\w+\s*\[`，命中即警告自己"确认是 C++17"。
2. 若搜到的变量是**可写的**（被某个函数赋值 / `=` 拷贝），必须改成 `extern` + .cpp 定义。
3. 若搜到的变量是**只读**且所有元素都是常量表达式，直接用 `static constexpr` 数组。
4. 升 C++17 是最后手段，且必须先全量编译验证所有依赖。

---

---

## 11. 版本

| 版本 | 日期       | 变更                                    |
| ---- | ---------- | --------------------------------------- |
| v1   | 2026-09-02 | 初版：定义 5 类 PageKind 与输入绑定规则 |
