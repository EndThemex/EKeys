# 阶段 13 — InputRouter 实现方案

> 状态：Step 0~4 已实施 · 2026-10-19
> 关联：[12-fun-features §6.1](./12-fun-features.md) · [ARCHITECTURE §3.3](../ARCHITECTURE.md) · [11-home-assistant-native-api](./11-home-assistant-native-api.md)
> 说明：按项目规则不主动编译；本文给出可评审、可分步落地的实现方案。

---

## 1. 目标与非目标

### 1.1 目标

1. 把"当前屏幕 → 矩阵键由谁消费"从 `MainTask` 的两组 bool 收口为一张路由表。
2. 明确 4 类消费者：`Keymap`（HID）、`Ui`（LVGL）、`Ha`（binary_sensor）、`Foreground`（游戏/乐器/宠物）。
3. **按下时记录 owner，释放时按记录释放**，解决跨屏释放错误（HA 卡 on、ASR 误结束、HID 卡键）。
4. 为 Foreground 提供独立的小队列，不挤占现有 `DisplayMessage` 队列。
5. 不改变任务边界：MainTask 执行 HID/HA/队列写入，DisplayTask 执行 UI/GameHost。
6. 现有 13 屏的 **press 行为保持不变**；release 仅在跨屏/非 Keymap 场景按"按下时 owner"修正。

### 1.2 非目标

- 不实现具体游戏；只留 `GameHost` 接口和空屏。
- 不把 `KeyResolver` / HID 逻辑搬进 `InputRouter`。
- 不改旋钮的 UI 导航语义（Foreground 旋钮后续单独讨论）。
- 不新增 App/Web 协议；第一版 Foreground 由本地功能键进入。
- 不修 `KEYMAPPED_SECONDARY` 目前仍发 HID 的现状（见 §2.3，单独评估）。

---

## 2. 现状

### 2.1 路由散落点

[`MainTask.cpp`](../src/tasks/MainTask.cpp) 5ms tick：

```cpp
const bool ha_routing =
    tick_screen == UI_SCREEN_HA ||
    tick_screen == UI_SCREEN_HA_SECONDARY;
const bool suppress_hid =
    ha_routing ||
    tick_screen == UI_SCREEN_SETTING_SECONDARY ||
    tick_screen == UI_SCREEN_AUDIO_SECONDARY;
```

[`DisplayTask.cpp`](../src/tasks/DisplayTask.cpp) `ActionInput`：

```cpp
if (tag == UI_SCREEN_KEYMAPPED) { ... }
else if (tag == UI_SCREEN_KEYMAPPED_SECONDARY ||
         tag == UI_SCREEN_SETTING_SECONDARY) { ... }
else if (tag == UI_SCREEN_AUDIO_SECONDARY) { ... }
// 其它屏：矩阵键为 HID 专用，不触发 UI 导航
```

同一套"屏幕语义"被两处分别维护，新增玩法屏需要同时改两边。

### 2.2 现有行为矩阵

| 屏幕                            | 矩阵键 HID | FUN 预扫描 | function 页跳转 | HA 边沿 | ActionInput     |
| ------------------------------- | ---------- | ---------- | --------------- | ------- | --------------- |
| MAIN                            | ✓          | ✓          | ✓               | —       | ✓（UI 忽略）    |
| KEYMAPPED                       | ✓          | ✓          | ✓               | —       | ✓（截胡进二级） |
| KEYMAPPED_SECONDARY             | ✓          | ✓          | ✓               | —       | ✓（焦点）       |
| MUSIC / MUSIC_SECONDARY         | ✓          | ✓          | ✓               | —       | ✓（忽略）       |
| AUDIO                           | ✓          | ✓          | ✓               | —       | ✓（忽略）       |
| AUDIO_SECONDARY                 | —          | —          | —               | —       | ✓（音效板）     |
| PC_STATUS / PC_STATUS_SECONDARY | ✓          | ✓          | ✓               | —       | ✓（忽略）       |
| HA / HA_SECONDARY               | —          | —          | —               | ✓       | ✓（忽略）       |
| SETTING                         | ✓          | ✓          | ✓               | —       | ✓（忽略）       |
| SETTING_SECONDARY               | —          | —          | —               | —       | ✓（设置 UI）    |
| UNKNOWN（启动首帧）             | ✓          | ✓          | ✓               | —       | ✓               |

