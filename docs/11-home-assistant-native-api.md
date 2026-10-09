# EKeys 接入 Home Assistant（ESPHome Native API）实施方案

> 状态：已实施。2026-10-09 联调修复两处协议编码错误（帧 size 语义、
> api_version wire type，见 §2 末尾修复记录），修复后 HA 可正常添加。

## 1. 总体设计

### 1.1 角色与生命周期

```
Home Assistant (TCP 客户端)
        │  连接 6053 端口（mDNS 自动发现 _esphomelib._tcp）
        ▼
EKeys = TCP 服务端（HaApiService，MainTask 上下文轮询驱动）
        │  11 个矩阵键 → 11 个 binary_sensor 实体
        │  press   → BinarySensorStateResponse{state=true}
        │  release → BinarySensorStateResponse{state=false}
        ▼
HA 侧：设备 "EKeys" + 11 个按键实体 → 自动化触发
```

三条生命周期独立管理（关键设计决策）：

| 层面            | 生命周期                                                                                                                                | 理由                                                       |
| --------------- | --------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| mDNS 声明       | WiFi 已连接即声明一次                                                                                                                   | 仅广播"设备存在"，不建立通信；避免随屏幕切换反复 begin/end |
| TCP 监听 + 连接 | **仅在活动屏为 HA 屏（`UI_SCREEN_HA` / `UI_SCREEN_HA_SECONDARY`）且 WiFi 已连时**监听 6053 并接受 HA 连接；离开 HA 屏立即断开并停止监听 | 需求：只在 HA 页面中才连接 HA 进行通信                     |
| 按键路由        | **活动屏为 HA 屏时**，矩阵键进 HA、不发 HID（未连上 HA 时丢弃）                                                                         | 与"屏幕决定路由"的现有模式一致                             |

- **屏幕边沿检测**：MainTask 5ms tick 里用缓存的上一拍屏幕 tag 检测
  进入/离开 HA 屏的边沿，触发 `setListening(true/false)`；屏幕 tag 为
  DisplayTask 单写 / MainTask 单读的 uint8_t（既有约定，无锁安全）。
- **重连延迟预期**：进入 HA 屏后 HA 客户端按自身重试周期回连（秒级到
  几十秒不等），期间实体在 HA 侧显示 unavailable、按键事件不上报——
  这是"仅页面内连接"模型的固有行为，联调时以此解释现象。
- 不新增持久化配置项：服务开关隐含于 WiFi + 当前屏幕。
- 单客户端假设：同一时间只服务一个 HA 连接；新连接到来时先关旧连接再接受。

### 1.2 协议版本选择：明文协议 v1 子集

已对照 ESPHome 官方协议文档（见文末 Sources）确认：

- **帧格式**（明文协议，当前文档与历史版本一致）：

  ```
  0x00 指示字节 + varint(载荷长度) + varint(消息类型) + protobuf 载荷
  ```

- **握手时序**：
  1. HA 连上 → 发 `HelloRequest`
  2. 设备回 `HelloResponse`（声明 api_version）
  3. HA 可能发 `ConnectRequest`（密码）→ 设备回 `ConnectResponse`
  4. `DeviceInfoRequest` → `DeviceInfoResponse`
  5. HA 发 `ListEntitiesRequest` → 设备逐个发实体描述 + `ListEntitiesDoneResponse`
  6. HA 发 `SubscribeStatesRequest`
  7. 之后设备可随时推状态；`PingRequest`/`PingResponse` 保活

- **不实现 Noise 加密**（明文 HA 仍支持，接入时 HA 会提示"未加密"警告，
  可接受；加密列为远期增强）。

兼容性策略：**对外声明 api_version 1.7**，走 HA 对老固件的向后兼容路径
（HA 至今仍支持连接多年前刷的 ESPHome 设备）。设备端无论 HA 发不发
`ConnectRequest` 都能工作（收到即校验，未配置密码直接通过）。

## 2. 协议消息子集清单

实现时以官方 `api.proto` 逐字段核对，下表为本方案最小集合：

