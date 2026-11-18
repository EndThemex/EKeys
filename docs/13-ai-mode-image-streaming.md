# 阶段 13 — AI Mode 图像接收与显示

> 状态：设计方案（未实现）
> 2026-11-18 评审修订：负载上限按 type 区分（RGB565 直写不受 64KB 限制）；激活钩子从 navigateNow 移到 run() 主循环 tag 比对；USB 分流补齐 `'\n'` 归属与 SerialProtocol 门控规则；设备→主机控制帧改 JSON 行；frame_id 回绕、队列满槽回收、WiFi+RGB565 帧率估算修正。
> 关联文档：[`05-ui-screens.md`](./05-ui-screens.md) / [`06-network-voice-rgb-audio.md`](./06-network-voice-rgb-audio.md) / [`desktop-app-protocol.md`](./desktop-app-protocol.md)
> 关联代码：[`ui_types.h`](../src/ui/ui_types.h) / [`DisplayTask.cpp`](../src/tasks/DisplayTask.cpp) / [`LvglPort.cpp`](../src/display/LvglPort.cpp) / [`SerialProtocol.cpp`](../src/protocol/SerialProtocol.cpp) / [`WiFiManager.cpp`](../src/network/WiFiManager.cpp)

## 1. 目标

新增一个一级页 **AI MODE**。进入该页后，设备通过 **USB CDC（串口）** 或 **WiFi TCP** 接收外部主机推送的图像帧，并实时渲染到 428×142 屏幕上；离开该页后停止接收并释放资源。

**不做的事**（避免范围膨胀）：

- 不做端侧 AI 推理、不解析图像语义，只做"图像接收 + 显示"。
- 不改动现有 HID 按键行为、键映射、App 的 JSON 行协议（0x01~0x17）。
- 不把图像写入 SPIFFS/Flash（写放大与寿命问题，见 [`03-config-persistence.md`](./03-config-persistence.md)）。

## 2. 现有硬件与固件基线（约束条件）