> 注意：`KEYMAPPED_SECONDARY` 目前 **仍发 HID**，只有 `SETTING_SECONDARY` / `AUDIO_SECONDARY` / HA 被屏蔽。这是现状，本方案不改变。

### 2.3 三个问题

1. **双写**：路由知识分散在 MainTask 和 DisplayTask，新屏容易漏改。
2. **跨屏释放错误**：release 用"当前屏"判断，而不是"按下时是谁消费的"。
   - 在主页按下键 3（HID `c` 已按下）→ 旋钮切到 HA 屏 → 松开：当前代码会同时发 HA `false` 和 HID `release`；HA 收到一个从未 `true` 的边沿。
   - 在 HA 屏按下键 1 → 切回主页 → 松开：当前代码发 HID release，但 HID 从未 press；同时 HA 实体可能永远停在 on（直到 HA 断链）。
   - 在音效二级页按下语音触发键 → 松开：当前代码会 `finishCapture()`，可能误结束正在进行的 ASR。
3. **Foreground 无入口**：如果游戏边沿走 `DisplayMessage`，队列长度 10、单条约 1.2KB（见 `message_types.h` 注释），快速连按会丢边沿。

---

## 3. 设计

### 3.1 类型

新增 `src/input/InputRoute.h`（纯 POD，不依赖 Arduino/LVGL，便于 host 测试）：

```cpp
namespace ekeys
{
    /* 与 MatrixScanner::kMatrixKeyCount 对齐；MainTask.cpp 做 static_assert */
    inline constexpr uint8_t kInputKeyCount = 11;

    enum class InputOwner : uint8_t
    {
        None = 0,
        Keymap,     // KeyEventDispatcher + KeyResolver → HID
        Ui,         // 只投 ActionInput → LVGL
        Ha,         // HaApiService binary_sensor
        Foreground, // 前台玩法（GameHost）
    };

    struct MatrixRoute
    {
        InputOwner owner{InputOwner::Keymap};
        bool fun_prescan{true}; // 只对 Keymap 有意义
        bool ui_action{true};   // 是否投 101~111 的 ActionInput
    };
}
```

### 3.2 路由表

`InputRouter::routeFor(screen)` 的唯一事实源：

| 屏幕               | owner        | fun_prescan | ui_action | 依据                                                                         |
| ------------------ | ------------ | ----------- | --------- | ---------------------------------------------------------------------------- |
| HA / HA_SECONDARY  | `Ha`         | false       | **true**  | 不发 HID；保留 ActionInput 以触发 `bumpActivity()`，否则 HA 屏 5s 自动回主页 |
| SETTING_SECONDARY  | `Ui`         | false       | true      | 矩阵键只做焦点/调值                                                          |
| AUDIO_SECONDARY    | `Ui`         | false       | true      | 矩阵键触发音效板                                                             |
| GAME（新增）       | `Foreground` | false       | false     | 矩阵键交给 GameHost；游戏屏禁用自动回主页                                    |
| 其余（含 UNKNOWN） | `Keymap`     | true        | true      | 现有 HID 主路径                                                              |

实现：

```cpp
MatrixRoute InputRouter::routeFor(ui_screen_tag_t screen)
{
    switch (screen)
    {
    case UI_SCREEN_HA:
    case UI_SCREEN_HA_SECONDARY:
        return {InputOwner::Ha, false, true};
    case UI_SCREEN_SETTING_SECONDARY:
    case UI_SCREEN_AUDIO_SECONDARY:
        return {InputOwner::Ui, false, true};
    case UI_SCREEN_GAME:
        return {InputOwner::Foreground, false, false};
    default:
        return {InputOwner::Keymap, true, true};
    }
}
```

### 3.3 API

新增 `src/input/InputRouter.{h,cpp}`：

