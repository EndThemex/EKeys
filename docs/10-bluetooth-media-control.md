# 阶段 10 — 蓝牙媒体控制（BLE HID Consumer + App 状态推送）

> 状态：方案文档（待评审 / 实施前）
> 关联章节：[`FEATURE_DOC.md §13 / §14`](../FEATURE_DOC.md) / [docs/desktop-app-protocol.md §8.4](./desktop-app-protocol.md)
> 关联代码：[`src/output/BLEKeyboardImpl.cpp`](../src/output/BLEKeyboardImpl.cpp) / [`src/protocol/commands/cmd_music.cpp`](../src/protocol/commands/cmd_music.cpp) / [`src/ui/ui_MusicScreenSecondary.c`](../src/ui/ui_MusicScreenSecondary.c)

## 1. 背景与目标

### 1.1 现状盘点

| 能力                 | 现状                                                                                       |
| -------------------- | ------------------------------------------------------------------------------------------ |
| BLE HID 键盘         | `BLEKeyboardImpl` 基于 `t-vk/ESP32-BLE-Keyboard`（Keyboard + Mouse + Consumer 已包含在库内） |
| 媒体控制键下发       | 库支持 `KEY_MEDIA_PLAY_PAUSE` / `NEXT_TRACK` / `PREVIOUS_TRACK` / `VOLUME_UP/DOWN` / `MUTE`，但 `BLEKeyboardImpl::press()` 仅透传 HID usage code（0x01\~0x77），**当前未暴露媒体键通道** |
| 播放状态显示         | `MusicPlayerInfo` + `CMD_MUSIC_STATUS(0x0e)` 已就绪，但**仅 USB + 桌面 App（TCP/CDC）链路** |
| 音乐屏 UI            | `ui_MusicScreenSecondary` 已有 PREV/TOGGLE/NEXT 三按钮 + `ui_MusicScreenSecondary_request_control()`，但**没有连到 HID 输出** |
| `BLEKeyboardImpl`    | F11 修复链路负责 usage ↔ 库域映射（`usageToLibKeycode()`），媒体键码 0xE8\~0xEC 在 0x01\~0x77 之外 → **被 usageToLibKeycode() 静默拒绝** |

### 1.2 目标

实现"蓝牙模式连接手机后，能从设备端获取手机的播放状态（标题 / 艺人 / 进度 / 播放态）并控制播放（上一首 / 播放暂停 / 下一首 / 音量）"，并把屏幕和协议层统一收口。

边界澄清（避免范围蔓延）：

- 仅做"控制 + 状态显示"。**不实现 A2DP sink / AVRCP target**（ESP32-S3 端做蓝牙音频接收在 BLE-only 栈上不现实，且本项目已有本地 Speaker + ESP32-audioI2S）。
- 仅做 HID Consumer 报告的发送；**不解析 AVRCP metadata PDU**。
- 状态获取走手机端 App（iOS / Android 各自实现），App 端通过 USB CDC 或 WiFi/TCP 把状态封进现有 `CMD_MUSIC_STATUS(0x0e)` 推给主控，主控再分发到 UI。

### 1.3 兼容性预期

| 主机     | BLE HID Consumer（控制） | App 推状态（展示）         |
| -------- | ------------------------ | -------------------------- |
| Android  | 稳定                     | 需 App（无自带 App）       |
| Windows  | 稳定                     | 需桌面 App                 |
| macOS    | 稳定                     | 需桌面 App                 |
| iOS      | 需"Voiceover"辅助开启    | 需 EKeysApp（iOS 端 App）  |
| Linux    | 稳定                     | 需桌面 App                 |

> iOS BLE HID 默认拒绝媒体键遥控器；Apple 在"Voiceover → 蓝牙键盘"开启后才允许 HID 输入。本文档按"用户在 iOS 设置里开启 Voiceover"作为前提。

## 2. 总体架构

