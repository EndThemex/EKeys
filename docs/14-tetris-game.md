# 阶段 14 — 游戏菜单与俄罗斯方块（竖屏）

> 状态：**已实现（2026-11-19）**，待实机编译/烧录验证。本轮按方案落地了全部代码，未执行 pio 编译（遵循 `.trae/rules/rules.md`）。
> 2026-11-19 初稿：基于现有 SquareLine/LVGL 8.3 屏幕框架、旋钮输入语义、WS2812B 灯效单一渲染点设计。
> 关联文档：[`05-ui-screens.md`](./05-ui-screens.md) / [`06-network-voice-rgb-audio.md`](./06-network-voice-rgb-audio.md) / [`13-ai-mode-image-streaming.md`](./13-ai-mode-image-streaming.md) / [`.trae/rules/rules.md`](../.trae/rules/rules.md)
> 关联代码：[`ui_types.h`](../src/ui/ui_types.h) / [`ui.c`](../src/ui/ui.c) / [`DisplayTask.cpp`](../src/tasks/DisplayTask.cpp) / [`MainTask.cpp`](../src/tasks/MainTask.cpp) / [`RotaryEncoder.cpp`](../src/input/RotaryEncoder.cpp) / [`LvglPort.cpp`](../src/display/LvglPort.cpp) / [`DisplayDriver.cpp`](../src/display/DisplayDriver.cpp) / [`RGBLightControl.cpp`](../src/rgb/RGBLightControl.cpp)

## 1. 目标与范围

在现有 9 个一级页的旋钮轮播中插入 **GAMES 游戏菜单页**，单击旋钮进入 **游戏选择二级页**（旋转选游戏，当前只有「俄罗斯方块」可选，另一槽位为敬请期待占位），再次单击进入 **俄罗斯方块游戏页**。游戏页把当前这块 NV3007 屏幕切成 **竖屏 142×428** 使用；竖屏下实体按键位于屏幕左侧，屏幕上按实体顺序画出 **3 列 × 4 行键位图**（末行两键 + 旋钮），色块颜色与对应 WS2812B 灯色一一对应。

**做**：

- 新增 3 个页面：游戏菜单（一级）、游戏选择（二级）、俄罗斯方块游戏页（竖屏）。
- 完整俄罗斯方块玩法：7-bag 随机、SRS 旋转 + 踢墙、幽灵块、软降/硬降、锁定延迟、消行计分、等级加速、Hold、暂停、结束重开。
- 旋钮 + 矩阵 11 键双套操作；按键用不同颜色 LED 标识，屏幕左侧键位图同色标注（K9/K10/K11 也分配功能）。

**不做**（避免范围膨胀）：

- 不改 HID 键映射、键位 Profile、App 协议（`docs/desktop-app-protocol.md` 的 0x01~0x17 命令）。
- 不新增 FreeRTOS 任务、不新增 LVGL 输入设备（仍无触屏）。
- 不持久化最高分到 SPIFFS/NVS（单局内计分，掉电即失）。
- 不实现联网对战、音效（如需可后续接 `AudioPad`，本期不接）。

## 2. 固件基线（约束条件）

| 项目 | 实测/配置值 | 依据 |
| --- | --- | --- |
| MCU | ESP32-S3 N16R8，双核 240MHz，8MB OPI PSRAM | `platformio.ini` |
| 屏幕 | NV3007 物理 **142(W)×428(H)**，构造时 `rotation=1` → 逻辑 **428×142**（横屏） | `DisplayDriver.cpp:52-64` |
| 旋转机制 | `Arduino_TFT::setRotation(r)`：偶数 → 142×428，奇数 → 428×142；偏移随 r 变（见 §6） | `lib/GFX.../Arduino_TFT.cpp:147-170` |
| LVGL | 8.3.11，`LV_COLOR_DEPTH=16`、`LV_COLOR_16_SWAP=0`，`LV_MEM_SIZE=128KB`（PSRAM 池） | `include/lv_conf.h`、`ui.c:18-23` |
| 绘制缓冲 | 单缓冲 16 行 = 428×16×2 ≈ 13.7KB，**内部 RAM**；flush 同步阻塞 | `LvglPort.cpp:26-28,141-167` |
| 输入 | EC11 旋钮：CW→`LV_KEY_RIGHT`、CCW→`LV_KEY_LEFT`、单击→`LV_KEY_ENTER`(10)、双击→`LV_KEY_ESC`(27)；矩阵键 1~11 → `100+key_id`(101~111) | `RotaryEncoder.cpp`、`rules.md §1-2` |
| 屏幕导航 | 各一级页 `LV_EVENT_KEY` 里直接 `_ui_screen_change`；轮播链见 `rules.md`，插入点见 §4 | `ui_*Screen.c` |
| 事件路由 | `DisplayTask::applyMessage(ActionInput)` 是输入唯一改写点；其它屏矩阵键默认丢弃（HID 专用） | `DisplayTask.cpp:360-427` |
| HID 屏蔽先例 | `MainTask::loop()` 中 `suppress_hid` 已覆盖 SETTING_SECONDARY / AUDIO_SECONDARY / HA | `MainTask.cpp:641-645` |
| 灯效 | `RGBLightControl::renderFrame()` 是 LED 唯一写入点，30ms 一帧，ClickHighlight 做 override | `RGBLightControl.cpp:200-245` |
| 自动回主页 | 一级页 5s 无操作回主页；二级页豁免；二级功能页（AI）另有常亮豁免 | `DisplayTask.cpp:861-893,914-934` |
| 页面常驻 | `ui_init()` 一次性创建全部屏幕，切屏不销毁；`ui_destroy()` 统一销毁 | `ui.c:31-78` |