```cpp
class InputRouter
{
public:
    void begin();
    void reset();

    /* 返回 true = 屏幕发生变化；MainTask 只在进出 Foreground 时 releaseAll */
    bool updateScreen(ui_screen_tag_t screen);
    ui_screen_tag_t screen() const { return last_screen_; }

    static MatrixRoute routeFor(ui_screen_tag_t screen);
    static bool isForeground(ui_screen_tag_t screen);

    /* 按下：返回路由并记录 owner；释放：用记录值，跨屏不变 */
    MatrixRoute onPress(uint8_t key, ui_screen_tag_t screen);
    MatrixRoute onRelease(uint8_t key, ui_screen_tag_t screen);

private:
    InputOwner pressed_owner_[kInputKeyCount + 1]{};
    ui_screen_tag_t last_screen_{UI_SCREEN_UNKNOWN};
};
```

`onPress` / `onRelease`：

```cpp
MatrixRoute InputRouter::onPress(uint8_t key, ui_screen_tag_t screen)
{
    const MatrixRoute route = routeFor(screen);
    if (key >= 1 && key <= kInputKeyCount)
    {
        pressed_owner_[key] = route.owner;
    }
    return route;
}

MatrixRoute InputRouter::onRelease(uint8_t key, ui_screen_tag_t screen)
{
    MatrixRoute route = routeFor(screen);
    route.fun_prescan = false;
    route.ui_action = false; // release 不投 UI

    if (key >= 1 && key <= kInputKeyCount)
    {
        if (pressed_owner_[key] != InputOwner::None)
        {
            route.owner = pressed_owner_[key];
        }
        pressed_owner_[key] = InputOwner::None;
    }
    return route;
}
```

### 3.4 边沿语义

**按下（press）**

| owner        | 动作                                                                                 |
| ------------ | ------------------------------------------------------------------------------------ |
| `Keymap`     | `KeyEventDispatcher::onKeyEdge(true)` → function 页跳转判断 → `KeyResolver::press()` |
| `Ui`         | 无 HID；仅后续 ActionInput                                                           |
| `Ha`         | `HaApiService::sendKeyState(key, true)`                                              |
| `Foreground` | `ForegroundKeyQueue::push({key, true, millis()})`                                    |

所有 owner 在 `route.ui_action == true` 时都投 `ActionInput`（101~111），保持现有 UI/自动回主页行为。

**释放（release）**

| 按下时 owner | 动作                                                              |
| ------------ | ----------------------------------------------------------------- |
| `Keymap`     | `KeyEventDispatcher::onKeyEdge(false)` + `KeyResolver::release()` |
| `Ui`         | 无（未发过 HID）                                                  |
| `Ha`         | `HaApiService::sendKeyState(key, false)`                          |
| `Foreground` | `ForegroundKeyQueue::push({key, false, millis()})`                |

**不再无条件发 HID release**：对现有 13 屏，这是本方案唯一的行为变化，目的是让 release 与 press 配对；跨屏场景仍正确，见 §3.5。

**FUN 预扫描**

只在 `routeFor(screen).fun_prescan == true` 时执行（即 Keymap 屏）。HA/Ui/Foreground 不参与 FUN 组合层。

**function 页跳转**

仍属于 Keymap 通道：按下时 owner 记为 `Keymap`。即使该键最终只跳页面、没发 HID，release 仍走 `KeyEventDispatcher(false)` + `KeyResolver::release()`：

- 若同一物理键是语音触发键，release 能正常 `finishCapture()`；
- `KeyResolver::release()` 对未按下的映射发 release，主机侧无副作用；
- 保持 press/release 配对，不引入新状态。

**旋钮**

保持现状：始终 `ActionInput(LV_KEY_*)`，不进 HID、不进 Foreground。  
游戏如果需要旋钮，后续给 `InputRouter` 加 `encoderRoute`，或在 `GameHost` 里消费 `LV_KEY_*`，不在本方案范围内。

**屏幕切换**

MainTask 每 5ms 读 `ui_get_active_screen_tag()`：

```cpp
const ui_screen_tag_t prev = router_.screen();
if (router_.updateScreen(screen) &&
    (InputRouter::isForeground(prev) || InputRouter::isForeground(screen)))
{
    keyboard_->releaseAll(); // 只在前台进出时清 HID，避免跨屏卡键
}
```

不在每次切屏都 `releaseAll()`：那会破坏"按住某键 + 旋钮切屏"的现有行为。只有进入/离开 Foreground 时清，因为游戏期间不应该让 HID 键保持按下。