| 消息                             | 类型号 | 方向    | 用到的 protobuf 字段                                                                                                                                                           |
| -------------------------------- | ------ | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| HelloRequest                     | 1      | HA→设备 | `client_info=1`（string，忽略内容）                                                                                                                                            |
| HelloResponse                    | 2      | 设备→HA | `api_version_major=1`、`api_version_minor=2`（uint32 varint，固定 1/7）、`server_info=3`（"ekeys native api"）、`name=4`（"ekeys"）                                                  |
| ConnectRequest                   | 3      | HA→设备 | `password=1`（string；未配置密码时任意值通过）                                                                                                                                 |
| ConnectResponse                  | 4      | 设备→HA | `invalid_password=1`（false）                                                                                                                                                  |
| DisconnectRequest                | 5      | 双向    | 空                                                                                                                                                                             |
| DisconnectResponse               | 6      | 双向    | 空（回完后关连接）                                                                                                                                                             |
| PingRequest                      | 7      | HA→设备 | 空                                                                                                                                                                             |
| PingResponse                     | 8      | 设备→HA | 空                                                                                                                                                                             |
| DeviceInfoRequest                | 9      | HA→设备 | 空                                                                                                                                                                             |
| DeviceInfoResponse               | 10     | 设备→HA | `uses_password=1`(false)、`name=2`("ekeys")、`mac_address=3`、`esphome_version=4`("1.7.0-ekeys")、`compilation_time=5`、`model=6`("EKeys ESP32-S3")、`has_deep_sleep=7`(false) |
| ListEntitiesRequest              | 11     | HA→设备 | 空                                                                                                                                                                             |
| ListEntitiesBinarySensorResponse | 12     | 设备→HA | `object_id=1`("key_1"~"key_11")、`key=2`(fixed32，1~11)、`name=3`("Key 1"~"Key 11")（原 `unique_id=4` 自 ESPHome 2025.10 起为 reserved，不再发送）                             |
| ListEntitiesDoneResponse         | 19     | 设备→HA | 空                                                                                                                                                                             |
| SubscribeStatesRequest           | 20     | HA→设备 | 空（置"已订阅"标志）                                                                                                                                                           |
| BinarySensorStateResponse        | 21     | 设备→HA | `key=1`(fixed32)、`state=2`(bool)、`missing_state=3`(false)                                                                                                                    |

- protobuf 编解码手写（proto3：字段号 + wire type；string 用
  `0x0A + varint长度 + 字节`，bool/int 用 varint，fixed32 用
  `0x0D + 4字节LE`），**不引入 nanopb**。实体的 key 直接用 key_id 1~11。
- 实体命名注意：`object_id` 固定为 `key_N`，**不随当前键映射内容变**
  （映射可变，实体身份必须稳定；HA 侧显示名可在 HA 里改）。

#### 帧格式与联调修复记录（2026-10-09）

对照官方源码（`api_frame_helper_plaintext.cpp` / aioesphomeapi
`_frame_helper/plain_text.py`）核实的明文帧语义：

```
0x00 | varint(载荷长度) | varint(消息类型) | protobuf 载荷
```

- **载荷长度只含 protobuf 载荷字节数，不含类型 varint**（与 Noise 协议
  的"含头长度"不同）。修复前误把类型 varint 计入长度，HA 按 size 读载荷
  永远差 1 字节，hello 等满 30s 超时 → 配置流程报"无法连接到 ESPHome
  设备，请确保 YAML 包含 api 部分"。
- **`api_version_major/minor` 是 uint32（varint）**，非 fixed32；fixed32
  编码会被 HA 的 protobuf 解析器当未知字段跳过，版本号丢失。
- 载荷长度 0 合法（PingRequest/DisconnectRequest 为空载荷帧），解析端
  不得把 0 长度当坏帧丢弃。

mDNS 声明（ESPmDNS，Arduino 核心自带）：

- 服务类型 `_esphomelib`，协议 `_tcp`，端口 6053
- 实例名 "ekeys"，TXT 记录：`version=1.7`、`platform=ESP32`
  （HA 的 zeroconf 发现按 `_esphomelib._tcp.` 匹配）

## 3. 文件拆分与模块设计

新增 `src/ha/` 目录，与 `src/network/` 平级：

```
src/ha/
├── HaApiProto.h/.cpp    纯编解码层：帧封装/解析、varint、各消息字段常量与编/解码
│                        （无状态、无 IO，可独立单测）
├── HaEntities.h         11 键实体描述常量（object_id/name/unique_id/key 表）
└── HaApiService.h/.cpp  服务层：WiFiServer 监听 6053（仅 HA 屏激活时）
                         + 单客户端连接状态机 + mDNS 声明
                         + sendKeyState(key_id, pressed) 对外接口
```

### 3.1 HaApiService 接口（与现有模块同风格：单例 + process 轮询）

```cpp
class HaApiService {
public:
    static HaApiService &instance();
    void start();            // WiFiManager on_connected 回调：MDNS.begin + addService（仅声明，不监听）
    void stop();             // WiFi 断开时：关监听/客户端 + MDNS.end 善后
    void setListening(bool); // HA 屏进入/离开边沿：listen 6053 / 断开连接并停监听
    void process();          // MainTask::tick() 驱动：accept / 逐帧解析 / 超时检测
    void sendKeyState(uint8_t key_id, bool pressed);  // MainTask 按键路由调用
    bool isClientConnected() const;                   // HA 屏状态显示
};
```

