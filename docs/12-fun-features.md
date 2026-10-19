# 阶段 12 — 让 EKeys 更好玩的功能提案

> 状态：提案（未排期）· 2026-10-19
> 关联：[FEATURE_DOC](../FEATURE_DOC.md) · [ARCHITECTURE](../ARCHITECTURE.md) · [05-ui-screens](./05-ui-screens.md) · [11-home-assistant-native-api](./11-home-assistant-native-api.md)
> 说明：本文只做方案与优先级讨论，不含实现；按项目规则不主动编译。

---

## 1. 一句话结论

EKeys 已经具备"迷你游戏机 / 桌面乐器 / 桌面摆件"的全部硬件要素：13 屏、11 键、11 颗独立 RGB、喇叭、麦克风、Wi-Fi/BLE、桌面 App 与 Web 配置端。  
但当前交互闭环仍是 **"按键 → HID + 状态展示"**：设备很能干，却很少"主动陪用户玩"。

建议先补 3 个玩法基础设施（前台输入路由、音效服务、玩法存储），再按四步推进：

1. **即时反馈**：按键音效、打字律动灯、音乐打字机；
2. **小游戏**：Simon Says、打地鼠、Flappy、反应力、节奏敲击；
3. **陪伴与联动**：桌面宠物、统计/成就、场景剧场、通知中心；
4. **深度玩法**：MIDI 乐器、Stream Deck、变声器、ModC 硬件模块。

---

## 2. 现状盘点：已经"好玩"的部分

| 能力 | 现状 | 位置 |
| --- | --- | --- |
| 输入 | 11 键矩阵 + EC11 旋钮 + I2C ModA/ModB | [`src/input/`](../src/input) |
| 键映射 | 8 Profile × 2 FUN 层，支持 function / text / normal / macro | [`KeyResolver.cpp`](../src/keymap/KeyResolver.cpp) |
| 音效板 | 11 键各自绑定 mp3/wav，App/Web 上传与试播 | [`AudioPad.h`](../src/audio/AudioPad.h) |
| 灯光 | 11 种模式，含 3 种拾音律动（频谱/矩阵/渐变） | [`RGBLightControl.h`](../src/rgb/RGBLightControl.h) |
| 显示 | 13 屏（含二级页）、翻页时钟、状态条、主题系统在规划中 | [`ui.h`](../src/ui/ui.h)、[`docs/theme-spec/`](./theme-spec) |
| 语音 | 腾讯云一句话识别，按键触发，文本回传 App / HID 兜底 | [`VoiceRecognizer.cpp`](../src/voice/VoiceRecognizer.cpp) |
| 桌面联动 | 音乐状态、PC 状态、Profile 图标、OTA | [`src/protocol/commands/`](../src/protocol/commands) |
| 智能家居 | ESPHome Native API，11 键作为 binary_sensor | [`docs/11-home-assistant-native-api.md`](./11-home-assistant-native-api.md) |
| 硬件余量 | 16MB Flash（App 3MB×2 + SPIFFS 3.875MB）/ 8MB PSRAM / 电池 ADC / 扩展 I2C + 5V 升压 | [`partitions-16MB.csv`](../partitions-16MB.csv)、[`PINOUT.md`](../PINOUT.md) |

结论：**不缺执行器，缺"玩法循环"**。屏幕、灯、喇叭、麦克风都已在，但没有一个"持续运行、对输入即时反馈、有输赢/成长"的循环。

---

## 3. 差距：四个玩法基础设施

### 3.1 前台输入捕获

目前"HID 屏蔽"是逐屏硬编码的：

- HA 屏截胡矩阵键 → HA binary_sensor；
- 设置二级页 / 音效二级页屏蔽矩阵键 HID；
- 其它屏矩阵键只走 HID，UI 侧忽略。