### 3.5 跨屏释放示例

| 场景                           | 按下时 owner | 切到的屏幕      | 释放动作                              | 结果                |
| ------------------------------ | ------------ | --------------- | ------------------------------------- | ------------------- |
| 主页按 3（HID `c`）→ HA → 松开 | Keymap       | HA              | HID release `c`                       | 正确释放，不误发 HA |
| HA 按 1 → 主页 → 松开          | Ha           | MAIN            | HA `false`                            | HA 实体不卡 on      |
| 音效二级页按语音键 → 松开      | Ui           | AUDIO_SECONDARY | 无                                    | 不会误结束 ASR      |
| 主页按语音键 → 游戏 → 松开     | Keymap       | GAME            | `finishCapture()`                     | 正常结束录音        |
| 游戏按 1 → 主页 → 松开         | Foreground   | MAIN            | 前台 release（GameHost 已退出则丢弃） | 不发 HID            |

### 3.6 Foreground 事件队列

不复用 `DisplayMessage`：

- 队列长度 10、单条约 1.2KB；游戏一次按下+释放就是 2 条，快速连按会丢边沿。
- 新增 `src/input/ForegroundKeyQueue.{h,cpp}`，SPSC 环形队列：

```cpp
struct ForegroundKeyEvent
{
    uint8_t key{0};
    bool pressed{false};
    uint32_t timestamp_ms{0};
};

class ForegroundKeyQueue
{
public:
    static constexpr size_t kCapacity = 32;
    static ForegroundKeyQueue &instance();

    bool push(const ForegroundKeyEvent &ev); // MainTask
    bool pop(ForegroundKeyEvent &out);        // DisplayTask
    void clear();

private:
    ForegroundKeyEvent buf_[kCapacity]{};
    volatile uint32_t head_{0};
    volatile uint32_t tail_{0};
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
};
```

- 容量 32 × 8B ≈ 256B；`portMUX` 临界区保护 head/tail。
- 满时丢弃新事件并计数（`dropped_`），日志节流；游戏可接受极少数丢帧，胜过 1.2KB 消息堆积。
- 时间戳在 MainTask 按下瞬间取 `millis()`，供节奏/反应类游戏判定。
- DisplayTask 每轮 `run()` 排空：

```cpp
ForegroundKeyEvent ev;
while (ForegroundKeyQueue::instance().pop(ev))
{
    GameHost::instance().onKey(ev.key, ev.pressed, ev.timestamp_ms);
}
```

### 3.7 GameHost 边界

`GameHost` 只负责"前台玩法"：

```cpp
class GameHost
{
public:
    static GameHost &instance();
    void enter(ui_screen_tag_t from);
    void exit();
    void onKey(uint8_t key, bool pressed, uint32_t timestamp_ms);
    void tick(uint32_t dt_ms);
    bool active() const;
    lv_obj_t *screen(); // 惰性创建
};
```

- 跑在 DisplayTask（Core 0）；
- 只读 `ForegroundKeyQueue`，不碰 HID/HA/协议；
- 音效通过 `SoundService::request()`（后续阶段）转 MainTask；
- 退屏由 `DisplayTask::navigateNow()` 调用 `GameHost::exit()`；
- 游戏屏不参与 5s 自动回主页。

---

## 4. 代码改动清单

### 4.1 新文件

| 文件                                  | 作用                                                                 |
| ------------------------------------- | -------------------------------------------------------------------- |
| `src/ui/ui_screen_tag.h`              | 从 `ui_KeyMapped.h` 拆出 `ui_screen_tag_t`，去掉 LVGL 依赖，便于测试 |
| `src/input/InputRoute.h`              | `InputOwner` / `MatrixRoute` / `kInputKeyCount`                      |
| `src/input/InputRouter.h/.cpp`        | 路由表 + 按下 owner 记录                                             |
| `src/input/ForegroundKeyQueue.h/.cpp` | 前台按键 SPSC 队列（阶段 3）                                         |
| `src/game/GameHost.h/.cpp`            | 前台宿主接口 + 空实现（阶段 3）                                      |
| `src/ui/game/ui_GameScreen.{h,c}`     | 程序化创建的游戏屏（阶段 3，不走 SquareLine）                        |