**关键结论**：竖屏不是「在横屏里画一个竖的棋盘」，而是把整块屏的 GFX rotation 与 LVGL 逻辑分辨率一起切换；这是全局状态，必须保证「只在这一个游戏页期间为竖屏」，离开即恢复，否则其它 9 屏会错位。

## 3. 总体设计

### 3.1 页面流转

```
                ┌──────────────── 旋钮轮播（横屏 428×142）────────────────┐
                ▼                                                          │
   MAIN ──RIGHT──▶ KEYMAPPED ──▶ MUSIC ──▶ AUDIO ──▶ AI ──▶ [GAMES] ──▶ PC_STATUS ──▶ HA ──▶ SETTING ──┐
    ▲                                                        │                                          │
    └────────────────────── LEFT（反向）───────────────────────┘◀──────────────────────────────────────────┘

   [GAMES]  单击ENTER ──▶  GAME_SELECT（游戏选择二级页，横屏）
                │              │ 旋转：在「俄罗斯方块」/「敬请期待」间切换高亮
                │              │ 单击ENTER ──▶ TETRIS（切竖屏 142×428）
                │              │ 双击ESC  ──▶ 回 [GAMES]
                │              ▼
                │         TETRIS 游戏页（竖屏）
                │              │ 双击ESC ──▶ 暂停；暂停中单击ENTER继续；暂停中双击ESC ──▶ 退出回 [GAMES]
                └── 5s 自动回主页（仅一级页）◀──────────────┘
```

轮播插入点只改 2 行现有代码：`ui_AiScreen.c` 的 RIGHT 从 `PC_STATUS` 改为 `GAMES`，`ui_PcStatusScreen.c` 的 LEFT 从 `AI` 改为 `GAMES`；`GAMES` 页自身 LEFT→AI、RIGHT→PC_STATUS。不破坏既有环形顺序。

### 3.2 分层与职责

| 层 | 新增模块 | 职责 | 依赖方向 |
| --- | --- | --- | --- |
| 纯逻辑 | `src/game/TetrisCore.h/.cpp` | 棋盘、方块、7-bag、SRS、消行、计分、等级、状态机。**不 include lvgl / Arduino** | 无 |
| 游戏门面 | `src/game/TetrisGame.h/.cpp` | 持有 Core；驱动重力时钟；把 Core 状态画到画布/HUD；输入分派；LED 调色板开关；对外 C 接口 | Core + UI + RGB + LvglPort |
| 页面 | `src/ui/ui_GameScreen.*`、`ui_GameSelectScreen.*`、`ui_TetrisScreen.*` | 只做 LVGL 控件布局与 `LV_EVENT_KEY` 导航；游戏页只暴露绘图原语 | `TetrisGame.h`（C 链接门面） |
| 显示 | `DisplayDriver::setRotation()`、`LvglPort::setPortrait()` | 横竖屏切换的底层原语 | GFX / LVGL |
| 灯效 | `RGBLightControl::setGamePalette()/clearGamePalette()` | 游戏中 LED 固定色覆盖，优先级最高 | RGBDriver |
| 编排 | `DisplayTask.cpp` / `MainTask.cpp` 少量插入 | 输入路由、每帧 tick、竖屏进入/退出时机、HID 屏蔽 | 上述全部 |

设计原则：**玩法逻辑与 LVGL 解耦**（Core 可单测）、**UI 文件保持 C**（与现有 SquareLine 产物一致）、**跨核只传 `ui_screen_tag_t` 与 `ActionInput`**（沿用既有无锁原子读约定，见 `rules.md §7`）。

## 4. 新增页面设计

### 4.1 游戏菜单页（一级，横屏 428×142）

对齐 `ui_AiScreen.c` 的「大图标 + 标题」样式，不引入新视觉语言：

- 图标：80×80 圆角描边方块，内写 `G`（`ui_font_BebasNeueFont48`）。
- 标题：`GAMES` / 中文 `游戏`（`ui_font_FontCKJGT24`，跟随 `ui_lang`）。
- 底部小字：`LEFT: AI   RIGHT: PC   CLICK: SELECT`（静态提示，避免用户迷路）。
- 按键：`LEFT→AI`、`RIGHT→PC_STATUS`、`ENTER→GAME_SELECT`、`ESC→MAIN`。

### 4.2 游戏选择二级页（横屏 428×142）

