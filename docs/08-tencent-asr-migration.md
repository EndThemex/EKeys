# 阶段 08 — 语音识别迁移：百度 ASR → 腾讯云一句话识别

> 状态：方案稿（待实施）
> 目标：将 `src/voice/` 后端从百度短语音 REST 替换为腾讯云 [一句话识别](https://cloud.tencent.com/document/product/1093/35646)
> 关联章节：[`FEATURE_DOC.md §11`](../FEATURE_DOC.md)、[`docs/06-network-voice-rgb-audio.md`](06-network-voice-rgb-audio.md)
> 关联目录：[`src/voice/`](../src/voice/)、[`src/config/DeviceSettings.h`](../src/config/DeviceSettings.h)、[`src/protocol/commands/cmd_config.cpp`](../src/protocol/commands/cmd_config.cpp)

## 1. 背景与动机

当前 `VoiceRecognizer` 走百度短语音 REST，依赖 `AsrTokenCache` 缓存 `access_token`，凭证字段为 `voice_baidu_api_key` / `voice_baidu_secret_key` / `voice_dev_pid`（数字 ID）。

替换为腾讯云一句话识别 (`SentenceRecognition`) 后：

- **协议**：`POST https://asr.tencentcloudapi.com/`，JSON 体 + TC3-HMAC-SHA256 头签名
- **凭证**：`SecretId` + `SecretKey`（无 OAuth token；每次请求重新签名）
- **音频**：`SourceType=1` + base64 编码 PCM（≤60s / ≤3MB，本机 16k/16bit/mono 完全够）
- **引擎**：固定 `16k_zh`（中文普通话，FEATURE_DOC §11.3 不要求多语言）
- **删除**：百度凭证字段、token 缓存、dev_pid 数字映射

## 2. 决策摘要（与用户确认）

| 维度     | 决策                                                                         |
| -------- | ---------------------------------------------------------------------------- |
| 凭证方案 | SecretId/SecretKey 写入 DeviceSettings；ESP32 侧直接 HMAC-SHA256 签名        |
| 音频提交 | `SourceType=1` 直 POST 二进制 PCM（base64 内嵌 JSON body）                   |
| 引擎模型 | 固定 `16k_zh`                                                                |
| 范围     | 完整：替换 token 缓存 → 新签名器 → 改后台识别 → 配置/协议/HA 屏适配          |
| UI 屏    | 设置二级页**不新增 SecretId/Key 输入项**，仅走桌面 App `CMD_CONFIG_SET` 写入 |

## 3. 协议差异要点（百度 vs 腾讯）

| 项     | 百度                                             | 腾讯云                                                                                                       |
| ------ | ------------------------------------------------ | ------------------------------------------------------------------------------------------------------------ |
| 端点   | `vop.baidu.com/server_api?dev_pid=&cuid=&token=` | `asr.tencentcloudapi.com`（POST 体 JSON）                                                                    |
| 鉴权   | URL 拼 `token`（client_credentials）             | Header `Authorization: TC3-HMAC-SHA256 ...`                                                                  |
| 请求体 | raw 二进制 PCM                                   | JSON：`{"EngSerViceType":"16k_zh","SourceType":1,"VoiceFormat":"wav","Data":"<base64>","DataLen":N}`         |
| 响应   | `{"err_no":0,"result":["..."]}`                  | `{"Response":{"Result":"...","RequestId":"..."}}` 或 `{"Response":{"Error":{"Code":"...","Message":"..."}}}` |
| 错误码 | `err_no != 0`                                    | `Response.Error.Code` 存在即失败                                                                             |

## 4. TC3-HMAC-SHA256 签名（一次性写入 ESP32 侧）

> 详细规范参考 [腾讯云签名方法 v3](https://cloud.tencent.com/document/api/1093/35640)。
> 固件侧只需实现"步骤 1（拼 CanonicalRequest）+ 步骤 2（拼 StringToSign）+ 步骤 3（HMAC 链）"三步。

### 4.1 请求头

| Header           | 取值                                                                                                                |
| ---------------- | ------------------------------------------------------------------------------------------------------------------- |
| `Host`           | `asr.tencentcloudapi.com`                                                                                           |
| `Content-Type`   | `application/json; charset=utf-8`                                                                                   |
| `X-TC-Action`    | `SentenceRecognition`                                                                                               |
| `X-TC-Timestamp` | 请求时间（秒，10 位 unix）                                                                                          |
| `X-TC-Version`   | `2019-06-14`                                                                                                        |
| `X-TC-Region`    | `ap-shanghai`（ASR 控制台默认地域）                                                                                 |
| `Authorization`  | `TC3-HMAC-SHA256 Credential=SecretId/date/region/asr/tc3_request, SignedHeaders=content-type;host, Signature=<hex>` |

### 4.2 签名链（5 段）

```
Step1  CanonicalRequest =
       POST\n
       /\n
       content-type:application/json; charset=utf-8\n
       host:asr.tencentcloudapi.com\n
       \n
       content-type;host\n
       <Payload(SHA256)>          // Payload = 序列化后的 JSON 体（小写 hex）

Step2  StringToSign =
       TC3-HMAC-SHA256\n
       <Timestamp>\n
       <YYYY-MM-DD>/asr/tc3_request\n
       <CanonicalRequest SHA256 hex>

Step3  SecretDate  = HMAC-SHA256("TC3" + SecretKey, Date)
       SecretService = HMAC-SHA256(SecretDate, "asr")
       SecretSigning = HMAC-SHA256(SecretService, "tc3_request")
       Signature     = HMAC-SHA256(SecretSigning, StringToSign)
```

### 4.3 资源用量

- `mbedtls/md.h` HMAC-SHA256 4 次（链）+ SHA256 2 次（payload / canonical 摘要）
- 栈：每段摘要 32 字节，临时缓冲 `~256B`（canonical + stringToSign）
- 现有 `Upgrade.cpp` 已用 mbedtls（MD5），无需新增依赖

## 5. 改动清单（按文件）

### 5.1 配置层（DeviceSettings）

[`src/config/DeviceSettings.h`](../src/config/DeviceSettings.h)

- **删除**：
  - `voice_baidu_api_key[65]`
  - `voice_baidu_secret_key[65]`
  - `voice_dev_pid`（腾讯用字符串 `EngSerViceType`，不需要数字 PID）
- **新增**：
  - `voice_tencent_secret_id[65]`
  - `voice_tencent_secret_key[65]`

保留字段：`voice_enable` / `voice_trigger_key` / `voice_max_record_ms` / `voice_auto_enter` / `voice_cuid`。

`voice_cuid` 在腾讯云协议里被忽略（仅腾讯云 V2/V3 OAuth 协议使用），固件仍保留字段便于后续迁移，不参与签名。

### 5.2 配置 IO

[`src/config/Configuration.cpp`](../src/config/Configuration.cpp)

- `loadGlobalSettings_locked()`：把 `voice_baidu_api_key` / `voice_baidu_secret_key` / `voice_dev_pid` 三处读取替换为 `voice_tencent_secret_id` / `voice_tencent_secret_key` 两处读取
- `sectionOfKey()`：保持 `voice_*` 前缀规则即可（无需改动）

### 5.3 配置 SET 解析

[`src/config/parseConfigSetCommand.cpp`](../src/config/parseConfigSetCommand.cpp)

- 删除 `voice_dev_pid` 分支
- 删除 `voice_baidu_api_key` / `voice_baidu_secret_key` 分支
- 新增 `voice_tencent_secret_id` / `voice_tencent_secret_key` 分支（写入 `s.voice_tencent_*`，加入 `str_changes` 持久化）

### 5.4 协议 GET/SET 透传

[`src/protocol/commands/cmd_config.cpp`](../src/protocol/commands/cmd_config.cpp)

- `sendConfigSnapshot()`：将三处百度字段替换为两处腾讯字段
- 桌面 App 端契约：`voice_tencent_secret_id` / `voice_tencent_secret_key` 走相同 JSON 键名

### 5.5 语音配置常量

[`src/voice/VoiceConfig.h`](../src/voice/VoiceConfig.h)

- **删除**：
  - `kTokenUrl`（OAuth 端点）
  - `kAsrUrlBase`（百度端点 + 拼接 `?dev_pid=&cuid=&token=`）
  - `kAsrContentType`（`audio/pcm;rate=16000`；腾讯为 JSON）
  - `kDefaultDevPid`
  - `kTokenRefreshMarginMs`
- **新增**：
  - `kTencentAsrHost` = `"asr.tencentcloudapi.com"`
  - `kTencentAsrAction` = `"SentenceRecognition"`
  - `kTencentAsrVersion` = `"2019-06-14"`
  - `kTencentAsrRegion` = `"ap-shanghai"`
  - `kTencentAsrEngine` = `"16k_zh"`
  - `kTencentAsrVoiceFormat` = `"wav"`（与 PCM 16k/16bit 单声道兼容）
  - `kTencentAsrTimeoutMs` = `10000`
- **保留**：录音参数 / HID 注入延迟 / token 相关以外的常量

### 5.6 签名器（新增）

[`src/voice/TencentAsrSigner.h`](../src/voice/TencentAsrSigner.h)（新文件）

```cpp
namespace ekeys::voice {
// 生成 TC3-HMAC-SHA256 头（Authorization / X-TC-*）所需字段。
// 内部用 mbedtls HMAC-SHA256；栈 ≤ 256B。
struct SignedRequest {
    char timestamp[16];      // "1700000000"
    char authorization[512];  // 完整 Authorization 头值
    char date[16];            // "2026-09-08"
};

// payload: 序列化后的 JSON 体（必须与 POST body 完全一致）
bool signRequest(const char *secret_id, const char *secret_key,
                 const char *payload, size_t payload_len,
                 SignedRequest &out);
}  // namespace ekeys::voice
```

[`src/voice/TencentAsrSigner.cpp`](../src/voice/TencentAsrSigner.cpp)（新文件）

- SHA256 hex 工具（局部静态函数，16 字节摘要转 32 hex 字符）
- HMAC-SHA256 工具（基于 `mbedtls/md.h`，4 段链）
- 主函数按 §4.2 三步组装，输出 `SignedRequest`

### 5.7 Token 缓存（删除）

- [`src/voice/AsrTokenCache.h`](../src/voice/AsrTokenCache.h)
- [`src/voice/AsrTokenCache.cpp`](../src/voice/AsrTokenCache.cpp)

直接删除（百度 OAuth 不再需要）。`VoiceRecognizer` 不再调用 `AsrTokenCache::instance()`。

### 5.8 识别主循环（核心）

[`src/voice/VoiceRecognizer.cpp`](../src/voice/VoiceRecognizer.cpp)

- 删除 `#include "voice/AsrTokenCache.h"`
- 新增 `#include "voice/TencentAsrSigner.h"`
- `AsrJob` 结构：
  - 删除 `dev_pid`
  - `cuid` 保留（仅日志占位，不参与请求）
  - 新增 `secret_id[65]` / `secret_key[65]`（从 `DeviceSettings` 快照时填充）
- `asrTaskLoop()`：
  - **删除**：token 获取 / URL `?dev_pid=&cuid=&token=` 拼接
  - **新增**：
    1. 从 job 拿 `secret_id` / `secret_key`
    2. 用 ArduinoJson 构造请求 JSON（`EngSerViceType` / `SourceType=1` / `VoiceFormat="wav"` / `Data`（base64） / `DataLen`）
    3. `voice::signRequest(...)` 生成签名头
    4. `http.begin("https://" + host)`，`http.addHeader("Host", host)` + 7 个 X-TC-\* 头 + `Authorization`
    5. `http.POST(payload, json_len)`
    6. 解析响应：`Response.Result` 文本 或 `Response.Error.Code` 报错
  - 失败路径与原来一致（`LOG_ERROR` / `free(job.pcm)` / `continue`）
- 日志前缀从 `baidu err` 改为 `tencent err`，错误码字段统一为字符串

> 注：base64 编码 PCM。用 `mbedtls/base64.h`（项目已有使用，见 [`cmd_profile.cpp`](../src/protocol/commands/cmd_profile.cpp)），最大 PCM 缓冲 30s × 16k × 2B = 960KB，base64 后 ≈ 1.28MB，**仍 < 3MB 上限**。但本项目 `voice_max_record_ms` 上限 60000ms 已设硬限（参见 [`parseConfigSetCommand.cpp`](../src/config/parseConfigSetCommand.cpp#L291-L306)），缓冲最多 60s × 16k × 2B = 1.92MB，base64 ≈ 2.56MB，**接近上限但仍合法**；任务栈 8KB 需升级到 **12KB**（容纳 base64 输出 + HTTPClient + JsonDocument）。

### 5.9 HA 屏（无需改动）

[`src/ui/ui_HaScreenSecondary.c`](../src/ui/ui_HaScreenSecondary.c) 仅展示 `voice_enabled` / `voice_recording` 状态，与后端解耦，无需适配。

### 5.10 设置二级页（无需改动）

[`src/ui/ui_SettingScreenSecondary.c`](../src/ui/ui_SettingScreenSecondary.c) 仅暴露 `voice_enable`；SecretId/Key 通过桌面 App 推送。

### 5.11 任务栈

[`src/voice/VoiceRecognizer.cpp`](../src/voice/VoiceRecognizer.cpp#L296-L308) `xTaskCreate` 栈 8192 → **12288**（base64 临时缓冲 + HTTPClient + ArduinoJson 文档双开）。

### 5.12 data/config.ini

[`data/config.ini`](../data/config.ini) **无需新增默认项**（首次上电走默认值，全 0 表示未配置）；语音段保持现状即可。

### 5.13 文档

- [`docs/06-network-voice-rgb-audio.md`](../docs/06-network-voice-rgb-audio.md) §6.10~6.13 章节：把"百度"替换为"腾讯云一句话识别"，删除 `AsrTokenCache` 任务，文档新增 TencentAsrSigner 任务
- [`FEATURE_DOC.md §11`](../FEATURE_DOC.md)：
  - §11.1 引擎从百度改为腾讯云一句话识别，凭证改为 `voice_tencent_secret_id` / `voice_tencent_secret_key`
  - §11.2 流程不变（`CMD_VOICE_TEXT` 推送 App 文本保持）
  - §11.3 限制保持（USB + WiFi 已连 + voice_enable）

## 6. 验证

> 项目规则：不主动编译，需用户明确要求后才执行 `platformio run`。

### 6.1 单元/集成验证项

- [ ] **签名单元测试**：从腾讯云官方签名文档取一对示例凭证，与固件 `signRequest()` 输出对比 `Authorization` 头（实施时附参考文档链接；不在本仓硬编码示例值，避免误触发 Secret Scanner）
- [ ] **空凭证短路**：`voice_tencent_secret_id` 或 `key` 为空 → `asrTaskLoop` 直接 `LOG_ERROR("ASR", "no secret, skip")` + `free(pcm)` + `continue`
- [ ] **网络失败降级**：`http.POST` 非 200 → `LOG_ERROR` + 释放，不上报
- [ ] **JSON 解析**：正常 + 错误响应（`Response.Error`）两条路径
- [ ] **端到端**（仅文档描述，不主动执行）：USB 模式 + WiFi + 语音键 → ASR → 桌面 App 收到文本

### 6.2 自检记录（实施后追加）

- ASR 平均响应时长
- PCM 60s 上限 base64 后体积确认 < 3MB

## 7. 风险与回退

| 风险                          | 缓解                                                                 |
| ----------------------------- | -------------------------------------------------------------------- |
| SecretKey 明文落 `config.ini` | 仅本地 SPIFFS；README 提醒家庭使用；若需进一步加固，后续接 SCF 代理  |
| 60s 录音接近 3MB 上限         | 实际缓冲取自 `voice_max_record_ms`（用户配），默认 5000ms 远小于上限 |
| TC3 签名实现 bug 导致全员失败 | 先用官方 Python SDK 在本地交叉验证签名串，再合入固件                 |
| 桌面 App 还在推旧字段         | 桌面 App 同步切换字段名；本项目内字段已统一切换，App 改动不属本仓    |

## 8. 任务清单

- [ ] **8.1** 修改 `DeviceSettings` 字段（删除 3 个 / 新增 2 个）
- [ ] **8.2** 修改 `Configuration.cpp` 加载段
- [ ] **8.3** 修改 `parseConfigSetCommand.cpp` SET 解析
- [ ] **8.4** 修改 `cmd_config.cpp` GET 快照
- [ ] **8.5** 修改 `VoiceConfig.h` 常量（删除百度 / 新增腾讯）
- [ ] **8.6** 新建 `TencentAsrSigner.h/.cpp`
- [ ] **8.7** 删除 `AsrTokenCache.h/.cpp`
- [ ] **8.8** 修改 `VoiceRecognizer.cpp` 后台识别循环
- [ ] **8.9** 任务栈 8192 → 12288
- [ ] **8.10** 更新 `docs/06` + `FEATURE_DOC.md`
- [ ] **8.11** 用户确认后执行编译验证（按规则不主动触发）
- [ ] **8.12** 端到端联调（语音键 → ASR → 桌面 App 文本）

## 9. 关键路径参考

- 签名规范：<https://cloud.tencent.com/document/api/1093/35640>
- ASR 文档：<https://cloud.tencent.com/document/product/1093/35646>
- 旧百度实现：[`src/voice/AsrTokenCache.cpp`](../src/voice/AsrTokenCache.cpp)（仅参考删前逻辑）
- mbedtls HMAC 用法参考：[`src/upgrade/Upgrade.cpp`](../src/upgrade/Upgrade.cpp)（MD5 风格）
- mbedtls base64 用法参考：[`src/protocol/commands/cmd_profile.cpp`](../src/protocol/commands/cmd_profile.cpp)