代码见 [`MainTask.cpp`](../src/tasks/MainTask.cpp) 的 `suppress_hid` / `ha_routing` 分支和 [`DisplayTask.cpp`](../src/tasks/DisplayTask.cpp) 的 `ActionInput` 路由。

**游戏、宠物、乐器都需要"当前前台接管按键"**，应把这套规则收口成 `InputRouter`，而不是继续加 `if (screen == ...)`。

### 3.2 声音合成 / 混音

[`Speaker`](../src/audio/Speaker.h) 封装的是 ESP32-audioI2S 的单实例、单流播放：

- 同一时刻只能播一个文件；
- 启动要打开 SPIFFS、探测解码器，延迟可能 50~100ms；
- AudioPad 与音乐播放、ASR 录音之间已经有一套串行/互斥规则。

"按键音效"可以接受一次性短音；但"音乐打字机 / 节奏游戏 / MIDI"需要 **低延迟 PCM 合成**，这是隐藏成本最高的一块。

### 3.3 连续动画 / 游戏循环

[`DisplayTask`](../src/tasks/DisplayTask.cpp) 是"消息驱动 + LVGL tick"：

- 收到消息才刷新对应屏；
- 没有统一的 `tick(dt)` 给某个"前台玩法"跑逻辑；
- 5s 自动回主页对游戏屏不适用。

需要一个 `GameHost` / `ForegroundApp` 抽象：`enter / onKey / tick / render / exit`。

### 3.4 长期记忆

目前只有配置、键映射、音效绑定、Profile 图标。

没有：按键次数、每日统计、游戏最高分、宠物状态、成就解锁、场景定义。  
这类数据需要独立存储（建议 SPIFFS 固定结构 + CRC，定期刷盘；**不要每次按键写 NVS**）。

---

## 4. 功能候选总览

成本口径：S ≤ 3 人日，M ≈ 1~2 周，L ≥ 3 周（含联调）。好玩度为主观评分。

| # | 功能 | 一句话玩法 | 主要复用 | 主要新增 | 成本 | 好玩度 |
| --- | --- | --- | --- | --- | --- | --- |
| F1 | 按键音效包 | 每次按键/松开播放机械、打字机、气泡、科幻音效 | 按键边沿、Speaker、AudioPad | SoundService、音效资源、设置项 | S~M | ★★★★★ |
| F2 | 音乐打字机 | 11 键变成五声音阶，旋钮换八度，RGB 随音高亮 | 按键边沿、RGB、Speaker | PCM 合成/低延迟播放、音阶映射 | M | ★★★★★ |
| F3 | 打字律动灯 | 灯随敲击速度/力度波动，越快越"燃" | RGBLightControl、按键边沿 | 新 RGB 模式 + 击键节拍状态 | S | ★★★★ |
| F4 | LED 像素画布 | Web/App 画 11 颗灯，保存成动画预设 | RGBDriver、Web/App | 图案存储、编辑器、动画播放 | S~M | ★★★ |
| F5 | 小游戏合集 | Simon、打地鼠、Flappy、反应力、节奏敲击 | 屏幕、11 键、RGB、喇叭 | GameHost、游戏屏、分数存储 | M | ★★★★★ |
| F6 | 桌面宠物 | 屏幕角落养一只像素宠物，随敲击/时间/声音变化 | 主屏、时间、按键统计 | 状态机、精灵动画、养成存储 | M~L | ★★★★★ |
| F7 | 打字统计/热力图 | 每日按键数、WPM、最常用键；RGB 热力图 | 按键边沿、RGB | 统计存储、App/Web 面板 | M | ★★★★ |
| F8 | 成就系统 | "1000 次敲击""Simon 10 连""夜猫子"等徽章 | 统计、屏幕 toast | 成就定义/解锁/持久化 | S~M | ★★★★ |
| F9 | 番茄钟/专注模式 | 25 分钟倒计时 + 灯环 + 休息提醒 + 连续天数 | 时间 tick、RGB、屏幕 | 计时器、专注统计 | S~M | ★★★ |
| F10 | 场景剧场 | 一键"开播/下班/游戏"：灯光、音效、PC 动作按时间线执行 | Profile、App、RGB、Speaker | 场景 JSON、App 动作执行器 | M | ★★★★★ |
| F11 | 通知中心 | Discord/Teams/CI 通知推到屏幕 + 灯，按键处理 | 屏幕、RGB、App | 通知协议、横幅 UI、勿扰 | M | ★★★★ |
| F12 | 屏幕镜像/手机遥控 | App 看设备实时画面，手机当遥控器 | LVGL flush、TCP | 帧编码/传输、远程输入 | M | ★★★ |
| F13 | MIDI 乐器模式 | 11 键当鼓垫/琴键，旋钮当 CC，接 DAW | 按键、RGB、USB | USB MIDI 复合描述符、音阶/力度 | L | ★★★★★ |
| F14 | Stream Deck 模式 | 每键显示图标/状态，控制 OBS/Spotify/Twitch | 48×48 图标、Profile、App | 每键图标 UI、App 集成 | L | ★★★★ |
| F15 | 变声器/录音回放 | 按住说话，变机器人/花栗鼠/慢放后回放 | Mic、Speaker、PSRAM | 录音缓冲、DSP 变声 | L | ★★★★ |
| F16 | 节日/彩蛋 | 节日灯效、生日祝福、Konami 密码 | NTP、RGB、屏幕 | 彩蛋规则表 | S | ★★★ |
| F17 | ModC 硬件模块 | 摇杆/震动/NFC/环境传感器，即插即用 | 扩展 I2C、5V 升压 | 新模块协议、输入类型 | L | ★★★★ |

