/*
 * VoiceConfig.h
 *
 * 语音识别常量（FEATURE_DOC §11，阶段 06 任务 6.11）。
 * 凭证（api_key / secret_key / dev_pid / cuid）运行时从 DeviceSettings 读取，
 * 此处只放协议端点与硬编码行为参数。
 */

#ifndef EKEYS_VOICE_VOICE_CONFIG_H
#define EKEYS_VOICE_VOICE_CONFIG_H

namespace ekeys::voice {

/* 百度 OAuth2 token 端点（GET，client_credentials） */
constexpr const char *kTokenUrl =
    "https://openapi.baidu.com/oauth/2.0/token"
    "?grant_type=client_credentials&client_id=%s&client_secret=%s";

/* 百度短语音识别（raw PCM POST；dev_pid / cuid / token 由调用方拼接） */
constexpr const char *kAsrUrlBase = "https://vop.baidu.com/server_api";
constexpr const char *kAsrContentType = "audio/pcm;rate=16000";

/* 默认 cuid（FEATURE_DOC §11.1 建议 FunModularKeyboard，本项目用 EKeys） */
constexpr const char *kDefaultCuid = "EKeys";
constexpr uint16_t kDefaultDevPid = 1537;  // 普通话（有标点）

/* token 提前 60s 刷新，避免边界过期 */
constexpr uint32_t kTokenRefreshMarginMs = 60000;

/* 录音参数 */
constexpr uint32_t kPcmSampleRate = 16000;
constexpr uint32_t kMinRecordMs = 200;          // 短于该时长放弃识别
constexpr uint32_t kMaxRecordMsCap = 30000;     // 缓冲上限（PSRAM ~960KB）
constexpr size_t kFeedChunkSamples = 512;       // Mic.Read 单块大小

/* 兜底 HID 注入：每字符间隔 */
constexpr uint8_t kAsciiInjectDelayMs = 8;

}  // namespace ekeys::voice

#endif  // EKEYS_VOICE_VOICE_CONFIG_H
