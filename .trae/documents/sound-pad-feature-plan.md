# 音效板（Sound Pad）功能实现计划

## Context（背景与目标）

为 EKeys 新增"音效板"功能：设备端新增一个**音效 UI 页**，在该页按下矩阵键 1~11 播放各自绑定的音频文件；**桌面 App 新增"音效"配置页**，可上传音频文件到设备、管理文件空间、为 11 个键绑定音频并试播。

用户已确认的设计决策：
- **音频来源**：App 上传（主）+ `data/` 编译预置（重烧 SPIFFS 镜像会重置为预置内容）
- **配置粒度**：每键仅绑定文件；音量全局复用现有 `device_volume`
- **页面位置**：导航环 主屏→键映射→音乐→**音效**→PC状态→HA→设置→(回主屏)
- **存储**：复用现有 storage SPIFFS 分区（0x620000，3.875MB，现仅存 config/键映射小文件），**不改分区表**

关键现状（已核实）：
- 实际音频库为 `esphome/ESP32-audioI2S@^2.3.0`（platformio.ini L87），`Speaker::PlayLocalAudio(path)`（connecttoFS 按扩展名选解码器，支持 MP3/WAV）**已实现但无调用方**，含 SPIFFS.exists 检查与录音互斥守卫，直接复用
- 录音互斥：`VoiceRecognizer.cpp` L72-75 采集前会强制 `Speaker::Stop()`
- 协议 JSON 行，行缓冲 2048B（串口+TCP 同），命令枚举至 0x13；文件传输先例 = 0x11 图标 base64 上传（单行小文件，音频需分块）
- App 端无原生文件选择器，沿用 panel_settings.rs 图标上传模式：TextEdit 填路径 + `std::fs::read` + 客户端校验
- App 请求模式：`with_link(|lm| lm.request(cmd, data, timeout))` 同步阻塞，router 线程按 seq 配对

---

## 一、协议设计（新增 2 命令）

### 0x14 `CMD_AUDIO_FILE`（文件管理，data.op 分发）
| op | 请求 data | 响应 data |
|----|-----------|-----------|
| `list` | — | `files:[{name,size}]`（上限 64 条）、`total_bytes/used_bytes/free_bytes` |
| `begin` | `{name, size}` | ack；校验后创建 `/name.part` |
| `data` | `{name, index, b64}` | `{received:N}`；1024B 二进制/块（b64 后 ~1.4KB < 2048 行缓冲） |
| `end` | `{name, size}` | 校验大小一致 → `SPIFFS.rename(.part → 终名)` 原子提交 → 回 `free_bytes` |
| `abort` | `{name}` | 删 `.part`（App 取消/失败回滚） |
| `delete` | `{name}` | 删终名；**先扫描绑定表清除引用该文件的键并持久化**，响应带回更新后 pads |

约束（固件侧强校验）：
- 文件名白名单 `^[a-z0-9_]{1,20}\.(mp3|wav)$`，存 SPIFFS **根目录**（SPIFFS 对象名上限 32 字符：`/xxx.mp3` ≤25、`.part` ≤30 均安全，不建子目录）
- 单文件 ≤ 2MB；`begin` 时校验 `free_bytes ≥ size + 64KB headroom`
- 上传互斥：同一时刻仅一个 in_progress；正在播放的同名文件拒绝 delete

### 0x15 `CMD_AUDIO_PAD`（绑定与播放控制，data.op 分发）
| op | 请求 | 响应 |
|----|------|------|
| `get` | — | `pads:[{key, file}]`（11 键全量，空绑定 `""`） |
| `set` | `{key, file}`（单键即改即发，`file:""` 清除） | 校验文件存在于 /audio 白名单内 → 落盘 → 响应该键绑定 |
| `play` | `{key}` 或 `{file}`（试播） | 立即 ack |
| `stop` | — | ack |

---

## 二、固件改动（EKeys）