---

## 5. 推荐路线

### 5.1 第一批：一周内"有感觉"

**F1 按键音效 + F3 打字律动灯 + F5 的 Simon Says + F16 彩蛋**

- 不需要新屏幕导航，先在主屏/现有屏上就能体验；
- 立刻改变"这台键盘的手感"，演示效果最好；
- 同时验证 SoundService 的延迟和 AudioPad 的串行约束。

### 5.2 第二批：一个月"有得玩"

**F5 游戏包 + F6 桌面宠物 + F7 统计/热力图 + F8 成就 + F4 LED 画布**

- 先落 GameHost / 玩法屏 / 统计存储三块基础设施；
- App 增加 Game / Stats 面板，Web 增加 LED 画布；
- 宠物与统计/成就互相喂数据，形成"每天打开都有变化"。

### 5.3 第三批：季度"有身份"

**F10 场景剧场 + F11 通知中心 + F13 MIDI + F14 Stream Deck + F15 变声器 + F17 ModC**

- 面向不同人群：直播/办公、音乐、智能家居、硬件玩家；
- 需要协议、App、UI 的较大改动，适合作为单独版本主题。

---

## 6. 重点方案

### 6.1 基础设施 A：`InputRouter`（前台输入路由）

**现状**

`MainTask::loop()` 里：

```cpp
const bool ha_routing = screen == UI_SCREEN_HA || screen == UI_SCREEN_HA_SECONDARY;
const bool suppress_hid = ha_routing ||
                          screen == UI_SCREEN_SETTING_SECONDARY ||
                          screen == UI_SCREEN_AUDIO_SECONDARY;
```

新增玩法屏如果要接按键，只能继续加条件，且 release 路径、FUN 预扫描、ActionInput 投递都要逐个照顾。

**建议**

新增 `src/input/InputRouter.{h,cpp}`，把"当前屏幕 → 谁消费按键"变成一张表（详细设计见 [13-input-router-design.md](./13-input-router-design.md)）：