- 标题 `SELECT GAME` / `选择游戏`。
- 两个条目（为将来扩展留位置，本期第二项不可进入）：
  1. `俄罗斯方块 / TETRIS` — 可选，左侧色块用方块拼图色（青/黄/紫）。
  2. `敬请期待 / COMING SOON` — 置灰，ENTER 时弹 1.2s toast「即将上线」，不切页。
- 选中项：高亮底色 + 左侧指示条；旋转只改 `selected_index`（0/1），不触发任何 HID。
- 按键：`LEFT/RIGHT→切换选中`、`ENTER→启动选中游戏`、`ESC→回 GAMES`。
- 5s 自动回主页豁免（二级页）。

### 4.3 俄罗斯方块游戏页（竖屏 142×428）

进入本页时先把屏幕切成竖屏（当前 rotation=0），再 `lv_scr_load`。布局（坐标基于 142×428）：

```
      ← 实体键在屏幕左侧 →          ← 屏幕右侧 →
y=0   ┌──────────────┬────────────────┐
      │ [K8出][K4软][K1左] │  SCORE        │  键位图：3 列 × 4 行，
      │ [K9逆][K5硬][K2右] │  LV / LN       │  与实体键位置一一对应
y=76  │ [K10重][K6停][K3转]│  HOLD / N1N2N3 │  色块 = 对应 LED 色
      │ [K11硬][K7持][旋钮] │                │
y=100 ├──────────────┴────────────────┤
      │      10×20 棋盘（cell=12px）    │  ← 120×240，x=11 居中；幽灵块用暗色描边
y=360 ├────────────────────────────────┤
      │ 连击提示                       │
y=384 │ 旋钮:移动/单击转 / 双击暂停      │
y=428 └────────────────────────────────┘
      状态覆盖层（PAUSED / GAME OVER）居中叠在棋盘上
```

- 实体键网格（竖屏、屏幕在右、按键在左，上→下 4 行 / 左→右 3 列）：

  | 左列（离屏最远） | 中列 | 右列（靠屏） |
  | --- | --- | --- |
  | K8 退出 | K4 暂停 | K1 重开 |
  | K9 左移 | K5 旋转 | K2 右移 |
  | K10 无 | K6 硬降 | K3 无 |
  | K11 无 | K7 无 | **EC11 旋钮** |

  该顺序在 `TetrisPalette.h::TETRIS_KEY_GRID_IDS` 集中定义，`ui_TetrisScreen.c` 的键位图与之一致；旋钮占末行第三格，无 LED。
  > 2026-11-19 调整：K1 重开 / K2 右移 / K4 暂停 / K5 旋转 / K6 硬降 / K8 退出 / K9 左移；K3 / K7 / K10 / K11 暂无功能。颜色按物理键固定不变，旋钮功能不变。
- 棋盘用 **`lv_canvas` + 索引 4-bit（16 色）**：120×240 → 画布约 14.5KB（含 16 色面板），在 128KB PSRAM 池内可控；若 4-bit 渲染异常退回 8-bit（约 29KB，仍可接受）。不做 200 个 `lv_obj` 方块（对象开销大、逐块刷新慢）。
- 只在**棋盘状态变化时**重绘画布（重力步进 / 移动 / 旋转 / 消行 / 落定），不是每个 LVGL 帧都画；HUD 仅在分数/等级变化时更新 label。
- 画布、label、色块控件全部挂在 `ui_TetrisScreen` 上；该屏**不注册 `LV_EVENT_KEY` 导航**（旋钮输入由 `DisplayTask` 直接送给 `tetrisGame_handleInput`，避免触发切屏）。
- 状态覆盖层用同一个 label（改 text + 显示/隐藏遮罩矩形），不另建屏幕。

### 4.4 操作与 LED 颜色映射

屏幕左侧键位图中每个色块颜色 = 对应物理键的 WS2812B 颜色（LED 下标 = 键 ID − 1）：

> 2026-11-19 功能调整（颜色按物理键固定不变，仅重定义功能）：**K1 重开 / K2 右移 / K4 暂停 / K5 旋转 / K6 硬降 / K8 退出 / K9 左移；K3 / K7 / K10 / K11 暂无功能**。旋钮功能保持原样。