### 新增文件
1. **src/audio/AudioPad.h/.cpp** — 音效板业务单例：
   - 成员：`char bindings_[11][25]`、`uint8_t playing_key_`
   - `load()`：读 `/audio_pad.ini`（格式与 ConfigStore INI 风格一致，如逐行 `padN=xxx.mp3`），文件不存在用空表
   - `setBinding(key, file)`：改缓存 + 持久化 + 投递 DisplayMessage 刷 UI
   - `trigger(key)`（UI 按键）：未绑定→no-op+log；已绑定→若在播先 `Speaker::Stop()`，再 `PlayLocalAudio`，记 `playing_key_`（录音互斥由 Speaker 内部守卫兜底）
   - `play(file)`（App 试播，`playing_key_=0`）、`stop()`、`isPlaying()`
   - `loop()`：MainTask tick 调用，检测 `playing_key_!=0 && !Speaker::isRunning()` → 播完投消息清高亮
2. **src/protocol/commands/cmd_audio.h/.cpp** — 0x14/0x15 handler，结构参照 cmd_profile.cpp（`registerAudioHandlers/unregisterAudioHandlers`）
3. **src/ui/ui_AudioScreen.h/.c** — 手写 SquareLine 风格 C 屏：
   - 3×4 网格：11 个音效格（两行 label：键号大字 + 文件名短显示截断）+ 1 状态格（空间/提示）
   - `LV_EVENT_KEY`：ENTER→停止播放、ESC→回主屏（LEFT/RIGHT 交给 DisplayTask 既有转发逻辑切屏）
   - setter：`ui_AudioScreen_set_pads(const char files[11][25])`、`ui_AudioScreen_set_playing(uint8_t key)`（key=0 清全部高亮）

### 修改文件
4. **src/protocol/SerialProtocol.h** — 枚举加 `CMD_AUDIO_FILE = 0x14, CMD_AUDIO_PAD = 0x15`
5. **src/protocol/registration.cpp** — 注册/注销 cmd_audio
6. **src/ui/ui_KeyMapped.h** — `UI_SCREEN_MUSIC_SECONDARY` 后插 `UI_SCREEN_AUDIO`（isSecondaryScreen 为显式 switch，不受影响）
7. **src/ui/ui.c / ui.h** — `ui_init` 补 `ui_AudioScreen_screen_init()`、`ui_destroy` 补销毁、extern 声明
8. **src/ui/ui_MusicScreen.c** — RIGHT 目标 MUSIC→AUDIO 改为 →PC_STATUS 改指 AUDIO；**src/ui/ui_PcStatusScreen.c** — LEFT 目标 MUSIC 改指 AUDIO（导航环其余不动）
9. **src/tasks/DisplayTask.cpp**：
   - `navigateNow` 加 `case UI_SCREEN_AUDIO → ui_AudioScreen`
   - `applyMessage` ActionInput 矩阵键分支加：`tag==UI_SCREEN_AUDIO` → `AudioPad::instance().trigger(key_id)`，成功后同线程直接 `ui_AudioScreen_set_playing(key_id)`（不另发消息）
   - 新增 `DisplayMessageType::AudioPad`（message_types.h：`AudioPadInfo{char files[11][25]; uint8_t playing_key;}`）+ apply 分支调两个 setter（绑定变更/播完清高亮走此通道）
10. **src/tasks/MainTask.cpp** — L329 `suppress_hid` 加 `|| tag == UI_SCREEN_AUDIO`（release 循环保持原样防卡键）；`Speaker::loop()`（L682 旁）加 `AudioPad::instance().loop()`
11. **src/app/AppContext.cpp** — `init()` 中 `MainTask::begin()` 前调 `AudioPad::instance().load()`