```cpp
enum class InputRoute : uint8_t {
    Hid,        // 正常发 HID，同时投递 ActionInput 给 UI
    UiOnly,     // 不发 HID，只给 UI（设置二级页 / 音效二级页）
    Ha,         // HA binary_sensor
    Foreground, // 前台玩法（GameHost / Pet / Instrument）接管
};

InputRoute routeFor(ui_screen_tag_t screen);
```

- `MainTask` 只问 `routeFor()`，不再散落 `if`；
- `DisplayTask` 在 `Foreground` 路由下把 101~111 交给 `GameHost`；
- **release 永远走 HID release**（沿用现有"避免卡键"策略），只有 press 被接管；
- 为 `UI_SCREEN_GAME` / `UI_SCREEN_PET` 预留标签。

**验收**

- 普通打字、FUN 组合、宏、文本注入零回归；
- 进入玩法屏后按键不发 HID，退出后无卡键；
- HA 屏行为不变（docs/11 的既有契约）。

---

### 6.2 基础设施 B：`SoundService`（音效服务）

**为什么不能直接调 Speaker**

`DisplayTask` 在 Core 0，`Speaker` / `AudioPad` 的启停必须在 `MainTask` 串行执行（AudioPad 头文件已记录过 INT WDT 的教训）。  
所以玩法层只能"请求音效"，不能直接 `PlayLocalAudio`。

**建议**

```cpp
enum class SfxCue : uint8_t {
    KeyDown, KeyUp, Ok, Fail, Tick, Coin, Jump, GameOver, ...
};

class SoundService {
public:
    static SoundService &instance();
    void begin();                  // 加载音效表
    bool request(SfxCue cue, uint8_t volume = 100); // Core 0 可调用，仅置请求
    void service();                // MainTask tick 消费
    void setEnabled(bool on);
};
```

- 阶段 1：**短 WAV 一次性播放**（16kHz / 16bit / mono，50~120ms），文件放 SPIFFS `/sfx/`；
- 请求队列按优先级：ASR 录音期间静音（不播音效） > UI 音效 > AudioPad > 音乐；
- 与音乐同时播放时，阶段 1 允许"打断音乐"或"跳过音效"，阶段 2 再考虑混音；
- 阶段 2（音乐打字机/节奏游戏）：预加载 PCM 到 PSRAM，直接用 `i2s_write` 小 DMA 输出；或利用 ESP32-audioI2S 的 `audio_process_i2s` 钩子做混音；
- 资源估算：80ms 的 16kHz mono 16bit ≈ 2.5KB，30 个音效 ≈ 75KB，SPIFFS 完全放得下。

**关键指标**

- 按键 → 出声延迟目标 **< 30ms**（先用 WAV 实测；不达标再上 PCM）；
- 不能影响 ASR 录音与频谱（Mic 与 Speaker 使用独立 I2S 外设，见 `PinMap.h` 注释）；
- 关闭音效时零额外 CPU。

---

### 6.3 基础设施 C：`GameHost` + 玩法屏

**屏幕**

建议新增 `UI_SCREEN_GAME`，但**不要改 SquareLine 工程**：

- 在 `src/ui/game/ui_GameScreen.c` 里用 LVGL 代码直接创建（`lv_obj_create(NULL)` + canvas/label）；
- 在 `ui_KeyMapped.h` 的 `ui_screen_tag_t` 追加枚举（该文件已有手写扩展，非纯生成文件）；
- `DisplayTask::navigateNow()` 增加映射；
- 退出时销毁或复用一个常驻屏，避免反复建对象导致 128KB LVGL 池碎片。

**宿主**

```cpp
class GameHost {
public:
    void enter(GameId id);
    void onKey(uint8_t key, bool pressed); // 来自 DisplayTask 的 101~111
    void tick(uint32_t dt_ms);
    void render();
    void exit();
    bool active() const;
};
```