| 项目 | 实测/配置值 | 依据 |
| --- | --- | --- |
| MCU | ESP32-S3（N16R8：16MB Flash / 8MB OPI PSRAM），240MHz 双核 | [`platformio.ini`](../platformio.ini) |
| 屏幕 | NV3007，物理 142×428，`rotation=1` → 逻辑 **428×142** | [DisplayDriver.cpp](../src/display/DisplayDriver.cpp#L39-L67) |
| 像素格式 | RGB565，`LV_COLOR_DEPTH=16`、`LV_COLOR_16_SWAP=0` | [lv_conf.h](../include/lv_conf.h#L34) / [ui.c](../src/ui/ui.c#L18-L23) |
| LCD 接口 | SPI，时钟 **40MHz**（`begin(40000000)`） | [main.cpp](../src/main.cpp#L38) |
| LVGL | 8.3.11，单绘制缓冲 16 行（428×16×2 ≈ 13.7KB，内部 RAM） | [LvglPort.cpp](../src/display/LvglPort.cpp#L26-L28) |
| LVGL 内存池 | 128KB，整体分配在 PSRAM | [lv_conf.h](../include/lv_conf.h#L54) / [LvglMemPool.c](../src/display/LvglMemPool.c#L14-L24) |
| LVGL 刷新周期 | `LV_DISP_DEF_REFR_PERIOD` 默认 **30ms** | `lvgl/src/lv_conf_internal.h` |
| 全屏刷新代价 | 9 个 16 行分片，每片后 `vTaskDelay(1)` → 约 9ms 额外开销 | [LvglPort.cpp](../src/display/LvglPort.cpp#L78-L103) |
| 任务布局 | DisplayTask：Core0 / 优先级 1 / 栈 8KB；MainTask 由 Arduino loopTask 驱动（Core1） | [DisplayTask.cpp](../src/tasks/DisplayTask.cpp#L110-L117) |
| LVGL 线程模型 | 只有 DisplayTask 调用 LVGL，其他任务只投递消息 | [DisplayTask.cpp](../src/tasks/DisplayTask.cpp#L153-L262) |
| 现有串口协议 | JSON 行，行缓冲 2048B，日志与协议共用 USB CDC | [SerialProtocol.h](../src/protocol/SerialProtocol.h#L99) |
| 现有 WiFi | STA 状态机，`wifi_switch && work_mode != BLE` 才启用 | [WiFiManager.cpp](../src/network/WiFiManager.cpp#L54-L59) |
| 页面导航 | 一级页首尾相连：MAIN ↔ KEYMAPPED ↔ MUSIC ↔ AUDIO ↔ PC_STATUS ↔ HA ↔ SETTING ↔ MAIN | 各 `ui_*Screen.c` 事件处理 |

### 2.1 由此推导出的硬性红线

1. **AI 解码任务绝不能直接调用 LVGL**：像素缓冲的"上屏"必须在 DisplayTask 上下文完成（`lv_img_set_src` + `lv_obj_invalidate`）。
2. **全屏帧固定 121,552 字节**（428×142×2）。SPI 40MHz 的理论下限约 24.3ms，叠加分片让出约 33ms/帧，即**显示链路本身的天花板约 25~30fps**。
3. **大块像素缓冲只能放 PSRAM**。内部 RAM 曾因 LVGL 双缓冲被占导致 ASR TLS 握手失败（[LvglPort.cpp](../src/display/LvglPort.cpp#L32-L40) 注释），AI 模式不得再占用 100KB 级内部 RAM。
4. **不能直接用 LVGL 自带 SJPG**：`lv_sjpg` 面向"预切片的 SD 卡 JPEG 文件"，不支持网络连续流；项目当前也未开启 `LV_USE_SJPG`（默认 0）。
5. **AI 页必须显式豁免两条既有策略**，否则页面会被自动切走或黑屏：
   - 一级页 5s 无操作自动回主页（[`isAutoReturnEnabled()`](../src/tasks/DisplayTask.cpp#L783-L796)）；
   - 电源模式 30s 无操作降亮度/关背光（[`checkPowerIdle()`](../src/tasks/DisplayTask.cpp#L817-L862)）。

## 3. 方案总览

```
外部主机（PC / 树莓派 / 手机）
   │  ① USB CDC 二进制帧           ② WiFi TCP :30001
   ▼                                    ▼
AiLink（帧解析 + 分片重组 + CRC + 流控）   ← MainTask::loop() 内非阻塞泵
   │  完整帧就绪（FramePool 空闲槽）
   ▼
AiTask（Core1，低优先级；JPEG/TJPGDec 解码 → PSRAM 帧槽）
   │  DisplayMessage::AiFrame（只带槽号，不搬像素）
   ▼
DisplayTask（Core0，唯一 LVGL 线程）
   │  lv_img_set_src(dsc[slot]) + lv_obj_invalidate()
   ▼
LvglPort flush（16 行分片）→ NV3007（428×142）
```

**关键决策**（评审重点）：

| 决策点 | 结论 | 理由 |
| --- | --- | --- |
| 主图像格式 | **JPEG 为主**，RGB565 原始帧/RLE565 为辅 | JPEG 12KB 级 vs 原始 118.7KB，串口下差 10 倍 |
| 传输通道 | **自定义二进制帧**（USB CDC 与 TCP 同一套帧格式） | 复用 JSON 行协议时 base64 +33%、行缓冲 2KB 需切片、ArduinoJson 解析开销 |
| WiFi 角色 | 设备做 **TCP Server（30001）**，不改现有 30000 客户端通道 | 发送端可以是任意脚本/相机，不依赖桌面 App |
| 渲染方式 | `lv_img` + `lv_img_dsc_t` 直指 PSRAM 帧缓冲（不做 canvas） | 零额外拷贝、零额外分配；尺寸与屏完全相同不会平铺 |
| 帧缓冲 | PSRAM 三槽环形（3×121,552B ≈ 356KB） | 写槽 / 排队 / 正在显示互不重叠，天然避免撕裂 |
| 解码位置 | 独立 AiTask，Core1，优先级与 loopTask 同级 | 不阻塞 DisplayTask 的 LVGL tick，也不抢占 HID 扫描 |

## 4. 传输层设计

### 4.1 为什么不用现有 JSON 行协议

| 指标 | JSON + base64 | 二进制帧 |
| --- | --- | --- |
| 原始 RGB565（121,552B） | ≈162KB（+33%） | 121.5KB |
| 12KB JPEG | ≈16KB | 12KB |
| 每帧解析次数 | 30+ 次 `ArduinoJson` 解析（行缓冲 2KB 上限） | 0 次（按偏移 memcpy） |
| 对现有协议/App 的影响 | 需新增 0x18 命令 + App 改造 | 新增独立入口，旧协议零改动 |

结论：走二进制。JSON 行协议继续服务配置/状态，两者在 USB CDC 上通过魔数分流（§4.4）。

### 4.2 帧格式（小端）

**帧头 24 字节**：

| 偏移 | 类型 | 字段 | 说明 |
| --- | --- | --- | --- |
| 0 | u16 | magic | `0x5AA5`（字节序 `A5 5A`） |
| 2 | u8 | version | 1 |
| 3 | u8 | type | 1=JPEG，2=RGB565，3=RLE565，4=控制帧 |
| 4 | u16 | frame_id | 发送端自增，回绕 |
| 6 | u16 | width | 期望 428 |
| 8 | u16 | height | 期望 142 |
| 10 | u8 | flags | bit0=`scale_half`（JPEG 按 1/2 输出），bit1=带 frame_crc32 |
| 11 | u8 | reserved | 0 |
| 12 | u32 | payload_len | 压缩/原始负载总字节 |
| 16 | u32 | frame_crc32 | 负载 CRC32，0=不校验 |
| 20 | u16 | chunk_payload | 每片负载上限：USB=4096，TCP=8192 |
| 22 | u16 | header_crc16 | 覆盖 0~21 字节 |

**分片头 8 字节** + 负载：

| 偏移 | 类型 | 字段 |
| --- | --- | --- |
| 0 | u16 | frame_id |
| 2 | u16 | chunk_index（从 0 开始） |
| 4 | u16 | chunk_len |
| 6 | u16 | chunk_crc16（0=不校验） |

- 分片必须严格按 `chunk_index` 递增到达（USB CDC 与 TCP 都是可靠有序流，不做乱序重组）。
- 分片数 = `ceil(payload_len / chunk_payload)`；最后一包允许小于上限。
- 任一 CRC 失败、负载超限 → 丢弃本帧并回 NACK，等待下一个 magic 重新同步。**负载上限按 type 区分**：
  - JPEG / RLE565：`payload_len ≤ kAiMaxPayloadBytes`（64KB，需组装缓冲）；
  - RGB565 原始帧（type=2）：`payload_len ≤ kAiMaxRawPayloadBytes`（128KB，分片直写帧槽、不占组装缓冲，121,552B 在限内）。原 64KB 统一上限会杀死 RGB565 通道（121,552B 超限 86%），必须分开。
- `frame_id` 为 u16 自增回绕：序号跳变检测用 `(uint16_t)(new - last)`，差值为 1（或 0=重发）才接受，否则视为跳变丢弃。
- 半帧超时：≥300ms 收不到新字节 → 复位解析器。

**控制帧**（方向不对称）：

*主机→设备*：二进制帧，`type=4`，帧头后 12 字节负载：

```
u8  kind      1=PING
u8  result    0=OK 1=CRC 2=尺寸/超限 3=busy(无空闲槽) 4=超时
u8  free_slots
u8  dropped_frames
u16 frame_id
u16 fps_x10
u32 bytes_per_sec
```

*设备→主机*：一律走 **JSON 行**（与日志、现有 JSON 响应共用输出流，主机按行解析，零歧义——二进制控制帧混入 JSON/日志流会让主机端无法可靠定界）：

```json
{"ai_ctl":{"kind":"hello","result":0,"frame_id":0,"w":428,"h":142,
 "formats":[1,2,3],"chunk_payload":8192,"max_payload":65536,
 "max_raw_payload":131072,"version":1,"free_slots":2,"dropped":0,
 "fps_x10":0,"bytes_per_sec":0}}
```

- 主机连接后（TCP 连接建立 / USB 进入二进制态前）先收 `hello` JSON 行，再开始发帧；`hello` 携带宽高、支持格式、chunk 建议值、两类负载上限与版本。ACK/NACK/统计同样以 `ai_ctl` JSON 行回传。
- **流控**：主机最多保留 **2 帧未 ACK**；收到 `busy` 则停发至下一 ACK。没有流控时 TCP 会把积压帧堆在 socket 缓冲区，表现为"越播越延迟"。

**组装缓冲的归属（JPEG 必需）**：JPEG 必须以连续内存交给 TJpgDec，因此设 **2 个 64KB 组装缓冲做乒乓**：

- `AiLink`（MainTask 上下文）收满一帧后占住一个 assembler，投递 `AiJpegJob{assembler_idx, len, frame_id}` 给 AiTask；
- AiTask 解码完成后释放该 assembler 并回 ACK；
- 两个 assembler 都忙 → 回 `busy` 丢帧（同时 `dropped_frames++`）。
- RGB565/RLE 不需要 assembler：分片按偏移直接写入目标帧槽（该槽处于 `Writing` 态，解码完成即 `Queued`）。
- `commitWriteSlot()` 投递 `AiFrame` 消息失败（DisplayTask 队列满）→ 槽直接回 `Free`、`dropped_frames++`，避免槽泄漏卡死 FramePool。

### 4.3 WiFi 通道

- 传输：`WiFiServer server(30001)`，仅允许 1 个客户端，新连接踢掉旧连接。
- 生命周期：仅在 AI 页激活期间 `server.begin()`；离开页面 `client.stop(); server.end();`。
- 前置条件：`WiFiManager::isEnabled()` 为真且 `isConnected()`。不满足时 AI 页显示 `WIFI OFF`（不做隐式改配置）。
- 复用现有 STA 配置（`wifi_ssid/password`），**不新增 WiFi 状态机**，避免与 [`WiFiManager`](../src/network/WiFiManager.cpp) 的重连/宽限逻辑打架。
- 安全边界：局域网明文，无鉴权。默认只读不写设备；如需鉴权，在 HELLO 握手中加 8 字节 token（可选项，P3 再做）。
- 吞吐估算：2.4GHz LAN 下 TCP 有效 1~4MB/s，远高于 12KB/帧 × 20fps = 240KB/s 的需求。

### 4.4 USB CDC 分流（关键改动点）

现状：[`SerialProtocol::poll()`](../src/protocol/SerialProtocol.cpp#L36-L73) 独占 `Serial.read()`，逐字节拼 JSON 行。若二进制帧直接混入，会被当作超长行丢弃或触发无意义的解析错误。

改法（单读者 + 二进制优先）：

1. `MainTask::loop()` 中先调用 `AiLink::pollSerial()`，再调用 `SerialProtocol::poll()`。
2. `AiLink::pollSerial()` 仅在 AI 页激活时工作；**非二进制态**每 tick 最多消费 **8KB**（避免长时间占用 MainTask，按 5ms tick 折算仍可达 1.6MB/s）；**二进制态必须排空本次 `Serial.available()` 的全部字节**（原因见规则 5）。
3. 发送端必须在新帧前先发 `'\n'`（0x0A）再发 magic `A5 5A`。`'\n'` 的归属规则（关键，否则 JSON 行永远无法解析）：
   - `SerialProtocol` 新增 `feedByte(char)` 与 `isLineBufferEmpty()`：`'\n'` 到达时若行缓冲**非空** → 它是 JSON 行结束符，必须透传 `feedByte('\n')`；
   - 行缓冲**为空**时消费到 `'\n'` → AiLink 吞掉并置"期望 magic"标志（该标志**跨轮保持**）；
   - 下一个字节若为 `0xA5` → 进入二进制态，按帧头/分片长度精确消费；否则清除标志并透传 `feedByte()`，保证普通 JSON 完全不受影响（ASCII JSON 不可能以 `0xA5` 开头）。
4. 二进制态下所有字节都归 AI 解析器，帧结束（最后一包收满）或超时后回到行模式。
5. **二进制态下 `SerialProtocol::poll()` 必须被门控**：其 `while (Serial.available())` 会无条件消费全部字节，若 AiLink 因 8KB 预算提前退出，剩余帧数据会被当成 JSON 行垃圾（line overflow 告警、伪造 parse error 响应、甚至吃掉下一帧的 `0xA5` 首字节）。实现二选一：AiLink 在二进制态绕过预算排空；或给 SerialProtocol 增加"AI 二进制态挂起"标志，置位时 `poll()` 直接返回。
6. 已知限制：二进制态期间到达的 JSON 心跳/配置命令会被帧解析器吞掉（帧 CRC 失败）。USB 单流上二进制帧与 JSON **无法安全交织**，"JSON 命令可用"仅在帧间隙成立；连续推流时发送端应暂停发 JSON，或利用帧间隙发送。
7. 设备 → 主机方向不需要分流：ACK/HELLO/统计走 JSON 行（§4.2），日志与现有 JSON 响应保持不变。

> 注意：USB CDC 的 `Serial.begin(115200)` 只是虚拟波特率，实际吞吐由 TinyUSB Full-Speed 决定（实测约 0.7~1.2MB/s），不受 115200 限制。

## 5. 图像格式与解码

### 5.1 JPEG（推荐主通道）

- **不使用** LVGL `lv_sjpg`（面向已切片文件），把 **TJpgDec**（LVGL 依赖里自带 `sjpg/tjpgd.c`，Public Domain）收编到 `lib/tjpgd/` 或 `src/ai/jpeg/`，按需改配置：
  - `JD_FORMAT = 1`（直接输出 RGB565，与 LVGL/面板一致，免 RGB888→565 转换）——**现状 0，需改**；
  - `JD_FASTDECODE = 2`（32 位桶形移位 + Huffman 查表，32 位 MCU 上约 2~3 倍加速）——**现状 0，需改**；
  - `JD_SZBUF = 512`（输入窗口）；解码工作区约 6~8KB——现状已满足，无需改；
  - `JD_USE_SCALE = 1`（支持 1/2、1/4、1/8 降采样）——现状已满足，无需改。
- **宏守卫剥离（vendoring 必做）**：LVGL 内的 `sjpg/tjpgd.c` 整个文件被 `#if LV_USE_SJPG` … `#endif` 包裹（L28 起），而本项目 `lv_conf.h` 未定义 `LV_USE_SJPG`（默认 0），直接收编会得到空翻译单元（或因 `tjpgd.h` include 路径变化编译失败）。复制到 `lib/tjpgd/` 时需：
  1. 删除/替换 `#if LV_USE_SJPG` 守卫（可改为恒真，或直接去掉该行与文件末尾配对的 `#endif`）；
  2. 一并复制 `tjpgd.h`、`tjpgdcnf.h`，确认 `#include "tjpgdcnf.h"` 路径可解析（`lib/tjpgd/` 平级放置即可）；
  3. 不要在 `lv_conf.h` 开 `LV_USE_SJPG`（会把 lv_sjpg 一并链入，与本文方案无关）。
- 解码目标：直接写入 PSRAM 帧槽，stride = 428。
- **高画质技巧**：发送端发 **856×284** 的 JPEG，设备 `scale=1/2` 输出 428×142。文字/线条更锐利，且 TJpgDec 跳过高频 IDCT 后解码更快。
- RGB565 字节序：TJpgDec 在 little-endian MCU 上输出的 16 位像素内存布局低字节在前，与 `LV_COLOR_16_SWAP=0` 的 LVGL 缓冲一致，可零转换。

### 5.2 RGB565 原始帧（低延迟/无解码）

- 定义：每像素 u16 小端（`R5G6B5`，低字节先）。
- 与面板/flush 路径一致（[`my_disp_flush`](../src/display/LvglPort.cpp#L78-L103) 走 `draw16bitRGBBitmap`，swap=0），接收时**直接按分片偏移写入帧槽**，不需要额外组装缓冲。
- 负载 121,552B 超出 JPEG 类的 64KB 上限，适用 `kAiMaxRawPayloadBytes`（128KB，§4.2）。
- 代价：121,552B/帧，USB 下约 110~150ms → 6~8fps；WiFi 下约 30~120ms（按 TCP 1~4MB/s 推算，偏乐观项需实测）→ 6~13fps。

### 5.3 RLE565（可选折中）

- 简单 PackBits 风格：`[run_len u16][pixel u16]`，或者 `[控制字节][像素]`。
- 典型 UI 截图压缩比 3~6:1；解码为纯 memcpy 级别，几乎不占 CPU。
- 仅在"主机无 JPEG 编码器"或"要极低 CPU"时使用，作为 P2 的可选项。

### 5.4 尺寸适配

- 要求发送端输出 428×142（JPEG 可用 856×284 + `scale_half`）。
- 设备端**不做任意比例缩放**（最近邻缩放要额外一次 60k 像素读写，得不偿失）；宽高不匹配直接 NACK `result=2`，并在 HUD 显示实际收到的尺寸，便于排查。
- 未来如必须适配，只实现整数倍最近邻（1/2、1/4），与 TJpgDec 的 scale 语义保持一致。

## 6. 帧缓冲与渲染

### 6.1 FramePool（PSRAM 三槽）

```cpp
// src/ai/AiFramePool.h
struct AiFrameSlot {
    uint8_t  *px;          // PSRAM，428*142*2
    uint16_t  w, h;
    uint32_t  seq;         // frame_id
    uint8_t   state;       // Free / Writing / Queued / Displaying
};

class AiFramePool {
public:
    bool  ensureAllocated();          // 首次进入 AI 页时分配，之后常驻
    int   acquireWriteSlot();         // 返回 -1 表示三槽均忙（丢帧）
    bool  commitWriteSlot(int slot);  // Writing -> Queued，投递 AiFrame 消息
    int   takeQueuedSlot();           // DisplayTask：Queued -> Displaying
    void  releaseDisplaySlot(int slot);// DisplayTask：Displaying -> Free
    const lv_img_dsc_t *dsc(int slot) const;
};
```

- 三槽意义：`Displaying`（LVGL 正在逐片读取）与 `Writing`（解码写入）永不重合，第三槽做"已解码待上屏"的排队，避免解码等待刷新。
- 状态字段读写在两个核之间：用 `portMUX_TYPE` 临界区（或 32 位原子操作）保护状态迁移；像素内容本身不需要锁（槽独占）。
- 分配时机：第一次进入 AI 页时用 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` 一次性分配，退出页面**不释放**（避免反复 350KB 分配导致 PSRAM 碎片）。若分配失败，页面进入 `ERROR` 状态并提示。

### 6.2 LVGL 侧

- 每个槽持有一个常驻 `lv_img_dsc_t`：

```c
dsc.header.always_zero = 0;
dsc.header.w = 428;
dsc.header.h = 142;
dsc.header.cf = LV_IMG_CF_TRUE_COLOR;   // 原生 16bpp，无 alpha
dsc.data = slot->px;
dsc.data_size = 428 * 142 * 2;
```

- 屏幕只有一个铺满的 `lv_img`（0,0,428,142），HUD 文本作为**兄弟节点**叠在上层。
- `lv_img` widget 尺寸必须与源尺寸完全相同：本项目曾出现"widget 大于 src 时 LVGL 平铺重复"的问题（见项目记忆），这里尺寸相等天然规避。
- 换帧流程（**只在 DisplayTask**）：

```cpp
int slot = AiFramePool::instance().takeQueuedSlot();
if (slot >= 0) {
    ui_AiScreen_show_frame(AiFramePool::instance().dsc(slot));
    // 内部仅 lv_img_set_src(img, dsc) + lv_obj_invalidate(img)
    AiFramePool::instance().releaseDisplaySlot(prev_slot); // 旧槽在换源后即可释放
    prev_slot = slot;
}
```

- **不要每帧 `lv_refr_now()`**：它会阻塞 DisplayTask 约 33ms，挤占消息处理。交给 LVGL 刷新定时器（30ms 周期）即可；实测如需更低延迟，再在 AI 页激活期间把刷新周期调到 16ms：

```c
lv_timer_set_period(lv_disp_get_refr_timer(lv_disp_get_default()), 16);
```

### 6.3 为什么不加第四槽 / 为什么不做"直接刷屏"

- 第四槽只增加内存，不提升吞吐（瓶颈在 SPI 刷新 33ms）。
- "绕过 LVGL 直接 `gfx->draw16bitRGBBitmap()`"可省掉约 2~4ms 的 PSRAM→内部缓冲拷贝和 LVGL 调度开销，但需要：
  - 与 `my_disp_flush` 共享 SPI 总线互斥锁；
  - LVGL 任何一次局部重绘（HUD、状态栏）都会覆盖图像区域，必须重新补画；
  - 打破"LVGL 只在 DisplayTask"的约束。
  **列为 P3 可选优化**，在实测帧率不足 10fps 且确认瓶颈在 LVGL 拷贝时再做。

### 6.4 性能预算（估算，需实机验证）

| 路径 | 单帧网络字节 | 传输耗时 | 解码 | 显示（LVGL+SPI） | 预计端到端 fps |
| --- | --- | --- | --- | --- | --- |
| USB + JPEG 428×142（q75，约 12KB） | 12KB | 10~18ms | 20~30ms | 33~38ms | **12~18** |
| WiFi + JPEG（同上） | 12KB | 4~12ms | 20~30ms | 33~38ms | **12~18** |
| WiFi + JPEG 856×284 + 1/2 缩放 | 25~40KB | 10~20ms | 15~25ms | 33~38ms | **12~18**（画质更好） |
| USB + RGB565 原始 | 118.7KB | 110~150ms | 0 | 33~38ms | 6~8 |
| WiFi + RGB565 原始 | 118.7KB | 30~120ms | 0 | 33~38ms | 6~13（需实测带宽） |
| USB + RLE565（3:1） | ~40KB | 40~55ms | 3~6ms | 33~38ms | 10~13 |

> 显示链路是公共上限：9 分片 ×（2.7ms SPI + 1 tick 让出）≈ 33ms，且 LVGL 刷新定时器本身 30ms 触发一次。

## 7. 任务、并发与内存

### 7.1 AiTask

| 项 | 取值 | 说明 |
| --- | --- | --- |
| 核心 / 优先级 | Core1 / 1（与 loopTask 同级） | 与 HID 扫描、协议轮询公平分片；避免 20~30ms 的抢占抖动 |
| 栈 | 6144B | 解码工作区不放栈上，避免重蹈 `KeymapArray` 栈溢出 |
| 触发 | AI 页激活时创建，离开时自删除 | `AiLink` 同时关闭监听 socket |
| 循环 | 取"组装完成帧" → 解码 → commit → 节拍统计 | 无数据时 20ms 超时阻塞 |
| 让出 | TJpgDec 输出回调每 8 个 MCU 行 `vTaskDelay(1)` | 防止 Core1 上的 IDLE 饥饿与 task_wdt |
| 禁止 | 不调用 LVGL、不写 Flash、不阻塞等待网络 | — |

### 7.2 DisplayTask 增补

- 新增消息类型：
  - `DisplayMessageType::AiFrame = 13`：仅携带 `uint8_t ai_slot`（像素不搬进队列）。
  - `DisplayMessageType::AiStatus = 14`：`AiStatusInfo{fps_x10, bytes_per_s, state, dropped}`，由 AiTask 以 2Hz 直接 post，仅刷 HUD 统计。
- `applyMessage()` 增加两个分支：换帧、刷 HUD。
- **自动回主页豁免**：`isAutoReturnEnabled()` 对 `UI_SCREEN_AI` 返回 false（AI 页是"显示终端"，用户不用持续操作）。
- **省电豁免**：`checkPowerIdle()` 在 AI 页且 AI 激活时强制 `Normal`，避免 30s 后关屏；离开页面立即恢复原策略。
- **进入/离开钩子（不能挂 navigateNow）**：一级页旋钮导航不走 [`navigateNow()`](../src/tasks/DisplayTask.cpp#L446-L507)——各 `ui_*Screen.c` 的 `LV_EVENT_KEY` 处理器直接调 `_ui_screen_change()` + `ui_set_active_screen_tag()`（[ui_AudioScreen.c](../src/ui/ui_AudioScreen.c#L47-L52) / [ui_PcStatusScreen.c](../src/ui/ui_PcStatusScreen.c#L75-L79)），navigateNow 只覆盖 Navigate 消息、KEYMAPPED 截胡、自动回主页三条路径。改为在 `DisplayTask::run()` 主循环每轮比对 `ui_get_active_screen_tag()` 与上轮缓存值，变化时调用 `AiMode::instance().setScreenActive(tag == UI_SCREEN_AI)`；只做"启停标志 + 消息通知"，不在此处做耗时的解码/网络工作。

### 7.3 MainTask 增补

- `loop()` 开头插入 `AiMode::instance().pollSerial()`（在 `SerialProtocol::poll()` 之前）。
- `tick()` 中调用 `AiMode::instance().pollNetwork()`（accept/读 TCP，非阻塞）。
- `functionKeyJumpTarget()` 增加字符串 `"KEY_FUNCTION_PAGE_AI"` → `UI_SCREEN_AI`（注意 `activeFunLayer()` 三通道取值，见项目记忆中的 FUN 层教训）。

### 7.4 内存预算

| 项 | 大小 | 位置 | 生命周期 |
| --- | --- | --- | --- |
| 帧槽 ×3 | 3 × 121,552B ≈ 356KB | PSRAM | 首次进入 AI 页分配，常驻 |
| JPEG 组装缓冲 ×2 | 2 × 64KB = 128KB（`kAiMaxPayloadBytes`） | PSRAM | 同上 |
| TJpgDec 输入窗口 + Huffman 表 | 6~8KB | 内部 RAM | AiTask 生命周期 |
| AiTask 栈 | 6KB | 内部 RAM | 同上 |
| lv_img + HUD + dsc | < 3KB | LVGL 池（PSRAM） | UI 常驻 |
| DisplayMessage 增量 | ~24B × 队列 10 | 内部 RAM | 常驻 |

合计新增 PSRAM ≈ **490KB**（8MB 中占比 6%），内部 RAM ≈ **14KB**。相比 ASR TLS 约 35KB 的内部需求，余量可控；仍建议在 AI 页激活时避免同时触发语音识别。

## 8. UI 页面设计（428×142）

### 8.1 布局

```
┌──────────────────────────────────────────────┐
│ ┌──────────────────────────────────────────┐ │  ← 全屏 lv_img（428×142，0,0）
│ │                                          │ │
│ │           （图像内容 / 占位提示）           │ │  HUD 右上角：
│ │                                          │ │  "JPEG 15.2fps 240KB/s"
│ └──────────────────────────────────────────┘ │  半透明黑底 + BebasNeueFont14
└──────────────────────────────────────────────┘
```

**状态机**：

| 状态 | 显示内容 |
| --- | --- |
| `WAITING` | 居中 "AI"（BebasNeueFont48）+ "WAITING FOR FRAME"（BebasNeueFont14）；副行显示 `USB 115200` 或 `192.168.1.23:30001` |
| `STREAMING` | 图像铺满 + 右上角 HUD（fps / 带宽 / 格式） |
| `WIFI_OFF` | 文字 "WIFI OFF · CHECK SETTINGS"（不自动改配置） |
| `ERROR` | "PSRAM ALLOC FAILED" / "FRAME SIZE 640x480 ≠ 428x142" |

- 图标占位不要用 `LV_SYMBOL_IMAGE`：`ui_font_Symbol48` 只包含 8 个精选字形（见 [ui_font_Symbol48.c](../src/ui/ui_font_Symbol48.c#L4)），未收录该符号会静默渲染为空白（项目已踩过同类坑）。用文字 + 圆角描边矩形代替。
- 中英文案随 `ui_lang` 切换，接入 [`ui_lang_set()`](../src/ui/ui_lang.c#L26-L43) 的刷新列表（新增 `ui_AiScreen_apply_language`）。

### 8.2 交互（沿用 README 输入规则）

| 输入 | 行为 |
| --- | --- |
| 旋钮顺时针 | 切到下一一级页（PC_STATUS） |
| 旋钮逆时针 | 切到上一一级页（AUDIO） |
| 旋钮单击 | 切换 HUD 显示/隐藏（不影响接收） |
| 旋钮双击 | 回主页并停止接收 |
| 矩阵键 1~11 | 不拦截，继续走 HID（与"矩阵键为 HID 专用"的既有规则一致） |

### 8.3 导航链接入位置

建议插在 **AUDIO 与 PC_STATUS 之间**，改动最小且符合现有环形链：

| 文件 | 改动 |
| --- | --- |
| [ui_AudioScreen.c](../src/ui/ui_AudioScreen.c#L47-L52) | `LV_KEY_RIGHT` 目标：PC_STATUS → **AI** |
| [ui_PcStatusScreen.c](../src/ui/ui_PcStatusScreen.c#L69-L80) | `LV_KEY_LEFT` 目标：AUDIO → **AI** |
| `ui_AiScreen.c`（新） | LEFT → AUDIO，RIGHT → PC_STATUS，ESC → MAIN |
| [ui_types.h](../src/ui/ui_types.h#L54-L75) | 枚举**追加** `UI_SCREEN_AI`（不要插入中间，避免改变既有枚举值；tag 仅用等值比较，顺序无意义） |
| [ui.h](../src/ui/ui.h) / [ui.c](../src/ui/ui.c) | 包含、`screen_init`、`screen_destroy` |
| [DisplayTask.cpp](../src/tasks/DisplayTask.cpp#L446-L507) | `navigateNow` 增加 `UI_SCREEN_AI` 分支；激活钩子见 §7.2（run() 主循环 tag 变化比对，不挂 navigateNow） |
| [MainTask.cpp](../src/tasks/MainTask.cpp#L384-L421) | `functionKeyJumpTarget()` 增加字符串 `"KEY_FUNCTION_PAGE_AI"` → `UI_SCREEN_AI`（注意其它 `PAGE_*` 都跳二级页，AI 无二级页，直接跳一级页） |
| [keymap_types.h](../src/utils/keymap_types.h#L71-L77) | 功能串注释补充 `PAGE_AI` 与"跳一级页"说明 |
| EKeysApp `protocol.rs` / `panel_keymap.rs` | 功能下拉新增 "页面跳转: AI 模式"（固件串需逐字节一致） |

> 追加枚举不会影响既有页面判断（全部是等值比较）；但 `DisplayTask` 里若有"是否为二级页/是否自动回主页"的白名单判断，必须同步加入 `UI_SCREEN_AI`，否则会出现"进得去、5s 后被弹回主页"。

## 9. 代码结构与改动清单

### 9.1 新增文件

| 文件 | 职责 | 关键接口 |
| --- | --- | --- |
| `src/ai/AiConfig.h` | 常量集中配置（端口、槽数、分片大小、超时、负载上限） | `kAiTcpPort=30001`、`kAiSlotCount=3`、`kAiAssemblerCount=2`、`kAiMaxPayloadBytes=65536`（JPEG/RLE）、`kAiMaxRawPayloadBytes=131072`（RGB565 直写） |
| `src/ai/AiFrameProtocol.h/.cpp` | 帧头/分片解析、CRC16/CRC32、状态机、ACK 组装 | `pushBytes(const uint8_t*, size_t)`、`poll()` |
| `src/ai/AiFramePool.h/.cpp` | 3 槽 FramePool + `lv_img_dsc_t` 管理 | 见 §6.1 |
| `src/ai/AiImageDecoder.h/.cpp` | JPEG（TJpgDec）/RGB565/RLE565 → 帧槽 | `decode(type, src, len, dst, w, h, flags)` |
| `src/ai/AiSerialLink.h/.cpp` | USB CDC 二进制分流 + 非二进制字节回喂 | `pollSerial()`、`enterBinaryMode()` |
| `src/ai/AiNetLink.h/.cpp` | WiFiServer 30001 生命周期、accept/read | `begin(port)`、`poll()`、`end()` |
| `src/ai/AiMode.h/.cpp` | 门面：页面激活、任务生命周期、统计、状态投递 | `setScreenActive(bool)`、`pollSerial()`、`pollNetwork()` |
| `src/ai/AiTask.h/.cpp`（可并入 AiMode.cpp） | 解码任务 | `taskEntry()` |
| `src/ui/ui_AiScreen.h/.c` | AI 页面 | `show_frame(dsc)`、`set_status()`、`apply_language()` |
| `lib/tjpgd/`（vendored） | TJpgDec（`JD_FORMAT=1`, `JD_FASTDECODE=2`，剥离 `#if LV_USE_SJPG` 守卫，见 §5.1） | `jd_prepare` / `jd_decomp` |

### 9.2 修改文件

| 文件 | 改动 |
| --- | --- |
| [`message_types.h`](../src/message_types.h#L32-L45) | 新增 `AiFrame=13`、`AiStatus=14` 与 `AiStatusInfo`；`DisplayMessage` 增加 `uint8_t ai_slot` 与 `ai_status` |
| [`DisplayTask.h`](../src/tasks/DisplayTask.h) / `.cpp` | 两个消息分支、AI 页路由、自动回主页/省电豁免、激活钩子（run() 主循环 tag 变化比对，见 §7.2） |
| [`MainTask.cpp`](../src/tasks/MainTask.cpp) | `pollSerial` / `pollNetwork` 泵、`KEY_FUNCTION_PAGE_AI` |
| [`SerialProtocol.cpp`](../src/protocol/SerialProtocol.cpp#L36-L73) | 抽出 `feedByte(char)` 供分流器回喂；新增 `isLineBufferEmpty()`；`poll()` 内部改为调用 `feedByte`，并支持 AI 二进制态门控（见 §4.4 规则 5） |
| [`ui_types.h`](../src/ui/ui_types.h) / `ui.h` / `ui.c` | 新页面枚举与生命周期 |
| `ui_AudioScreen.c` / `ui_PcStatusScreen.c` | 导航链插入 AI 页 |
| [`ui_lang.c`](../src/ui/ui_lang.c) | 刷新列表加入 `ui_AiScreen_apply_language` |

### 9.3 建议的公开接口

```cpp
namespace ekeys {

class AiMode {
public:
    static AiMode &instance();

    void setScreenActive(bool active);  // DisplayTask 调用（唯一 UI 侧入口）
    void pollSerial();                  // MainTask::loop() 每轮调用，非阻塞
    void pollNetwork();                 // MainTask::tick() 每轮调用，非阻塞

    bool isActive() const;
    const AiStatusInfo &status() const;

    /* 供解码任务/协议层回调（内部使用） */
    void onFrameAssembled(uint8_t type, uint32_t seq, const uint8_t *payload,
                          uint32_t len, uint16_t w, uint16_t h, uint8_t flags);
};

} // namespace ekeys
```

## 10. 配置项

原则：**不新增持久化字段**（AI 模式是即用型功能，端口与缓冲策略固化成常量即可），避免再引出"save/load 必须成对"的历史坑。

`src/ai/AiConfig.h`：

```cpp
inline constexpr uint16_t kAiTcpPort        = 30001;
inline constexpr uint16_t kAiChunkUsb       = 4096;
inline constexpr uint16_t kAiChunkTcp       = 8192;
inline constexpr uint32_t kAiMaxPayloadBytes = 64 * 1024;      /* JPEG/RLE565，经组装缓冲 */
inline constexpr uint32_t kAiMaxRawPayloadBytes = 128 * 1024;  /* RGB565 直写帧槽，121,552B 在限内 */
inline constexpr uint8_t  kAiSlotCount      = 3;
inline constexpr uint32_t kAiHalfFrameTimeoutMs = 300;
inline constexpr uint32_t kAiSerialBudgetPerTick = 8192;
inline constexpr uint32_t kAiDecodeYieldMcus = 8;
```

- 若后续确实需要改端口，再按"`DeviceSettings` 新增字段 → `sectionOfKey/serialize` 与 `loadGlobalSettings_locked` 同步"的成对规则加 `ai_port`。
- WiFi 鉴权如需开启，建议放 `ai_token[17]`，同样遵循成对规则。

## 11. 分级实施计划

| 阶段 | 内容 | 验收 |
| --- | --- | --- |
| **P0** 页面与渲染通路 | `UI_SCREEN_AI` + `ui_AiScreen` + FramePool + 内置"渐变测试图"（不进网络）；导航链与语言接入 | 旋钮能从 AUDIO/PC_STATUS 进出 AI 页；测试图铺满无平铺/花屏；5s 不回主页、30s 不关屏 |
| **P1** USB + JPEG | `AiSerialLink` 分流、帧协议、TJpgDec、AiTask、HUD 统计 | 主机推 428×142 JPEG，连续 10 分钟稳定 ≥10fps；JSON 配置命令在 AI 页帧间隙可用；按键无丢键 |
| **P2** WiFi + 流控 | `AiNetLink`（TCP 30001）、HELLO/ACK、丢帧策略 | 手机/PC 直连推流 ≥12fps；断线重连自动恢复；`busy` 时不堆积延迟 |
| **P3** 可选优化 | RLE565、856×284 半尺寸解码、直刷屏绕过 LVGL、SoftAP、token 鉴权 | 按实测瓶颈择优；每项独立开关，默认关闭 |

## 12. 风险与规避

| 风险 | 影响 | 规避 |
| --- | --- | --- |
| PSRAM 带宽竞争（解码写 + LVGL 读 + WiFi 缓冲） | 帧率下降、抖动 | 解码与刷新分核；JPEG 优先；避免同时开 ASR |
| 内部 RAM 紧张（TJpgDec 工作区 + 任务栈） | ASR/HTTP TLS 分配失败 | 工作区控制在 8KB 内；AI 页激活时若正在 ASR 则延后启动 |
| 全屏刷新 33ms 上限 | 帧率上不去 | 已按 JPEG 12KB 设计；必要时提高 SPI 到 80MHz（需面板验证）或加大 LVGL 缓冲行数 |
| USB 分流误吞 JSON | 配置命令失效 | 严格的 `\n + A5 5A` 前缀 + 行缓冲状态判定（`isLineBufferEmpty`，'\n' 非空缓冲必须透传）+ 二进制态门控 `SerialProtocol::poll()` + 300ms 超时复位；P1 必须回归 0x07/0x06 命令（帧间隙） |
| HID 延迟 | 按键抖动 | AiTask 与 loopTask 同优先级并分段让出；串口每 tick 限 8KB；不在 MainTask 里做解码 |
| 撕裂/花屏 | 视觉异常 | 三槽状态机（写槽永不等于显示槽）；只允许 DisplayTask 改 `lv_img` src |
| task_wdt | 复位 | 解码回调周期让出；DisplayTask 已有每轮 `vTaskDelay(1)`（[DisplayTask.cpp](../src/tasks/DisplayTask.cpp#L255-L261)） |
| WiFi 与 BLE 共存 | 吞吐下降 | AI 走 WiFi 时建议 `work_mode=USB`（现有 `isEnabled()` 规则）；BLE 模式下页面显示 WIFI OFF |
| 图像写入 Flash | 寿命/卡顿 | 明确禁止：帧只在 PSRAM 流转 |
| 明文传输被同网段滥用 | 数据泄露/干扰 | 仅监听 LAN、单客户端、只读显示；P3 可加 token |

## 13. 验收标准

1. 旋钮可在 AI 页与相邻一级页之间往返；双击立即停止接收并回到主页。
2. USB 推送 428×142 JPEG 连续 10 分钟：无花屏、无撕裂、无复位；平均 ≥10fps，HUD 统计与实际相符。
3. WiFi 推送同规格：≥12fps；拔网/重连后 5s 内自动恢复。
4. 尺寸/CRC 错误的帧被丢弃并在 HUD 显示原因，不影响后续正常帧。
5. AI 页激活且无帧在途（帧间隙）时，JSON 配置命令（0x07/0x06）仍能正常应答（二进制态内到达的 JSON 会被丢弃，属预期行为，见 §4.4 规则 6）；矩阵键 HID 输出无丢键。
6. 退出 AI 页后：监听端口关闭、AiTask 退出、TCP 客户端断开；帧槽与组装缓冲常驻不释放，PSRAM 占用保持稳定（日志打印 `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` 确认不再持续下降）。
7. 语言切换后 AI 页文案同步中文/English。

## 附录 A — 发送端参考实现（Python）

```python
import io, socket, struct, zlib, time
from PIL import Image

MAGIC, VERSION = 0x5AA5, 1
W, H = 428, 142
CHUNK = 8192

def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

def send_frame(sock, jpeg: bytes, fid: int):
    payload = jpeg
    header = struct.pack('<HBBHHHBBIIHH',
        MAGIC, VERSION, 1, fid, W, H, 0, 0,
        len(payload), zlib.crc32(payload) & 0xFFFFFFFF, CHUNK, 0)
    header = header[:22] + struct.pack('<H', crc16_ccitt(header[:22]))
    sock.sendall(b'\n' + header)
    for idx in range(0, len(payload), CHUNK):
        part = payload[idx:idx + CHUNK]
        ch = struct.pack('<HHHH', fid, idx // CHUNK, len(part), crc16_ccitt(part))
        sock.sendall(ch + part)

img = Image.open('frame.png').convert('RGB').resize((W, H))
b = io.BytesIO()
img.save(b, 'JPEG', quality=75)
jpeg = b.getvalue()

s = socket.create_connection(('192.168.1.23', 30001))
for fid in range(1000):
    send_frame(s, jpeg, fid)
    time.sleep(1/15)
```

> USB 场景把同一字节流写到 CDC 串口即可（先发 `\n`）；发送端应按行读取设备回传的 `ai_ctl` JSON 行（ACK/HELLO/统计，§4.2）做 2 帧在途限流，非 JSON 的日志行跳过。

## 附录 B — 待实机验证项

1. 实际 SPI 全屏刷新耗时（`my_monitor_cb` 的 `refr cost / px` 日志，[LvglPort.cpp](../src/display/LvglPort.cpp#L111-L119)）。
2. TJpgDec（`JD_FASTDECODE=2`）单帧解码耗时：建议临时统计 `jd_decomp` 前后 `millis()`。
3. USB CDC 实测吞吐：连续发 12KB JPEG 统计 ACK 频率。
4. PSRAM 三槽 + 64KB 组装缓冲下的剩余堆（`heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`）。
5. AI 页激活 30 分钟后的 LVGL 池水位（现有 30s 水位日志）。
6. 40MHz 下全屏 15fps 连续刷新是否出现面板 TE 撕裂（必要时再评估 80MHz 或双缓冲分片策略）。