> `ui_screen_tag.h` 的拆分是可选但推荐：`InputRouter` 只依赖一个 enum，不再拖入 `lvgl.h`，host 单测更容易。  
> `ui_KeyMapped.h` 保留 `#include "ui_screen_tag.h"`，删除原 enum 定义。

### 4.2 MainTask

1. `MainTask.h` 增加：
   ```cpp
   #include "input/InputRouter.h"
   // ...
   InputRouter router_;
   ```
2. `MainTask::begin()` 调 `router_.begin()`；`end()` 调 `router_.reset()`。
3. 5ms tick 替换路由块（见 §4.4 伪代码）。
4. `MainTask.cpp` 顶部加：
   ```cpp
   static_assert(kInputKeyCount == kMatrixKeyCount,
                 "InputRouter key count mismatch");
   ```
5. 删除 `ha_routing` / `suppress_hid` 局部变量。

### 4.3 DisplayTask

1. `isAutoReturnEnabled()` 增加：
   ```cpp
   if (InputRouter::isForeground(tag)) return false;
   ```
2. `navigateNow()`：
   ```cpp
   const ui_screen_tag_t prev = ui_get_active_screen_tag();
   if (prev == UI_SCREEN_GAME && tag != UI_SCREEN_GAME)
       GameHost::instance().exit();
   if (tag == UI_SCREEN_GAME && prev != UI_SCREEN_GAME)
       GameHost::instance().enter(prev);
   ```
   `case UI_SCREEN_GAME: target = GameHost::instance().screen();`
3. `run()` 每轮排空 `ForegroundKeyQueue`（见 §3.6），并在 `LvglPort::tick()` 之后、`GameHost::active()` 时调用 `GameHost::tick(delta)`。
4. `applyMessage(ActionInput)` 保持现有 switch 不变；Foreground 不走 ActionInput，因此无需加分支。

### 4.4 MainTask 5ms tick 伪代码

```cpp
const ui_screen_tag_t screen = ui_get_active_screen_tag();

const ui_screen_tag_t prev = router_.screen();
if (router_.updateScreen(screen) &&
    (InputRouter::isForeground(prev) || InputRouter::isForeground(screen)))
{
    keyboard_->releaseAll();
}

const MatrixRoute screen_route = InputRouter::routeFor(screen);
if (screen_route.fun_prescan)
{
    /* 现有 FUN 预扫描循环，原样保留 */
}

for (uint8_t i = 0; i < pc; ++i)
{
    const uint8_t key = pressed[i];
    const MatrixRoute route = router_.onPress(key, screen);

    switch (route.owner)
    {
    case InputOwner::Ha:
        HaApiService::instance().sendKeyState(key, true);
        break;
    case InputOwner::Foreground:
        ForegroundKeyQueue::instance().push({key, true, millis()});
        break;
    case InputOwner::Ui:
        break;
    case InputOwner::Keymap:
    default:
    {
        KeyEventDispatcher::onKeyEdge(key, true);
        ui_screen_tag_t jump_target = UI_SCREEN_MAIN;
        if (functionKeyJumpTarget(resolver_, key, jump_target))
        {
            DisplayMessage jump_msg{};
            jump_msg.type = DisplayMessageType::Navigate;
            jump_msg.navigate_target = static_cast<uint8_t>(jump_target);
            postMessage(jump_msg);
        }
        else
        {
            resolver_.press(key, *keyboard_);
        }
        break;
    }
    }

    if (route.ui_action)
    {
        DisplayMessage nav_msg{};
        nav_msg.type = DisplayMessageType::ActionInput;
        nav_msg.action = static_cast<uint8_t>(kMatrixKeyActionBase + key);
        postMessage(nav_msg);
    }
}

for (uint8_t i = 0; i < rc; ++i)
{
    const uint8_t key = released[i];
    const MatrixRoute route = router_.onRelease(key, screen);

    switch (route.owner)
    {
    case InputOwner::Ha:
        HaApiService::instance().sendKeyState(key, false);
        break;
    case InputOwner::Foreground:
        ForegroundKeyQueue::instance().push({key, false, millis()});
        break;
    case InputOwner::Ui:
        break;
    case InputOwner::Keymap:
    default:
        KeyEventDispatcher::onKeyEdge(key, false);
        resolver_.release(key, *keyboard_);
        break;
    }
}
```