- 连接状态机（内部）：`Idle → Listening → Hello → Ready`（收到
  ListEntities + SubscribeStates 后即 Ready）。就绪前收到的状态类消息
  直接忽略。
- `setListening(false)`（离开 HA 屏）：主动关客户端连接 + `server.close()`，
  回到 Idle；下次进入 HA 屏重新 `listen()`。
- 保活超时（约 120s 无任何帧）主动断开，等 HA 重连。
- 断开时所有键视为 released（HA 侧 binary_sensor 自动转 unavailable，
  无需补偿）。

### 3.2 发送路径约束（性能红线）

`sendKeyState` 在 MainTask 上下文调用，**全程非阻塞**：

- 编码到固定缓冲区（帧头 ≤5 字节 + 载荷 <20 字节，栈对象无风险）后
  `client.write()` 一次发出；写失败即认为连接异常，标记断开下轮清理——
  不重试不缓冲，按键是边沿事件，丢了 HA 会重连恢复。
- `process()` 里 `accept()` 与 `read` 全部用 `available()`/`connected()`
  轮询，禁止 `readBytes` 类阻塞调用，**保护 RGB 30ms 帧**。

## 4. 现有代码改动点

### 4.1 MainTask（`src/tasks/MainTask.cpp`）

1. **begin()**（约 L305-L318）：`WiFiManager::setOnConnected` 回调里追加
   `HaApiService::instance().start()`（仅 mDNS 声明，不监听）；`tick()`
   （约 L960-L970）追加：
   - **屏幕边沿 → 监听开关**：缓存上一拍 `ui_get_active_screen_tag()`，
     进入 HA 屏边沿且 WiFi 已连 → `setListening(true)`；离开 HA 屏边沿
     → `setListening(false)`（进入时 WiFi 未连则只记边沿，不监听）
   - `HaApiService::instance().process()`，并在其前检查 WiFi 已断则
     `stop()`
2. **5ms tick 按键路由**（约 L587-L658）：
   - `suppress_hid` 扩展：`screen == UI_SCREEN_HA || screen == UI_SCREEN_HA_SECONDARY`
   - press 循环内：HA 屏激活时，跳过 `KeyEventDispatcher::onKeyEdge` /
     功能串跳转 / `resolver_.press`，改调
     `HaApiService::instance().sendKeyState(pressed[i], true)`
     （连 HA 与否都这样走，未连接时丢弃——与"其它屏矩阵键丢弃"规则一致）
   - release 循环：HA 屏激活时先 `sendKeyState(released[i], false)` 上报
     熄灭（2026-10-09 修复：原实现 release 不上报 HA，按过的键在 HA 中
     永远卡 on）；HID release 派发保持原样——被屏蔽 press 对应的 release
     派发无副作用，离开 HA 屏后 release 正常派发避免卡键
3. **恢复 PAGE_HA 功能串**（约 L382-L387）：取消注释。
4. **HA 状态快照**（约 L981-L1000）：`HaStatusInfo` 新增
   `ha_api_connected` 字段（`src/message_types.h`），赋值
   `HaApiService::instance().isClientConnected()`。

### 4.2 UI / 导航链恢复

- `src/ui/ui_PcStatusScreen.c` / `src/ui/ui_SettingScreen.c`：取消
  "暂时移除 HA 入口"的跳转恢复（PcStatus RIGHT→HA、Setting LEFT→HA）。
  `ui_HaScreen.c` 的三向导航（RIGHT→Setting、LEFT→PcStatus、ENTER→二级页）
  已是完整的，无需改。
- `src/ui/ui_HaScreenSecondary.c`：新增
  `ui_HaScreenSecondary_set_ha_status(bool connected)`（显示
  "HA: CONNECTED / ---"），DisplayTask `applyHaStatus` 透传新字段；
  头文件同步声明（规则：新增方法不忘声明）。
- HA 屏为矩阵键"HA 模式"的物理入口：建议在一级屏或二级屏用旋钮可感知
  的方式提示"按键已接入 HA"（文案层面，实施时定）。

### 4.3 App 端（EKeysApp，跨端同步必改）

- `protocol.rs` `function_label` 恢复/新增 `KEY_FUNCTION_PAGE_HA` 友好名
  （"页面跳转: HA"），`PAGE_JUMP_TARGETS` 同步——**两端字符串必须逐字节
  一致**（既有约定）。

### 4.4 平台配置

- `platformio.ini`：无需新依赖（ESPmDNS 在 arduino-esp32 核心内）。
  确认 build_flags 无冲突即可。

## 5. 实施顺序（每步可独立验证）

1. **HaApiProto**：varint + 帧封装/解析 + 14 个消息的编解码，对照
   `api.proto` 字段号核对一遍。