| 功能（2026-11-19） | 物理键 | LED 下标 | 颜色（RGB，固定） | 屏幕色块 | 旋钮等价操作 |
| --- | --- | --- | --- | --- | --- |
| 重开 | K1 | 0 | 红 `(255,48,48)` | 红（标"重"） | —（GAME OVER 单击） |
| 右移 | K2 | 1 | 橙 `(255,140,0)` | 橙（标"右"） | 顺时针 |
| 暂无功能 | K3 | 2 | 黄 `(255,220,0)` | 黄（标"-"） | — |
| 暂停 | K4 | 3 | 绿 `(40,230,80)` | 绿（标"停"） | **双击** |
| 旋转 | K5 | 4 | 青 `(0,220,255)` | 青（标"转"） | **单击** |
| 硬降 | K6 | 5 | 蓝 `(60,120,255)` | 蓝（标"硬"） | — |
| 暂无功能 | K7 | 6 | 紫 `(180,90,255)` | 紫（标"-"） | — |
| 退出 | K8 | 7 | 品红 `(255,60,180)` | 品红（标"出"） | 暂停/GAME OVER 双击 |
| 左移 | K9 | 8 | 薄荷绿 `(120,255,180)` | 薄荷绿（标"左"） | 逆时针 |
| 暂无功能 | K10 | 9 | 琥珀 `(255,200,80)` | 琥珀（标"-"） | — |
| 暂无功能 | K11 | 10 | 青 `(0,220,255)` | 青（标"-"） | — |
| — | EC11 旋钮 | — | 无 LED | 灰色圆角块（末行第三格） | 移动/单击旋转/双击暂停 |

说明：

- 旋钮 CW/CCW 与单击是主操作；7 个矩阵键参与玩法，与旋钮共用同一套动作处理函数（`tetrisGame_handleInput`）。
- K3/K7/K10/K11 暂无功能：按键被忽略，LED 仍显示对应物理键色，屏幕色块显示占位符 `-`。
- 颜色来源集中放在 `src/game/TetrisPalette.h`（`TETRIS_ACTION_RGB_0~10`）与 `src/game/TetrisConfig.h::kActionColors[11]`，屏幕色块与 LED 读取同一份数据，杜绝两边不一致。颜色按下标 = key_id−1 固定，与功能无关。
- 游戏中 `RGBLightControl` 用 game palette 覆盖**全部** 11 颗灯，退出时 `clearGamePalette()` 恢复用户配置灯效。
- **字体**：原 `ui_font_FontCKJGT16` 字库缺 `转/软/逆/重/钮/退`，这 6 个字由 `src/ui/ui_font_TetrisFallback16.c` 提供，并通过 `ui_font_FontCKJGT16.fallback` 链补全；其余字符仍用原字体渲染。缺字生成自系统黑体，风格与原字体略有差异但可正常显示。

## 5. 俄罗斯方块玩法设计（TetrisCore）

### 5.1 数据模型

```cpp
// src/game/TetrisCore.h（纯 C++，无 LVGL / Arduino 依赖）
namespace ekeys { namespace tetris {

enum class Piece { I, O, T, S, Z, J, L, Count };
enum class State { Ready, Playing, Paused, Over };

struct Cell { int8_t x, y; };                 // 相对方块原点
constexpr uint8_t kCols = 10;
constexpr uint8_t kRows = 20;                 // 可见行；另留 2 行隐藏生成区 = 22
constexpr uint8_t kCellPx = 12;

class Core {
public:
    void reset(uint32_t seed);                // 7-bag 重新洗牌
    bool step(uint16_t dt_ms);                // 重力/锁定推进；返回 true 表示画面需重绘
    bool move(int8_t dx);                     // 左右移动，成功返回 true
    bool rotate(int8_t dir);                  // +1 CW / -1 CCW，SRS 踢墙
    void softDrop();                          // 下移一格并计 1 分/格
    void hardDrop();                          // 直落并计 2 分/格
    void hold();                              // 每块限一次
    void setSoftDropping(bool on);            // 长按 K4：重力间隔降到 40ms
    void togglePause();

    // 只读视图（UI 绘制用）
    uint8_t  cellAt(uint8_t x, uint8_t y) const;   // 0=空, 1..7=Piece+1, 8=幽灵
    Piece    activePiece() const;
    uint32_t score() const; uint8_t level() const; uint32_t lines() const;
    State    state() const;
    Piece    nextAt(uint8_t i) const;              // 预览 3 个
    bool     hasHold() const; Piece holdPiece() const;
private:
    // 棋盘、当前块坐标/旋转态、bag 队列、hold、锁定计时、连击等
};
}}
```

### 5.2 规则细节

| 项目 | 规则 |
| --- | --- |
| 随机 | 7-bag：每 7 个方块为一组不重复洗牌，避免长干旱期；`reset(seed)` 用 `millis()` 播种 |
| 旋转 | SRS：JLSTZ 与 I 各自 4×4 状态表 + 踢墙偏移表；O 不旋转位移 |
| 生成 | 在顶部隐藏行生成；生成即碰撞 → `State::Over` |
| 重力 | 等级表 `[800,720,630,550,470,380,300,220,160,120,90,70,55,45,35]` ms，等级 = `lines/10 + 1`，封顶 15 |
| 软降 | 长按 K4 → 40ms/格；手动按一次下移一格 +1 分 |
| 硬降 | 一次性落底 + 锁定，每格 2 分 |
| 锁定延迟 | 落地后 500ms 锁定；每次成功移动/旋转最多重置 15 次，防无限拖延 |
| 消行 | 单行 100 / 双行 300 / 三行 500 / 四行 800，×等级；连击 `50×combo×level` |
| Hold | 每块一次；交换当前块与 hold 块，已用标志在落定后清除 |
| 幽灵块 | 计算当前块直落终点，用暗色值 8 画在画布上（不计入碰撞） |
| 暂停 | 冻结重力时钟与输入（只接受继续/退出）；恢复时不清空计时避免瞬间连落 |
| 结束 | 显示 `GAME OVER` + 得分；ENTER 重开（`reset` 新种子）、双击回 GAMES |