### 4.5 UI tag / 功能键

- `ui_screen_tag.h` 末尾加 `UI_SCREEN_GAME`（放在 `UI_SCREEN_SETTING_SECONDARY` 之后，避免已有值偏移）。
- `functionKeyJumpTarget()` 增加：
  ```cpp
  else if (fk == "KEY_FUNCTION_PAGE_GAME") out = UI_SCREEN_GAME;
  ```
- App/Web 的功能键下拉列表后续补 `KEY_FUNCTION_PAGE_GAME`；第一版可先用固定物理键/调试入口。
- `platformio.ini` 无需改：`build_src_filter` 的 `+<*>` 已覆盖 `src/input` / `src/game`。

### 4.6 文档同步

落地后已同步：

- [`ARCHITECTURE.md`](../ARCHITECTURE.md) §3.3：已改为实际文件与职责；
- [`.trae/rules/rules.md`](../.trae/rules/rules.md) §3 / §8：已增加 InputRouter 路由表，明确"矩阵键 HID 专用"只对 Keymap owner 成立；
- [`docs/12-fun-features.md`](./12-fun-features.md) §6.1：已链接本文。

---

## 5. 分步实施

| 步骤 | 内容                                                                         | 影响                                     | 验收                                       |
| ---- | ---------------------------------------------------------------------------- | ---------------------------------------- | ------------------------------------------ |
| 0    | 拆 `ui_screen_tag.h`；给现有 13 屏跑一遍手工基线                             | 无行为变化                               | 编译通过；13 屏导航/按键与改动前一致       |
| 1    | 新增 `InputRoute.h` / `InputRouter.{h,cpp}`，只加不调用                      | 无                                       | host 表单测通过（或 on-device 表驱动自检） |
| 2    | MainTask 接入 router，替换 `ha_routing`/`suppress_hid`                       | **唯一行为变化**：release 按按下时 owner | 6 类回归测试全过（见 §6）                  |
| 3    | DisplayTask 接入 `isForeground`；加 `ForegroundKeyQueue` + `GameHost` 空实现 | 新屏可达，无 HID                         | 进出 GAME 无卡键、无 WDT                   |
| 4    | 在 GameHost 上实现 Simon（第一个游戏）                                       | 新功能                                   | 可玩、退出恢复 HID                         |

建议每个步骤单独提交，步骤 2 出问题可直接 revert，不影响 0/1。

---

## 6. 测试

### 6.1 自动测试

`test/test_input_router.cpp`（Unity，host 或 on-device 均可）：

```cpp
void test_route_table()
{
    auto main = InputRouter::routeFor(UI_SCREEN_MAIN);
    TEST_ASSERT_EQUAL(InputOwner::Keymap, main.owner);
    TEST_ASSERT_TRUE(main.fun_prescan);
    TEST_ASSERT_TRUE(main.ui_action);

    auto ha = InputRouter::routeFor(UI_SCREEN_HA);
    TEST_ASSERT_EQUAL(InputOwner::Ha, ha.owner);
    TEST_ASSERT_FALSE(ha.fun_prescan);

    auto setting = InputRouter::routeFor(UI_SCREEN_SETTING_SECONDARY);
    TEST_ASSERT_EQUAL(InputOwner::Ui, setting.owner);

    auto game = InputRouter::routeFor(UI_SCREEN_GAME);
    TEST_ASSERT_EQUAL(InputOwner::Foreground, game.owner);
}

void test_release_uses_press_owner()
{
    InputRouter router;
    router.begin();

    router.onPress(3, UI_SCREEN_MAIN);
    auto r = router.onRelease(3, UI_SCREEN_HA);
    TEST_ASSERT_EQUAL(InputOwner::Keymap, r.owner); // 不是 Ha

    router.onPress(1, UI_SCREEN_HA);
    r = router.onRelease(1, UI_SCREEN_MAIN);
    TEST_ASSERT_EQUAL(InputOwner::Ha, r.owner); // 不是 Keymap
}
```

覆盖：

