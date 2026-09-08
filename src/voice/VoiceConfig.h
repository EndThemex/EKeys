/*
 * VoiceConfig.h
 *
 * 语音识别常量（FEATURE_DOC §11，阶段 08 迁移至腾讯云一句话识别）。
 * 凭证（secret_id / secret_key / cuid）运行时从 DeviceSettings 读取，
 * 此处只放协议端点与硬编码行为参数。
 */

#ifndef EKEYS_VOICE_VOICE_CONFIG_H
#define EKEYS_VOICE_VOICE_CONFIG_H

#include <stddef.h>

namespace ekeys::voice {

    /* 腾讯云一句话识别（SentenceRecognition，TC3-HMAC-SHA256 头签名） */
    constexpr const char *kTencentAsrHost = "asr.tencentcloudapi.com";
    constexpr const char *kTencentAsrAction = "SentenceRecognition";
    constexpr const char *kTencentAsrVersion = "2019-06-14";
    constexpr const char *kTencentAsrRegion = "ap-shanghai";
    constexpr const char *kTencentAsrEngine = "16k_zh";
    /* 16k/16bit/单声道无头 PCM（官方格式表：pcm = raw dump） */
    constexpr const char *kTencentAsrVoiceFormat = "pcm";
    constexpr const char *kTencentAsrContentType = "application/json; charset=utf-8";
    constexpr uint32_t kTencentAsrTimeoutMs = 10000;

    /* 默认 cuid（腾讯云 TC3 协议不使用，保留占位便于后续迁移） */
    constexpr const char *kDefaultCuid = "EKeys";

    /* 录音参数 */
    constexpr uint32_t kPcmSampleRate = 16000;
    constexpr uint32_t kMinRecordMs = 200;      // 短于该时长放弃识别
    constexpr uint32_t kMaxRecordMsCap = 30000; // 缓冲上限（PSRAM ~960KB）
    constexpr size_t kFeedChunkSamples = 512;   // Mic.Read 单块大小

    /* 兜底 HID 注入：每字符间隔 */
    constexpr uint8_t kAsciiInjectDelayMs = 8;

}  // namespace ekeys::voice

#endif  // EKEYS_VOICE_VOICE_CONFIG_H