### 5.3 输入分派（`DisplayTask::applyMessage` 新增分支）

```
if (tag == UI_SCREEN_TETRIS):
    action 为旋钮 LEFT/RIGHT/ENTER/ESC 或矩阵 101~111（全部 11 键）
    → tetrisGame_handleInput(action)
    → 不调用 lv_event_send(active_screen, LV_EVENT_KEY, ...)  ← 关键：避免误触导航
else if (tag == UI_SCREEN_GAME_SELECT):
    矩阵键丢弃；旋钮 LEFT/RIGHT 改选中项，ENTER 启动，ESC 返回（走屏幕自身事件）
else:
    保持现有路由不变
```

2026-11-19 功能映射（在 `tetrisGame_handleInput` 内分派）：

- PLAYING：ENTER/单击 或 K5 → 旋转；LEFT 或 K9 → 左移；RIGHT 或 K2 → 右移；ESC/双击 或 K4 → 暂停；K6 → 硬降；K1 → 重开；K8 → 退出；K3/K7/K10/K11 忽略。
- PAUSED：ENTER 或 K4 → 继续；K1 → 重开；ESC 或 K8 → 退出。
- GAME OVER：ENTER 或 K1 → 重开；ESC 或 K8 → 退出。
- 旋钮功能保持不变。HID 一律屏蔽（见 §7）。

## 6. 横竖屏切换方案（最高风险项）

### 6.1 底层原语

1. `DisplayDriver::setRotation(uint8_t r)`：封装 `gfx_->setRotation(r)`，须在 `begin()` 后调用。`Arduino_TFT::setRotation` 会同时改 `_width/_height` 和 `_xStart/_yStart`：
   - `r=1`（现状）：逻辑 428×142，偏移 `_xStart=ROW_OFFSET1(0)`、`_yStart=COL_OFFSET2(14)`。
   - `r=0`：逻辑 **142×428**（当前竖屏方向），偏移 `_xStart=COL_OFFSET1(12)`、`_yStart=ROW_OFFSET1(0)`。
   - `r=2`：逻辑 142×428（相对 `r=0` 旋转 180°），偏移 `_xStart=COL_OFFSET2(14)`、`_yStart=ROW_OFFSET2(0)`。
   - **当前取值**：2026-11-19 用户要求把竖屏方向翻转一次，`kPortraitRotation` 由 `2` 改回 `0`（`TetrisConfig.h`）。两个值只差 180°，如需切回只改这一个常量即可，玩法与输入代码零改动。
2. `LvglPort::setOrientation(bool portrait)`：
   - `disp_drv_.hor_res/ver_res` 改为 `142/428` 或 `428/142`；
   - 绘制缓冲**不需要重分配**（现有 6848 像素 > 142×16，足够覆盖竖屏单行）；
   - `lv_obj_invalidate(lv_scr_act())` + `lv_refr_now(NULL)` 强制下一帧按新分辨率刷新。
   - flush 回调无需改动：它用 LVGL 给的 `area` 坐标直接调 `gfx->draw16bitRGBBitmap`，GFX 内部按当前 rotation 映射物理像素。

### 6.2 切换时序（必须在 DisplayTask 线程内同步完成）

```
进入游戏：  GAME_SELECT 的 ENTER 回调
  ① tetrisGame_enter()
  ② DisplayDriver::setRotation(kPortraitRotation)
  ③ LvglPort::setOrientation(true)
  ④ ui_TetrisScreen_screen_init()（首次懒创建，按 142×428 布局）
  ⑤ lv_scr_load(ui_TetrisScreen); lv_refr_now(NULL)
  ⑥ ui_set_active_screen_tag(UI_SCREEN_TETRIS)
  ⑦ RGBLightControl::setGamePalette(...)
  ⑧ Core::reset(millis())

退出游戏：  PAUSED 双击 / GAME OVER 双击
  ① tetrisGame_exit()
  ② RGBLightControl::clearGamePalette()
  ③ DisplayDriver::setRotation(1); LvglPort::setOrientation(false)
  ④ lv_scr_load(ui_GameScreen); lv_refr_now(NULL)
  ⑤ ui_set_active_screen_tag(UI_SCREEN_GAMES)
```

- 关键约束：**②③ 与 ④ 必须在同一同步调用栈内完成**，中间不能被 `lv_timer_handler()` 插入，否则会有一帧「横屏内容画在竖屏分辨率」的花屏。入口在 UI 事件回调里（DisplayTask 线程），天然满足。
- 屏幕对象生命周期：`ui_TetrisScreen` 首次进入时懒创建（建立在竖屏坐标系），之后常驻；`ui_destroy()` 里增加 `ui_TetrisScreen_screen_destroy()`。其它 9 屏仍在横屏坐标系，退出恢复后不受影响。
- 状态栏只挂在 `ui_MainScreen` 上（`ui_StatusBar.c:56-57` 注释），不在 top layer，竖屏期间不会错位。