- `GameHost` 跑在 `DisplayTask`（Core 0），直接调 `RGBLightControl` / `SoundService::request`；
- 分数写 `/game_scores.ini`，由 MainTask 持有存储（或通过消息请求）；
- 5s 自动回主页在游戏屏禁用（沿用"二级页不自动回"的规则）；
- 退出游戏时由 `MainTask` 在路由切回 HID 的边沿执行 `keyboard_->releaseAll()`，避免游戏期间按下的键卡住。

**风险**

- 5ms 矩阵扫描 + ~30ms 渲染：输入延迟约 10~35ms，动作游戏可接受，音游偏紧；
- 音游建议 **PC 端播放歌曲**，设备只做音符显示与判定，避免设备音频延迟；
- LVGL 对象生命周期与内存水位要重点测（参考 `lv_conf.h` 里 64KB→128KB 的 OOM 历史）。

---

### 6.4 游戏包（F5）

| 游戏 | 玩法 | 输入 | 反馈 | 成本 |
| --- | --- | --- | --- | --- |
| Simon Says | 灯/音序列越来越长，按顺序复现 | 11 键 | 11 RGB + 音阶 | 2~3 人日 |
| 打地鼠 | 3×4 网格随机亮，按对应键 | 11 键 | 屏幕 + RGB + 音效 | 2~3 人日 |
| Flappy | 任意键扇翅膀，穿管道 | 任意键/旋钮 | 屏幕 + 音效 | 1~2 人日 |
| 反应力 | 屏幕变色后最快按键，毫秒计时 | 任意键 | 屏幕 + RGB | 1 人日 |
| 节奏敲击 | 4 轨音符下落，判定 Perfect/Good/Miss | 4~8 键 | 屏幕 + 音效 | 4~6 人日 |

Simon 是最推荐的"第一个游戏"：不需要复杂美术、天然用满 11 键和 11 灯、规则 10 秒讲清、演示效果极好。

---

### 6.5 键感增强包（F1 / F2 / F3）

**F1 按键音效**

- 音效包：机械青轴、打字机、气泡、科幻、静音；
- 按下/松开可不同音；每次随机 ±5% 音高，避免机械重复；
- 设置项：`key_sound_enable`、`key_sound_pack`、`key_sound_volume`；
- 触发点：`KeyEventDispatcher::onKeyEdge()` 之后（HID 已经发出，不影响打字）。

**F2 音乐打字机**

- 11 键映射到五声音阶（C D E G A），旋钮换八度；
- RGB 按音高给色，按下即亮；
- 按住可延音（需要合成器），否则一次性音符；
- 可加"录音循环"：录 8 拍，循环播放，变成一人乐队。

**F3 打字律动灯**

- 新增 `RGB_TYPING_MODE`；
- 记录最近 2s 击键间隔，映射到亮度/波动速度/色相偏移；
- 快速敲击 → 彩虹追逐；停手 → 呼吸回落；
- 不需要新协议，只在 `RGBLightControl` 增加状态。

**设置项改动面**

新增字段会同时影响：

- [`DeviceSettings.h`](../src/config/DeviceSettings.h)；
- `config.ini` 读写；
- [`parseConfigSetCommand.cpp`](../src/config/parseConfigSetCommand.cpp)（0x08）；
- [`ui_settings_types.h`](../src/ui/ui_settings_types.h) 与设置屏（可选）；
- App 的 `protocol.rs` / `state/mod.rs` / `panel_settings.rs`；
- Web 的 `protocol/settings.ts` / `SettingsPanel.tsx`。

建议默认全关，首次开启时在 App 里给一段试听。

---

### 6.6 桌面宠物（F6）

**定位**

不是"再来一个功能"，而是主屏的"人格层"：用户不操作时，它在那里；用户敲键盘时，它有反应。

**状态机**

- 属性：心情、精力、亲密度、年龄；
- 输入：按键次数、上次活动时间、当前 Profile、音量/频谱、番茄钟状态；
- 行为：发呆、打瞌睡、被敲击吓一跳、饿了、开心、睡觉；
- 输出：像素动画 + 偶尔音效 + RGB 情绪色。