2. **HaApiService**：TCP 监听/连接状态机/process 轮询，接入 MainTask
   tick（此步可先用 PC 上的 Python `aioesphomeapi` 客户端连 6053 端口
   验证握手 + 实体列表，不依赖 HA）。
3. **按键路由**：MainTask suppress_hid 扩展 + HA 分支。
4. **UI + 导航链恢复 + App 端选项**：PAGE_HA、HA 屏导航、连接状态显示。
5. **联调**（见 §7）。

## 6. 风险与注意事项

| 风险                                                    | 处置                                                                                                                                                                           |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| MainTask 单线程轮询被 TCP 拖慢 → RGB 帧延迟（既有教训） | 全程非阻塞；按键发送失败直接丢弃不重试；`process()` 每轮最多解析 2 帧，剩余下轮                                                                                                |
| HA 未来移除 v1 明文兼容                                 | 2026.1 仅移除了新固件的 authenticate RPC，老固件路径仍保留；若日后真被移除，升级 Noise PSK 加密（协议结构已预留，只在传输层换帧）                                              |
| `api_version`/字段号偏差                                | 实现时以官方 api.proto 为准核对；api_version 固定报 1.7                                                                                                                        |
| mDNS 与 WiFi 重连                                       | WiFi 断开时 `stop()` 里做 `MDNS.end()`，重连 `start()` 里重新 `begin()+addService`，避免重复注册；**屏幕切换只开关 TCP 监听，不触碰 mDNS**；DiscoveryService（UDP 广播）无冲突 |
| 仅页面内连接的断链表现                                  | 离开 HA 屏即断链：HA 侧实体转 unavailable、历史事件不丢（state machine 照常记录）；重进 HA 屏后 HA 按重试周期回连（见 §1.1），非故障                                           |
| 频繁 listen/close                                       | 进出 HA 屏均可能开关监听：`setListening` 两条路径都要完整清理（client.stop + server.close），防止 socket/内存泄漏；状态机统一回 Idle                                           |
| HA 屏与 FUN 组合键语义冲突                              | HA 屏内按键全部截胡进 HA，FUN 预扫描跳过（同设置二级页先例）；功能串跳转在 HA 屏内不触发（已在二级页语境）                                                                     |
| 明文无密码                                              | 局域网内任何人可连 6053 触发按键事件；可后续加 `ha_api_password` 配置项（ConnectRequest 校验位已留好）                                                                         |
| 大栈对象                                                | 本方案无 >1KB 栈对象；帧缓冲固定 <32B，规则自查无需 static                                                                                                                     |

## 7. 联调步骤

1. PC 先行验证：`pip install aioesphomeapi` 写 10 行脚本连 `<设备IP>:6053`，
   确认 hello/connect/device_info/list_entities 全链路。
2. HA 添加：设置 → 设备与服务 → 添加集成 → **ESPHome** → 输入设备 IP
   （或等 mDNS 自动发现）→ 密码留空 → 出现 "EKeys" 设备与 11 个
   binary_sensor。**注意：添加集成时设备必须停留在 HA 屏**
   （否则 6053 不监听，HA 连不上）。
3. 按键验证：切到 HA 屏（等 HA 回连、实体恢复 available 后），逐键按住/
   松开，HA 开发者工具看实体状态翻转。
4. 自动化示例：

```yaml
automation:
  - alias: "EKeys Key1 press"
    triggers:
      - trigger: state
        entity_id: binary_sensor.ekeys_key_1
        to: "on"
    actions:
      - action: light.toggle
        target: { entity_id: light.living_room }
```

5. 回归检查：离开 HA 屏后 HA 连接断开（实体转 unavailable，属预期）、
   按键恢复 HID 正常（含截胡期间按住的键释放不卡键）；FUN 组合键、
   USB/BLE 模式不受影响。

## 8. 已定取舍（评审结论）

- **实体范围：仅 11 个按键（binary_sensor）**，电池/RSSI/方案名等扩展
  实体及双向控制（light/media_player 等）暂不实现，留作后续路线。
- 实体类型选 **binary_sensor**（press/release 两态，兼容性最稳、类型号
  12/21 确定），而非 2024.6+ 的 event 实体（更"正统"但类型号需再核对）。
- **仅 HA 屏激活时监听 6053 并接受连接**（mDNS 声明仍随 WiFi 常开）；
  离开 HA 屏即断链，HA 侧显示 unavailable 属预期行为。

## Sources

- [ESPHome API Protocol Details](https://developers.esphome.io/architecture/api/protocol_details/)
- [Native API Component](https://www.esphome.io/components/api/)
- [esphome-client api.proto (hjdhjd)](http://raw.githubusercontent.com/hjdhjd/esphome-client/HEAD/src/api.proto)