### 预置音效
12. **data/** 根目录放预置 MP3（如 `k01.mp3`）→ `pio run -t buildfs` 打包。⚠️ mkspiffs 对子目录（data/audio/）的路径展开行为不确定，**实现时验证**；若保留 `/audio/` 前缀则与根目录白名单冲突，规避方案 = 预置文件直接放 data/ 根（用户已确认"预置+上传"双通道，预置放根目录即可）

---

## 三、App 改动（EKeysApp）

13. **src/protocol.rs** — `CMD_AUDIO_FILE=0x14`、`CMD_AUDIO_PAD=0x15` 常量 + 请求/响应 serde 结构（ListResp、UploadBegin/Data/End、PadState、PadSetReq 等）+ 单测（参照现有测试风格）
14. **src/state/mod.rs** — `Page::Audio`（Voice 后插入）；`AppHandle` 加 `audio: Arc<Mutex<AudioPadData>>`（文件列表/空间/pads/`upload: Option<UploadProgress>`）；连接后 auto_get 链末尾追加 0x15 get + 0x14 list（失败仅记日志不弹 Toast，与 0x03/0x07 同策略）
15. **src/ui/panel_audio.rs**（新）— 不走 settings_panel_scaffold/draft/diff（即改即发）：
    - 空间条（used/total）+ 文件列表（名字/大小/试播/删除）
    - 上传区：TextEdit 填路径（沿用 0x11 图标上传模式）→ 客户端校验扩展名/≤2MB → **后台线程**分块上传（`begin → data×N → end`，1024B/块，每块独立 request 超时 2s，失败重试 2 次，可取消）→ 进度写 `audio.upload`，经 UiEvent 驱动进度条/完成 Toast 并自动刷新列表；串口连接时提示"USB 串口约 10KB/s，大文件建议 TCP 连接"
    - 11 键绑定：每键一个 ComboBox（未绑定 + 文件列表），选中即发 0x15 set；附试播/停止按钮
16. **src/app.rs** — 快捷键导航顺移（音效=Ctrl+7，日志=Ctrl+8，关于=Ctrl+9）；`handle_link_event` 补 0x94/0x95 识别（主要靠 request 内 seq 配对，迟到响应按 ACK 日志处理）
17. **src/ui/sidenav.rs** — ITEMS 插入"音效"项，日志/关于 hint 顺移

---

## 四、文档

18. **docs/desktop-app-protocol.md** — 新增 0x14/0x15 章节（报文示例、白名单、2MB/1024B 分块约束、错误码）
19. **docs/protocol-usage.md** — 命令表同步
20. **EKeysApp/docs/ui-design.md** — 补音效页设计；固件相关 docs 补导航环描述（音乐→音效→PC状态）

---

## 五、实现顺序

1. cmd_audio（0x14/0x15）+ AudioPad 模块（纯后端，无 UI）
2. 枚举 `UI_SCREEN_AUDIO` + 导航环（Music/PcStatus 左右键）+ DisplayTask 路由 + MainTask suppress_hid/loop
3. ui_AudioScreen + DisplayMessage 刷新链
4. App：protocol.rs → state/Page/sidenav/app.rs → panel_audio.rs（先 get/list/绑定，后上传）
5. 文档

## 六、验证

- **固件**（项目规则：不主动编译，建议用户执行）：
  - `pio run` 编译通过；`pio run -t buildfs` 验证预置打包（检查 mkspiffs 子目录行为，决定预置文件放根目录）
  - 烧录后手测：App 上传短 MP3 → 试播 → 绑定键 1 → 旋钮切到音效页按键 1 播放/高亮 → ENTER 停止 → 录音（语音触发）期间按键被拒绝 → 删除已绑定文件后确认绑定清除 + `/audio_pad.ini` 落盘 → 断电重启绑定/文件仍在
- **App**：`cargo check`、`cargo test`（protocol 单测；target\debug\deps 需沙箱外运行）
- **存储边界**：上传 >剩余空间的文件被拒（64KB headroom）；`list` 的 used/free 与 SPIFFS.info() 一致

## 七、风险清单

- **ESP32-audioI2S 2.3.0 解码 CPU**：44.1kHz 立体声 MP3 与 LVGL 抢核可能爆音/卡帧 → 建议音源用 32~44.1kHz **单声道**、64~128kbps，实测为准
- **上传占链路**：每块 request 持 link mutex，与 1s 推送 tick 串行竞争，UI 偶发 ~200ms 卡顿，可接受；拔线 → request 立即 Err → 线程中止 + 固件 abort，重连恢复
- **SPIFFS 碎片**：频繁删/写大文件可能"free 足但写不进"，headroom 64KB + .part/rename 原子提交缓解；极端情况擦除重烧 SPIFFS 恢复
- **LVGL 内存**：音效页仅 label 文本，无大缓冲；`set_pads` 传 C 数组避免 String 分配