**交互**

- 旋钮在宠物屏：摸摸头 / 喂食；
- 音效板播放 = 喂食；
- 语音识别到"你好" = 打招呼；
- 连续 7 天使用 = 进化/换装饰。

**实现**

- 32×32 或 48×48 精灵，4~8 帧，用 LVGL image/canvas 绘制；帧数据放 PSRAM 或 flash；
- 状态每 60s + 每次变化写 `/pet.ini`（带 CRC），掉电最多丢 1 分钟；
- 主屏侧栏常驻，不抢按键；独立"宠物屏"才接管旋钮；
- 与统计/成就共享事件（按键、番茄钟完成）。

**风险**

- 美术工作量大于代码；建议先做"像素豆子"占位，玩法验证后再换皮；
- 不要让它阻碍正常打字；所有互动都必须可选。

---

### 6.7 统计 / 热力图 / 成就（F7 / F8）

**统计**

- 每键按下次数、释放次数、每日总量、会话 WPM、最常用键、小时分布；
- 存储 `/stats.bin`：固定结构 + magic + CRC，5 分钟或空闲时刷盘；
- 不上报云端，默认只在本机与 App 展示。

**热力图**

- 新增 `RGB_HEAT_MODE`：按按键计数映射颜色（冷→暖），支持"今日/累计"切换；
- 键映射屏可显示每键次数（复用现有 11 格标签）；
- App 提供柱状图与"重置统计"。

**成就**

| ID | 条件 | 名称示例 |
| --- | --- | --- |
| A01 | 第一次使用宏 | 初试锋芒 |
| A02 | 累计 1,000 次敲击 | 千锤百炼 |
| A03 | 累计 100,000 次 | 键盘杀手 |
| A04 | 单次会话 11 键都按过 | 全键制霸 |
| A05 | Simon 10 连 | 记忆大师 |
| A06 | 连续 7 天使用 | 形影不离 |
| A07 | 凌晨 2~5 点使用 | 夜猫子 |
| A08 | 第一次语音识别成功 | 声控时代 |

- 解锁时屏幕 toast + RGB 闪光 + 音效；
- 持久化一个 bitmask；
- App 增加成就墙。

---

### 6.8 场景剧场 + 通知中心（F10 / F11）

**场景剧场**

一句话：**一个键，把桌面切换成某种状态。**

```json
{
  "id": "stream",
  "name": "开播",
  "steps": [
    { "delay": 0,   "device": { "rgb": "rainbow", "screen": "main" } },
    { "delay": 300, "device": { "sfx": "start.wav" } },
    { "delay": 800, "pc": { "action": "obs.scene", "args": { "name": "Starting" } } },
    { "delay": 1200, "pc": { "action": "app.launch", "args": { "path": "C:/..." } } }
  ]
}
```

- 设备执行本地 cue（灯、音、屏），App 执行 PC 动作；
- 每一步 ACK，失败则停止并提示；
- 场景存 SPIFFS `/scenes/*.json`，App/Web 编辑；
- 安全：PC 动作走 App 的白名单（启动应用、OBS、音量、播放控制），**默认不允许任意 shell**。

**通知中心**

- App 监听/接收通知（Windows Toast、Discord、Teams、GitHub Actions 等），转成 `NOTIFY` 帧；
- 设备显示 3s 横幅 + 对应颜色灯效；
- 按键 7 = 忽略，按键 11 = 稍后提醒；旋钮 = 查看列表；
- 勿扰模式（时间/专注钟联动）。

**协议**

建议保留 `0x18~0x1F`：