### 6.3 回退方案（若实机旋转异常）

若 `setRotation` 后 NV3007 显示偏移/花屏：不改业务逻辑，只在 `TetrisConfig.h` 调 `kPortraitRotation`（0↔2）与 `DisplayDriver` 构造参数中的 `col_offset` 组合；极端情况下退化为「横屏内居中绘制 120×240 竖棋盘 + 两侧放 HUD」，此时 `LvglPort::setOrientation` 变为 no-op，玩法与输入代码零改动。

## 7. 任务、时序与内存

- **不新建任务**。游戏重力由 `DisplayTask::run()` 每轮已有 `delta` 驱动：`TetrisGame::tick(delta_ms)` 累加到重力间隔就调 `Core::step()`；队列阻塞 10ms 保证 ~100Hz 轮转，重力最小 35ms，足够。
- 输入链路：旋钮/矩阵键 → `MainTask::sendDisplayAction()` → `DisplayMessage::ActionInput` → `DisplayTask` → `tetrisGame_handleInput()`。全程跨核只传 POD。
- **HID 屏蔽**：`MainTask::loop()` 的 `suppress_hid` 增加 `UI_SCREEN_GAME_SELECT || UI_SCREEN_TETRIS`。press 不进 `resolver_`/`KeyEventDispatcher`/`functionKeyJumpTarget`；release 仍照常派发，避免离开游戏后卡键（沿用 `rules.md §5` 先例）。
- **自动回主页**：`UI_SCREEN_GAME_SELECT`、`UI_SCREEN_TETRIS` 加入 `isSecondaryScreen()` 返回 true；游戏页额外加入 `checkPowerIdle()` 的常亮豁免（与 AI 二级页同理），防止长时间不看屏被降亮/关屏。
- 灯效覆盖在 `RGBLightControl::renderFrame()` 最前面判断 `game_palette_active_`：为 true 时按 11 色数组写 `setPixel` + `show()` 后 return，跳过所有普通模式与 ClickHighlight override。
- 内存预算：画布 4-bit ≈14.5KB + HUD 控件 <2KB + Core 状态 <1KB，远低于 128KB 池；但每次进/出只做一次 `lv_refr_now`，避免整屏高频重绘触发 `my_monitor_cb` 的 >100ms 告警与 IDLE0 饿死（历史教训见 `LvglPort.cpp:94-102`）。

## 8. 文件清单

### 8.1 新增

| 文件 | 内容 |
| --- | --- |
| `src/game/TetrisConfig.h` | 棋盘尺寸、cell 像素、重力表、计分表、11 色 LED/色块 RGB、`kPortraitRotation`（当前 =0） |
| `src/game/TetrisPalette.h` | C/C++ 共享颜色宏 + 竖屏 3×4 实体键网格 `TETRIS_KEY_GRID_IDS` |
| `src/game/TetrisCore.h/.cpp` | 纯游戏逻辑（§5.1/5.2），无 Arduino/LVGL 依赖 |
| `src/game/TetrisGame.h/.cpp` | 门面：C 链接 API（`tetrisGame_enter/exit/handleInput/tick/restart`）+ 画布/HUD 渲染 + LED 调色板控制 |
| `src/ui/ui_GameScreen.h/.c` | 一级游戏菜单页 |
| `src/ui/ui_GameSelectScreen.h/.c` | 二级游戏选择页 |
| `src/ui/ui_TetrisScreen.h/.c` | 竖屏游戏页控件与绘图原语（`ui_TetrisScreen_draw_board` 等） |
| `docs/14-tetris-game.md` | 本文档 |

### 8.2 修改（每个文件只动必要行）

| 文件 | 改动 |
| --- | --- |
| `src/ui/ui_types.h` | 枚举追加 `UI_SCREEN_GAMES`、`UI_SCREEN_GAME_SELECT`、`UI_SCREEN_TETRIS`（**追加在末尾**，不改现有数值，兼容消息载荷） |
| `src/ui/ui.h` / `src/ui/ui.c` | include + `ui_init()` 建 2 个新横屏页；`ui_destroy()` 销毁 3 个新页（游戏页懒创建时判空） |
| `src/ui/ui_AiScreen.c` | RIGHT 目标 `PC_STATUS` → `GAMES`（1 行） |
| `src/ui/ui_PcStatusScreen.c` | LEFT 目标 `AI` → `GAMES`（1 行） |
| `src/ui/ui_lang.c` | `ui_lang_set()` 追加 3 个 `*_apply_language` 调用 |
| `src/tasks/DisplayTask.cpp` | include 新门面；`navigateNow` 增加 3 个 case；`ActionInput` 增加 TETRIS/GAME_SELECT 路由；每帧 `TetrisGame::tick`；`isSecondaryScreen`/`checkPowerIdle` 豁免；`applySetting` 后同步游戏页语言 |
| `src/tasks/MainTask.cpp` | `suppress_hid` 条件增加两个游戏页 |
| `src/display/DisplayDriver.h/.cpp` | 新增 `setRotation(uint8_t)` |
| `src/display/LvglPort.h/.cpp` | 新增 `setOrientation(bool)` |
| `src/rgb/RGBLightControl.h/.cpp` | 新增 `setGamePalette(const RgbColor*, uint8_t)` / `clearGamePalette()`；`renderFrame()` 顶部优先判断 |