```
                        ┌────────────────────────┐
                        │  手机端 EKeysApp        │
                        │  (iOS/Android)         │
                        │  - 读取系统播放器状态   │
                        │  - 系统播放器控制入口   │
                        └─────┬───────────────┬──┘
                              │ BT（已连）     │ WiFi/USB
                              │   ↓            │   ↓
                              │  BLE HID       │  私有协议 JSON
                              │  Consumer 报告  │  CMD_MUSIC_STATUS 0x0e
                              │  (PLAY/NEXT…)  │  CMD_MUSIC_CONTROL 0x0f
                              ▼                ▼
┌─────────────────────────────────────────────────────────────┐
│ 主控 ESP32-S3                                               │
│ ┌──────────────────────┐    ┌─────────────────────────────┐ │
│ │ BLEKeyboardImpl       │    │ SerialProtocol (USB CDC/TCP)│ │
│ │  - HID Consumer 报告  │    │  - 解析 0x0e → MusicPlayer  │ │
│ │  - 媒体键码 0xE8..0xEC│    │  - 发送 0x0f → App         │ │
│ └──────────┬───────────┘    └──────────┬──────────────────┘ │
│            │ press(MediaKey)          │ 消息                 │
│            ▼                          ▼                     │
│ ┌──────────────────────┐    ┌─────────────────────────────┐ │
│ │ t-vk BleKeyboard      │    │ DisplayTask                │ │
│ │  HID over GATT       │    │  → ui_MusicScreenSecondary  │ │
│ └──────────────────────┘    │    (渲染标题/进度/控件态)   │ │
│                             └─────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

两条链路职责分明：

- **控制（主控 → 手机）**：BLE HID Consumer 报告。系统级通道，无需 App 运行。覆盖场景：手机锁屏、控制中心、Apple Watch 播放。
- **状态（手机 → 主控）**：EKeysApp 把系统播放器状态打包到现有 `CMD_MUSIC_STATUS(0x0e)` 推送。需 App 在前台或后台允许刷新。

## 3. 数据通道详设

### 3.1 BLE HID Consumer 报告（主控 → 手机）

利用 t-vk 库已实现的 BLE HID 多报告机制：

- 库内 Report ID `0x01` = Keyboard（已用）
- 库内 Report ID `0x02` = Consumer Control（待启用）
- 库内 Report ID `0x03` = Mouse（可选）

媒体键的 HID Usage Table：

| 操作       | Usage ID（Consumer Page 0x0C） | t-vk 库常量               |
| ---------- | ------------------------------ | ------------------------- |
| 播放/暂停  | 0xCD                           | `KEY_MEDIA_PLAY_PAUSE`    |
| 上一首     | 0xB6                           | `KEY_MEDIA_PREVIOUS_TRACK`|
| 下一首     | 0xB5                           | `KEY_MEDIA_NEXT_TRACK`    |
| 音量 +     | 0xE9                           | `KEY_MEDIA_VOLUME_UP`     |
| 音量 −     | 0xEA                           | `KEY_MEDIA_VOLUME_DOWN`   |
| 静音       | 0xE2                           | `KEY_MEDIA_MUTE`          |

发送规约（与 Keyboard 报告同源）：

- Consumer 报告长度 = 2 字节，Usage ID 为 little-endian uint16。
- t-vk 库的 `BleKeyboard::write(KEY_MEDIA_*)` 已经做了 report set → 50\~200ms → report 0 的"按下-释放"序列，**直接复用**。
- 媒体键不是"按住持续按住"的语义；**禁止**使用 `press/release` 持久按下，否则主机可能误判为按键粘连。

### 3.2 CMD_MUSIC_STATUS(0x0e)：App → 主控

现有协议已实现（[`src/protocol/commands/cmd_music.cpp`](../src/protocol/commands/cmd_music.cpp)）。**新增字段**（按需，本阶段选做）：

```json
{
  "cmd": 0x0e,
  "seq": 17,
  "data": {
    "music_status": {
      "connected": true,
      "is_playing": true,
      "is_paused": false,
      "can_prev": true,
      "can_next": true,
      "position_ms": 123400,
      "duration_ms": 240000,
      "title": "夜曲",
      "artist": "周杰伦",
      "player": "网易云音乐",
      "lyric_current": "一群嗜血的蚂蚁被",
      "lyric_next": "红光所吸引"
    }
  }
}
```

字段对齐参考 [`message_types.h::MusicPlayerInfo`](../src/message_types.h#L77-L93)，保持兼容。

### 3.3 CMD_MUSIC_CONTROL(0x0f)：主控 → App

**本阶段不真正实现**——仅在 BLE HID 不可达时（iOS 旧版 / 部分 Android 厂商 ROM 限制）给 App 兜底通道，App 收到后调系统 API 控制播放器。

```json
{"cmd": 0x0f, "seq": 0, "music_control": {"action": "play_pause"}}
```

可选 action：`play_pause` / `prev` / `next` / `volume_up` / `volume_down`。**优先级：BLE HID Consumer > 此命令**。

## 4. 固件端改造清单

### 4.1 `src/output/ConsumerControlCodes.h`（新建）

集中存放媒体键码 + 名称 ↔ HID Usage 映射，供 `BLEKeyboardImpl::press()` 与 UI ↔ `KeyResolver` 共享。

```cpp
namespace ekeys::output {

enum class MediaKey : uint8_t {
    PlayPause = 0,
    PrevTrack,
    NextTrack,
    VolumeUp,
    VolumeDown,
    Mute,
    Count,
};

// String（FEATURE_DOC §3.1 function_key 字段）→ MediaKey
bool mediaKeyFromString(const char* s, MediaKey& out);
// MediaKey → t-vk 库 KEY_MEDIA_* 编码（用于 press）
uint16_t mediaKeyToLibKeycode(MediaKey k);

}  // namespace ekeys::output
```

映射表与 §3.1 一致（Usage ID 0xCD / 0xB5 / 0xB6 / 0xE9 / 0xEA / 0xE2）。

### 4.2 `src/output/BLEKeyboardImpl.h/.cpp`

扩展接口：

```cpp
class BLEKeyboardImpl : public IKeyboard {
public:
    // 现有接口保留
    bool begin() override;
    void press(uint8_t keycode, uint8_t modifier = 0) override;
    void release(uint8_t keycode) override;
    ...