| ID | 用途 |
| --- | --- |
| 0x18 | Game（开始/退出/分数） |
| 0x19 | Stats（读取/重置） |
| 0x1A | Scene（运行/停止/列表） |
| 0x1B | Notify（推送/清除） |
| 0x1C | KeyIcon（每键图标/状态，Stream Deck 预留） |
| 0x1D | Sfx/asset（音效资源管理） |
| 0x1E | Pet（状态/互动） |
| 0x1F | MIDI（配置/映射） |

---

### 6.9 屏幕镜像 + 手机遥控（F12）

- 428×142×2B ≈ 121KB/帧；USB CDC 115200 只有 ~11KB/s，**不适合原样传帧**；
- 可行路径：
  1. App/Web 端根据现有状态重建 UI（已有 `state`），只传增量；
  2. 设备端做 RLE/调色板压缩 + 2~5fps，走 Wi-Fi TCP；
  3. 只传"屏幕截图"用于演示，不追求实时。
- 反向控制：App → 设备注入 `LV_KEY_*` / 矩阵键事件，做"手机遥控器"；
- 价值：演示、录屏、远程帮用户配置；不是核心玩法，建议作为工具功能排后。

---

### 6.10 MIDI 乐器 / Stream Deck（F13 / F14）

**MIDI**

- 11 键 → 鼓垫/琴键；旋钮 → CC（音量/滤波器/音色）；
- USB MIDI 需要把现有 CDC + HID 复合描述符扩展为 CDC + HID + MIDI；
- BLE MIDI 与现有 BLE HID 共存需要统一 GATT server，建议先做 USB；
- 力度：按键是二值的，可用"按下时长"做简易力度，或固定力度 + 表情旋钮；
- 延迟：USB MIDI 通常 1~3ms，远好于音频文件播放；
- 成本：L（USB 描述符、驱动验证、映射 UI、App 配置）。

**Stream Deck**

- 每键显示 40×40 图标 + 状态；4 列 × 3 行，单格约 107×47，48×48 图标略高，需缩到 40×40 或改 3×3；
- 图标上传可复用 0x11 的思路，新增每键图标路径；
- App 集成 OBS WebSocket / Spotify / Twitch；
- 成本：L（UI 重排、图标管理、App 集成、页面切换）。

---

### 6.11 变声器 / 节日彩蛋 / ModC（F15 / F16 / F17）

- **变声器**：Mic → PSRAM 环形缓冲 → 变调/变速/机器人 → Speaker；  
  风险是喇叭与麦克风距离近导致啸叫，建议"录音后回放"而不是实时扩音；成本 L。
- **节日彩蛋**：NTP 日期触发（元旦倒计时、生日、万圣节、圣诞）；  
  Konami 码（↑↑↓↓←→←→）触发隐藏灯效/小游戏；成本 S。
- **ModC 模块**：扩展 I2C（IO47/48/45）+ 5V 升压已具备；  
  可做摇杆、6 键小键盘、震动马达、NFC、IMU、温湿度/空气质量；  
  需要"模块自动识别 + 新输入 ID + 游戏/宠物联动"，成本 L（硬件 + 固件）。

---

## 7. 三端改动清单

| 功能 | 固件 | App（Rust/egui） | Web（React） |
| --- | --- | --- | --- |
| F1/F3 键感 | `DeviceSettings`、`parseConfigSetCommand`、`SoundService`、`RGBLightControl` | `panel_settings`、`protocol.rs` | `settings.ts`、`SettingsPanel` |
| F5 游戏 | `GameHost`、`ui_GameScreen`、`InputRouter`、分数存储 | `panel_game`、`protocol.rs` | 可选 |
| F6 宠物 | `PetState`、主屏侧栏、存储 | `panel_pet` | 可选 |
| F7/F8 统计/成就 | `StatsStore`、`Achievement`、RGB 热力图 | `panel_stats`、`state` | 图表页 |
| F10 场景 | 场景解析/本地 cue、协议 0x1A | 场景编辑器、动作执行器 | 场景编辑 |
| F11 通知 | 横幅 UI、0x1B | 通知桥接 | — |
| F13 MIDI | USB 描述符、MIDI 映射 | 映射配置 | — |
| F14 Stream Deck | 每键图标 UI、0x1C | OBS/Spotify 集成 | 图标上传 |
| F4 LED 画布 | 图案存储/动画 | 可选 | 画布编辑器 |
| F12 镜像 | 帧编码/远程输入 | 预览窗口 | 浏览器预览 |