## 9. 实施顺序（每步可独立验收）

1. **纯逻辑**：`TetrisConfig.h` + `TetrisCore.*`，先用桌面小测试（不进固件编译）验证 7-bag / SRS / 消行 / 计分。
2. **横屏两页**：`ui_GameScreen.*`、`ui_GameSelectScreen.*` + 枚举/轮播 2 行改动 + 语言分发；旋钮走通 GAMES↔SELECT，确认不破坏原 9 屏轮播。
3. **竖屏链路**：`DisplayDriver::setRotation` + `LvglPort::setOrientation` + `ui_TetrisScreen.*` 空棋盘；实机确认 rotation 0/2 与偏移，确认进出不留花屏。
4. **游戏接通**：`TetrisGame.*` + `DisplayTask` 输入/tick + `MainTask` HID 屏蔽 + LED 调色板；跑通「选择→开始→移动/旋转/消行→结束→重开→退出」全流程。
5. **打磨**：幽灵块、Hold、连击分、暂停覆盖层、低内存水位复测、5s 自动回主页与省电豁免回归。

> 按项目规则，**不主动编译**；以上每步完成后由你明确要求才执行 `pio run -e esp32-s3-wroom-1-n16r8` 与烧录验证。

## 10. 风险与验证清单

| 风险 | 缓解 / 验证 |
| --- | --- |
| 竖屏 rotation 偏移导致花屏 | §6.3 回退方案；实机先验证 `setRotation(0)` 全屏纯色填充，再上 UI |
| LVGL 池耗尽（历史两次 OOM 导致 WDT） | 画布用 4-bit；`ui_TetrisScreen` 懒创建；关注 `DisplayTask` 每 30s 的 pool 日志；退出不重复创建对象 |
| 整屏重绘 >100ms 饿死 IDLE0 | 只在状态变化时重绘 + `vTaskDelay(1)` 让出保留；观察 `my_monitor_cb` 与 `lv_timer_handler cost` 日志 |
| 横竖屏切换瞬间花屏 | §6.2 严格同步时序；`lv_refr_now` 紧跟 `lv_scr_load` |
| 矩阵键在游戏中误发 HID | `MainTask.suppress_hid` 覆盖两页；release 仍派发避免卡键；实机用主机键盘监听验证 |
| 旋钮 ESC 被当成导航 | TETRIS 页不走 `lv_event_send` 通用路由（§5.3）；实机验证双击只暂停 |
| 新页面破坏 5s 自动回主页 | GAMES 是一级页应参与；SELECT/TETRIS 进 `isSecondaryScreen` 豁免；逐页计时验证 |
| 语言切换后新页仍英文 | `ui_lang_set` 追加调用；中英各切一次验证 |
| 退出游戏后灯效不恢复 | `clearGamePalette()` 在退出路径最前调用；观察普通模式动画恢复 |

## 11. 验收标准

1. 主页 RIGHT 连按可到 GAMES；GAMES 左/右分别回 AI / 进 PC_STATUS；原轮播其余 8 屏顺序不变。
2. GAMES 单击 → 选择页；旋转能在两个条目间切换且第二项 ENTER 不进游戏；单击第一项 → 屏幕切竖屏且无花屏。
3. 竖屏布局满足 §4.3：屏幕左侧 3×4 键位图（末行两键 + 旋钮）与实体键位置一一对应、右侧 NEXT/HOLD/分数可见、中部 10×20 棋盘；K1~K11 灯色与色块一致。
4. 玩法满足 §5.2 全部条目：能消行、计分、升级加速、Hold、幽灵块、暂停/继续、Game Over 重开。
5. 双击暂停 → 再双击回到 GAMES，屏幕恢复横屏、灯效恢复用户配置、HID 恢复正常。
6. 全程无 WDT / LVGL OOM 日志；`lv_mem_monitor` 水位无明显持续增长。

## 12. 实现落点对照（2026-11-19）

已落地文件（相对仓库根）：