- 14 个屏幕 × route 字段（15 个 tag，含 UNKNOWN / GAME）；
- press → 切屏 → release 的 owner 保持；
- 未按下直接 release（fallback 当前 route，不崩溃）；
- key 0 / 12 越界不崩溃；
- `begin()` / `reset()` 清空 owner。

### 6.2 手工回归矩阵

| 用例              | 步骤                                 | 预期                                                 |
| ----------------- | ------------------------------------ | ---------------------------------------------------- |
| 普通打字          | 主页按 1~11                          | HID 字符正常，无延迟变化                             |
| FUN 组合          | 按住 FUN1/FUN2 + 其它键              | 组合层生效，FUN 键本身无 HID                         |
| function 页跳转   | 配置 `KEY_FUNCTION_PAGE_*` 后按键    | 跳二级页，不发 HID                                   |
| KEYMAPPED         | 主页旋钮进键映射屏，按 1~11          | 跳二级并聚焦，同时 HID 仍输出（现状）                |
| SETTING_SECONDARY | 设置二级页按 7/11                    | 焦点移动，无 HID                                     |
| AUDIO_SECONDARY   | 音效二级页按 1~11                    | 播放对应音效，无 HID                                 |
| HA 屏             | HA 屏按 1~11                         | HA binary_sensor 翻转，无 HID；5s 内按键不自动回主页 |
| 跨屏释放          | 主页按住 3 → 旋钮到 HA → 松开        | HID `c` 释放，HA 无虚假边沿                          |
| HA 跨屏释放       | HA 按住 1 → 回主页 → 松开            | HA 实体转 off，不卡 on                               |
| 语音跨屏          | 主页按语音键 → 进游戏 → 松开         | ASR 正常结束，不误触发                               |
| 游戏进出          | 功能键进 GAME，按 1~11，双击旋钮退出 | 无 HID；退出后打字正常；无卡键                       |
| 自动回主页        | 一级页 5s 无操作                     | 行为与改动前一致（HA 屏按键可续命）                  |

### 6.3 资源与稳定性

- `DisplayMessage` 不变，UI 队列长度不变；
- `ForegroundKeyQueue` 约 256B 静态内存；
- `InputRouter` 约 16B（12×owner + last_screen）；
- 连续 1 小时游戏/打字无 WDT、无 LVGL OOM。

---

## 7. 风险与回滚

| 风险                                       | 说明                                     | 对策                                                                          |
| ------------------------------------------ | ---------------------------------------- | ----------------------------------------------------------------------------- |
| `ui_screen_tag.h` 被 SquareLine 重导出覆盖 | 该文件是从生成头拆出的                   | 文件头注释 `// CUSTOM: do not regenerate`；`ui_KeyMapped.h` 只保留 include    |
| release 语义变化                           | 从"总是 HID release"改为"按按下时 owner" | 步骤 2 单独提交；用 §6.2 跨屏用例回归；必要时 revert                          |
| HA release 不再走 `KeyEventDispatcher`     | 按下在 HA 的键不会误结束 ASR             | 这是修正；若发现依赖旧行为，改为 release 时额外调 `KeyEventDispatcher(false)` |
| KEYMAPPED_SECONDARY 仍发 HID               | 现状不是本方案引入                       | 本文不改；如需修，另开任务并同步 `.trae/rules`                                |
| Foreground 队列丢事件                      | 容量 32，极端连按可能满                  | 丢弃新事件 + `dropped_` 计数 + 节流日志；后续可扩到 64                        |
| `InputRouter` 依赖 UI enum                 | input → ui 的反向依赖                    | 拆 `ui_screen_tag.h` 后只依赖 enum，不依赖 LVGL                               |
| 新增屏忘记配路由                           | default 走 Keymap，可能误发 HID          | `ui_screen_tag.h` 注释 + code review checklist + 单测覆盖所有枚举值           |
| 编译风险                                   | 按项目规则不主动编译                     | 实施时由你明确触发 `pio run`，再联调                                          |

**回滚**：

- 步骤 0/1 无行为变化，可保留；
- 步骤 2 是唯一行为改动，revert 该提交即可回到旧 `ha_routing`/`suppress_hid`；
- 步骤 3/4 是新增屏，删除 `UI_SCREEN_GAME` 分支即可停用。