> 三端协议目前是"手写三份"（`SerialProtocol.h` / `protocol.rs` / `commands.ts`），新增 0x18+ 时建议先更新 [desktop-app-protocol.md](./desktop-app-protocol.md) 作为单一事实源，再改代码。

---

## 8. 资源、风险与取舍

| 风险 | 说明 | 对策 |
| --- | --- | --- |
| 音频延迟/单流 | 文件播放启动慢，无法同时出声 | 先短 WAV 验证；音游用 PC 播放；需要时再做 PCM 混音 |
| LVGL 内存 | 13 屏常驻对象已迫使池从 64KB 扩到 128KB（见 `lv_conf.h` 注释） | 玩法屏懒创建/复用；精灵放 PSRAM；监控水位 |
| Flash/分区 | App 槽 3MB，游戏/资源可能挤压 | 资源放 SPIFFS；单游戏资源 < 200KB；必要时裁字体 |
| NVS/SPIFFS 寿命 | 每键写统计会写坏 flash | 统计 5 分钟刷一次；NVS 只存配置 |
| BLE/Wi-Fi 互斥 | BLE 模式会停 Wi-Fi（内存） | MIDI 先 USB；BLE MIDI 单独模式 |
| 输入语义冲突 | 矩阵键默认是 HID 专用 | 只有玩法屏接管；退出必 releaseAll；默认关闭 |
| SquareLine 重导出 | 新屏/枚举可能被覆盖 | 新屏用代码创建；生成文件改动加注释；枚举扩展记录 |
| 范围蔓延 | 每个方向都能做大 | 按批次做，每批有独立开关与验收 |

**明确不建议做**：

- 2.4G 无线键盘（项目已决定不做）；
- BLE A2DP 音频接收（ESP32-S3 BLE-only 栈不现实）；
- 在设备上跑大模型/在线视频（资源与功耗不匹配）；
- 触摸交互（硬件无触屏，现有旋钮 + 矩阵键已够）。

---

## 9. 建议的下一步

1. **3 天 spike（二选一）**
   - **Simon Says**：验证 InputRouter + 玩法屏 + SoundService + RGB，一次打通四个基础设施；
   - **按键音效**：验证音频延迟与 AudioPad 串行约束，最快看到"手感"变化。
2. **1 周内**：统计计数器 + App 状态面板（为宠物/成就打底）。
3. **2~3 周**：游戏包（先 Simon + 打地鼠 + 反应力）。
4. **1 个月**：宠物 + 成就 + LED 画布。
5. **季度**：场景剧场 / 通知中心 / MIDI / Stream Deck 中选一个方向做深。

---

## 10. 验收清单（适用于任何一批）

- [ ] 普通打字、FUN 组合、8 Profile、宏、文本注入零回归；
- [ ] 进入/退出玩法屏无卡键，`releaseAll()` 生效；
- [ ] 连续运行 1 小时无 WDT、无 LVGL OOM、PSRAM/内部堆水位稳定；
- [ ] 音效/游戏不打断 ASR 录音与频谱（或按优先级明确让路）；
- [ ] 所有新功能默认关闭，可在 App/设置屏关闭；
- [ ] 统计/分数/宠物状态重启后保留，掉电最多丢一个刷盘周期；
- [ ] OTA 升级与回滚不受影响；
- [ ] App/Web 对旧固件保持向后兼容（未知命令忽略）。

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-19 | 初稿：现状盘点、17 个候选、三批路线、基础设施与风险 |