**新增**
- `src/game/TetrisPalette.h` — C/C++ 共享颜色宏（比方案多拆一个文件，解决 C 层 UI 与 C++ 层逻辑的颜色复用）
- `src/game/TetrisConfig.h` — 集中配置（尺寸/重力/计分/竖屏 rotation/颜色）
- `src/game/TetrisCore.h/.cpp` — 纯逻辑（7-bag / SRS 踢墙 / 幽灵 / Hold / 连击 / 锁定延迟）
- `src/game/TetrisGame.h/.cpp` — C 链接门面（竖屏切换 / 输入分派 / 渲染 / LED 调色板）
- `src/ui/ui_GameScreen.h/.c` — 游戏菜单一级页
- `src/ui/ui_GameSelectScreen.h/.c` — 游戏选择二级页
- `src/ui/ui_TetrisScreen.h/.c` — 竖屏游戏页（4-bit 索引画布从 PSRAM 分配，直接写像素后一次性 invalidate）
- `src/ui/ui_font_TetrisFallback16.c` — 16px 后备字体，仅含原 CKJGT16 缺的 `转/软/逆/重/钮/退` 6 字

**修改**
- `src/ui/ui_types.h` — 追加 3 个屏幕 tag（追加在末尾，兼容既有数值）
- `src/ui/ui.h` / `ui.c` — 建/销毁新页（游戏页懒创建）
- `src/ui/ui_lang.c` — 3 页语言分发
- `src/ui/ui_AiScreen.c` — RIGHT→GAMES
- `src/ui/ui_PcStatusScreen.c` — LEFT→GAMES
- `src/tasks/DisplayTask.cpp` — include、navigateNow 3 case、ActionInput 的 TETRIS 截胡、每帧 tick、isSecondaryScreen / checkPowerIdle 豁免
- `src/tasks/MainTask.cpp` — `suppress_hid` 增加两游戏页
- `src/display/DisplayDriver.h/.cpp` — `setRotation()`
- `src/display/LvglPort.h/.cpp` — `setOrientation(bool)` + `portrait_`
- `src/rgb/RGBLightControl.h/.cpp` — `setGamePalette()` / `clearGamePalette()`，renderFrame 顶部最高优先级覆盖

**实现期修正的两处逻辑问题**（静态审查发现并修复）
1. `nextAt()` 曾返回当前正在下落的块：改为 `takeNext()` 消费队首并前移，Hold 交换时把预取块放回队首避免丢失预览。
2. K8 键在 PLAYING 状态原实现为暂停，与色块"退出"标注不符：改为直接退出。

### 12.1 按键布局优化（2026-11-19 追加）

按用户确认的竖屏实体键位重做布局：

- `kPortraitRotation` 由 `0` 改为 `2`（竖屏方向 180° 翻转）。
- 屏幕顶部 2×4 色块条改为**屏幕左侧 3×4 键位图**，槽位顺序与实体键完全一致（末行 K11 / K7 / 旋钮）；HUD 移到屏幕右侧，画布位置不变。
- 11 个矩阵键全部启用：LED / 色块调色板由 8 色扩为 11 色。功能映射 2026-11-19 调整为 K1=重开 / K2=右移 / K4=暂停 / K5=旋转 / K6=硬降 / K8=退出 / K9=左移；K3 / K7 / K10 / K11 暂无功能。
- `TETRIS_KEY_GRID_IDS` / `s_grid_action[]` 分别集中定义实体键顺序与槽位功能索引，`s_action_text[]` 定义每个功能索引的显示文字，改键位功能只需改这三处（颜色/LED 下标按物理键固定，无需改动）。

### 12.2 竖屏方向再次翻转（2026-11-19 追加）

- 按用户要求，`kPortraitRotation` 由 `2` 改回 `0`（与 `r=2` 相差 180°）。其余布局 / 按键 / 玩法代码零改动；键位提示图仍按屏幕坐标绘制。

**仍待实机验证**：当前 `kPortraitRotation=0` 的实际正立方向与列偏移、画布 4-bit 渲染、HID 屏蔽、内存水位（见 §10）。

### 12.3 游戏页 UI 清晰度优化（2026-11-20 追加）

仅改 `src/ui/ui_TetrisScreen.c` 的布局与样式，玩法 / 棋盘尺寸 / 对外接口零改动：

- **右上 HUD**：SCORE / LV / LN 由"裸数值 + 缩写"改为灰色小标题（`create_hud_caption`）+ 彩色数值，语义统一；数值字号 14→16。
- **预览区重排**：HOLD + NEXT×3 从右侧窄列移到键位图与棋盘之间的横带（y≈79..110），各加小标题、统一 18×18 并加描边圆角，一眼分清暂存与预览。右侧 58px 宽度无法容纳带标签的 5 组信息，故做此空间再分配。
- **空键槽降权**：K3/K7/K10/K11（4 个无功能键）由高亮实色块改为暗底 `#1A2230` + 物理键 LED 色 2px 描边 + 灰色文字，有效键更突出，同时保留颜色对应关系（`is_inactive_action`）。
- **状态覆盖层卡片化**：PAUSED / GAME OVER 由 80% 薄遮罩改为 90% 高对比底 + 青色描边 + 8px 圆角 + 内边距，尺寸 136×74，中文提示折 3 行不溢出。
- **底部提示提亮**：颜色 `#7C8798` → `#9FB0C4`，中英文案精简。
- 棋盘网格线**未做**：需改 4-bit 逐像素绘制、性能敏感，而幽灵块已有暗色区分，收益低于成本。