---

## 8. 后续扩展

1. **旋钮进 Foreground**：`InputRouter` 增加 `encoderRoute`，游戏可声明 `wants_encoder`；否则旋钮继续 UI 导航。
2. **ModA/ModB 模块输入**：`InputSource::Module`，复用 owner 概念，特殊输入 ID 走同一路由表。
3. **Pet / Instrument owner**：新增 `InputOwner::Pet` / `Instrument`，或复用 `Foreground` + `GameHost` 子模式。
4. **App/Web 功能键列表**：补 `KEY_FUNCTION_PAGE_GAME` 与游戏参数。
5. **自动化**：给 `InputRouter` 加 host 单测环境（`[env:native]`），避免每次上板验证路由表。

---

## 9. 实施记录（2026-10-19）

已完成 Step 0~4：

| 步骤 | 实际改动                                                                                                                                                              |
| ---- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 0    | 新增 `src/ui/ui_screen_tag.h`；`ui_KeyMapped.h` 改为 include；追加 `UI_SCREEN_GAME`                                                                                   |
| 1    | 新增 `src/input/InputRoute.h`、`InputRouter.h/.cpp`                                                                                                                   |
| 2    | `MainTask.h/.cpp` 接入 `InputRouter`；删除 `ha_routing` / `suppress_hid`；release 按按下时 owner                                                                      |
| 3    | 新增 `ForegroundKeyQueue`、`GameHost`、`ui_GameScreen`；DisplayTask 排空队列、`isForeground` 禁用自动回主页、`navigateNow` 处理 GAME；功能键 `KEY_FUNCTION_PAGE_GAME` |
| 4    | `GameHost` 实现 Simon Says；`ui_GameScreen` 加底部 11 格指示条（见 §9.2）                                                                                             |

未做：

- host 单测文件：设计见 §6.1，尚未加入 `test/`；
- `pio run` 编译与上板联调：按项目规则未主动编译，等确认后执行；
- Simon 音效 / RGB 联动 / 最高分持久化（依赖 SoundService、玩法存储，见 docs/12）。

### 9.1 试玩入口

当前 App/Web 的功能键下拉还没加 `KEY_FUNCTION_PAGE_GAME`，可直接改 `data/keymapN.ini`：

```ini
[key1]
function_key=KEY_FUNCTION_PAGE_GAME
```

重新 uploadfs / 重启后按该键进入 GAME 屏；双击旋钮退出。  
验证重点：进入游戏后按键不再发 HID；退出后普通打字、FUN、HA、设置二级页均正常。

### 9.2 Simon Says（Step 4）

规则与节奏（`src/game/GameHost.h` 常量）：

- 任意键开局；每轮序列随机追加 1 键（1~11，硬件 RNG），上限 32 轮通关；
- 回放：单步点亮 420ms + 间隔 160ms，回放中按键忽略；
- 跟按：总超时 5s 判负；按错判负；按对即时点亮对应格，回合边界统一熄灭；
- 结算：成绩 = 已完成轮数，结算屏锁定 900ms 防误触，任意键重开；best 为本次开机内最好成绩（RAM，不持久化）。

实现约束：

- 全部计时在 `tick(dt)` 内推进（无阻塞等待），事件只来自 `ForegroundKeyQueue`（只消费 press 边沿）；
- 屏显：中央状态文本（montserrat_24）+ 底部 11 格指示条（`ui_GameScreen_set_cell`，琥珀点亮/暗灰熄灭）；
- 随机数用 Arduino `random()`（ESP32 核心底层走 `esp_random` 硬件 RNG，无需播种）；
- 音效 / RGB 联动 / 最高分持久化留待 SoundService 与玩法存储阶段（docs/12）。

---

## 变更记录

| 日期       | 变更                                                                                     |
| ---------- | ---------------------------------------------------------------------------------------- |
| 2026-10-19 | 初稿：现状矩阵、四 owner 路由、按下 owner 记录、Foreground SPSC 队列、四步实施与回归矩阵 |
| 2026-10-19 | Step 4 落地：GameHost 实现 Simon Says，ui_GameScreen 加 11 格指示条                      |