    // 新增：消费控制键（内部 BleKeyboard::write(KEY_MEDIA_*) 一击即放）
    void pressMediaKey(MediaKey key);
};
```

实现 `BLEKeyboardImpl::pressMediaKey()`：

1. 复用 `s_ble` 静态实例。
2. 调用 `s_ble->write(mediaKeyToLibKeycode(key))`。
3. 不依赖 `tracked_[]`（媒体键不是持续按下，无 releaseAll 副作用——但 t-vk 库 `releaseAll()` 已包含 Consumer 报告清零，见 `BleKeyboard.cpp` 历史 commit "Fixed MediaKeys releasing on ReleaseAll"）。

### 4.3 `src/keymap/KeyResolver.h/.cpp`

`KEY_FUNCTION_MEDIA_*` 命中后，改派 `BLEKeyboardImpl::pressMediaKey()` 而不是走普通 `press()`：

- `MEDIA_PLAY` / `MEDIA_PLAY_PAUSE` → `MediaKey::PlayPause`
- `MEDIA_PREV_TRACK` → `MediaKey::PrevTrack`
- `MEDIA_NEXT_TRACK` → `MediaKey::NextTrack`
- `MEDIA_VOLUME_UP` → `MediaKey::VolumeUp`
- `MEDIA_VOLUME_DOWN` → `MediaKey::VolumeDown`
- `MEDIA_MUTE` → `MediaKey::Mute`

`IKeyboard` 接口暴露 `pressMediaKey()`（默认实现 no-op），`USBKeyboardImpl` 实现为 no-op（USB HID Consumer 报告可后续追加，本阶段不做）。

### 4.4 `src/ui/ui_MusicScreenSecondary.c`

当前 `_request_control()` 把请求写到 `s_music_control_request` 后没消费方（[`cmd_music.h`](../src/protocol/commands/cmd_music.h) 注释：联调接入）。**接入点**：

```cpp
// 在 ui_event_MusicScreenSecondary 的按键处理里（旋钮 ENTER / NEXT / PREV）
// 调用 SerialProtocol::sendMusicControl("prev"|"play_pause"|"next")，
// 同时对 BLE 已连接时本地调用 BLEKeyboardImpl::pressMediaKey()。
```

具体修改：

1. `ui_event_MusicScreenSecondary()` 接 LV_KEY_ENTER → 调用 `consumer_->pressMediaKey(PlayPause)`（持有 `IKeyboard*` 弱引用）。
2. 接 `action=108`（矩阵键 8）/ `action=110`（矩阵键 10）→ 上一首/下一首映射，按上述双路派发。
3. BLE 未连接时只走 `sendMusicControl()`（依赖 App 兜底）。
4. 保持现有"乐观切换"语义（[`music_secondary_apply_optimistic_toggle_state`](../src/ui/ui_MusicScreenSecondary.c#L197-L202)），反馈延迟由下一次 `CMD_MUSIC_STATUS` 自然修正。

### 4.5 `src/ui/ui_MusicScreenSecondary.h`

- `ui_MusicScreenSecondary_request_control()` 保留枚举（向后兼容）。
- 新增 `ui_MusicScreenSecondary_bind_keyboard(IKeyboard* kb)`，DisplayTask 在 `applyMusicPlayer()` 之前注入当前键盘实例。

### 4.6 `src/tasks/DisplayTask.cpp`

在 `applyMusicPlayer()` 入口调用 `ui_MusicScreenSecondary_bind_keyboard(current_keyboard_.get())`，跟随 `SettingUpdate` 中 `work_mode` 变化刷新引用。

### 4.7 协议命令 `0x0f CMD_MUSIC_CONTROL` 发送通道

`SerialProtocol::sendMusicControl(action)` 已实现（[`SerialProtocol.cpp:179`](../src/protocol/SerialProtocol.cpp#L179-L187)）。**补全调用链**：在 `ui_MusicScreenSecondary` 的 PREV/TOGGLE/NEXT 三处事件内触发（详见 4.4）。

## 5. 移动端 App 协议约定

EKeysApp 通过蓝牙"已配对"通道外，无任何反向通道。状态推送走原有 WiFi/USB CDC 链路：

- iOS：App 进入前台或后台后，定时器（建议 1s）调用 `MPNowPlayingInfoCenter.currentNowPlayingItem` 拉取元数据，封装成 `CMD_MUSIC_STATUS` 经 WiFi/USB 推给主控。控制走 BLE HID（已实现）。
- Android：`MediaSessionManager` + `MediaController`，`playbackState.changes` 订阅 → 推送。
- BLE 不可达时（iOS Voiceover 未开启 / Android 厂商 ROM），App 接收 `0x0f` 后调系统 API 控制播放器，作为兜底。

> App 端的实现不在本文档范围；本文档只规定**主控端**必须实现的能力与协议字段。

## 6. UI 行为契约

| 用户操作                            | 主控动作                                                      |
| ----------------------------------- | ------------------------------------------------------------- |
| 旋钮在音乐屏 ENTER                 | 单击进入 `MusicScreenSecondary`                              |
| 旋钮在音乐二级屏 ENTER              | `pressMediaKey(PlayPause)`；已连 App 则同步 `sendMusicControl("play_pause")` |
| 旋钮顺时针 / 逆时针（音乐二级屏）   | `pressMediaKey(NextTrack)` / `pressMediaKey(PrevTrack)`       |
| 矩阵键 8（UP）                      | `pressMediaKey(PrevTrack)`                                    |
| 矩阵键 10（DOWN）                   | `pressMediaKey(NextTrack)`                                    |
| 矩阵键 9（CENTER）                  | `pressMediaKey(PlayPause)`                                    |
| 应用键映射为 `MEDIA_PLAY` 等         | `KeyResolver` 命中 → 派发到 `IKeyboard::pressMediaKey()`      |
| 长按 1s+                            | 不响应（HID Consumer 无"按住连续"语义，会被 iOS 视为粘连）   |

乐观更新：`pressMediaKey()` 调用后立即翻转 UI 播放态指示（黑胶动画 / 唱片角度），等待下一次 `CMD_MUSIC_STATUS` 真实回写修正。

## 7. 验收标准

- [ ] Android + Windows + macOS：配对后立即可控制 Spotify / 网易云 / Apple Music 播放暂停。
- [ ] iOS 14+：开启 Voiceover 后，可控制系统级播放；锁屏界面按设备键切换歌曲生效。
- [ ] App 推送 `CMD_MUSIC_STATUS` 后，UI 在 1s 内显示标题 / 艺人 / 进度条。
- [ ] BLE HID 发送后不影响 Keyboard Report 状态（modifier 计数无副作用）。
- [ ] 模式切换 USB ↔ BLE 时，原 BLE 控制键映射（`MEDIA_*`）在 USB 模式下 no-op，不触发任何副作用。

## 8. 风险与决策

### 8.1 iOS Voiceover 依赖

Apple 政策不允许第三方 App 通过普通 BLE HID 做系统级媒体控制，必须 Voiceover 开启。**决策**：在 `docs/TROUBLESHOOTING.md` / `README.md` 加一段"iOS 蓝牙媒体键使用前需开启 Voiceover"提示，不主动绕开（绕开违反 Apple 政策且 App Store 审核会被拒）。

### 8.2 与 ESP32-audioI2S 本地音频冲突

当 `audio_enable=true` 时，`Speaker` 走 `data/audio/*.wav` 本地播放。**媒体键不会作用于本地音频**——本地音频是按键触发的音效板（[`src/audio/AudioPad.cpp`](../src/audio/AudioPad.cpp)），不是播放器。两条链路互不干扰。

### 8.3 BLE HID 与 WiFi 共存

[F12 修复](file:///d:/search/esp/Keys/EKeys/src/output/BLEKeyboardImpl.cpp#L120-L132) 已在 `BLEKeyboardImpl::begin()` 中设置 `ESP_LE_AUTH_BOND`，避免 S3 的 CTKD 问题。媒体键走相同 GATT 服务（同一 `BleKeyboard` 实例），无额外兼容性风险。

### 8.4 媒体键是否需要配对前缓存

BLE 模式下手机未连时，`pressMediaKey()` 应立即返回（不发送），与现有 `press()` 行为一致。**决策**：复用 `s_ble->isConnected()` 守卫，行为与键盘键完全对称。

### 8.5 CMD_MUSIC_CONTROL(0x0f) 是否还要做

`docs/desktop-app-protocol.md §8.4` 已标注"UI 链路尚未接通"。本阶段把调用方补上即可，序列化 / 反序列化在 [`SerialProtocol.cpp`](../src/protocol/SerialProtocol.cpp#L179-L187) 已就绪。

## 9. 任务清单

- [ ] **10.1 `src/output/ConsumerControlCodes.h`**：媒体键枚举 + 字符串解析 + t-vk 库码映射。
- [ ] **10.2 `src/output/IKeyboard.h`**：新增 `virtual void pressMediaKey(MediaKey) { /* no-op */ }`。
- [ ] **10.3 `src/output/BLEKeyboardImpl.h/.cpp`**：实现 `pressMediaKey()`，走 `s_ble->write()`。
- [ ] **10.4 `src/output/USBKeyboardImpl`**：`pressMediaKey()` 保持 no-op（USB Consumer 报告后续另起阶段）。
- [ ] **10.5 `src/keymap/KeyResolver`**：识别 `MEDIA_*` 字符串 → `MediaKey`，派发到 `pressMediaKey()`。
- [ ] **10.6 `src/ui/ui_MusicScreenSecondary.h/.c`**：bind_keyboard 接口 + 事件回调内双路派发 + 乐观更新。
- [ ] **10.7 `src/tasks/DisplayTask.cpp`**：在 `applyMusicPlayer()` 注入当前 `IKeyboard*`。
- [ ] **10.8 `docs/TROUBLESHOOTING.md`**：追加 iOS Voiceover 开启步骤。
- [ ] **10.9 `docs/desktop-app-protocol.md §8.4`**：把"未接通"改为"已接通"，补 action 取值表。
- [ ] **10.10 联调**：Android（系统播放器 + Spotify）/ macOS（Apple Music）/ Windows（Spotify UWP）/ iOS Voiceover 开启后控制 Apple Music；任一不通过则回滚 10.6 双路逻辑。

## 10. 变更记录

- 2026-09-17：方案文档创建。基于 [`src/output/BLEKeyboardImpl.cpp`](../src/output/BLEKeyboardImpl.cpp) 现状盘点 + [`src/protocol/commands/cmd_music.cpp`](../src/protocol/commands/cmd_music.cpp) 已存在的 0x0e / 0x0f 通道 + [`src/ui/ui_MusicScreenSecondary.c`](../src/ui/ui_MusicScreenSecondary.c) 已存在的 `request_control()` 未接通缺口。给出最小改造路径（`ConsumerControlCodes.h` 新增 + `pressMediaKey()` 4 处改动），不引入新协议栈、不改 BLE 控制器配置。