/*
 * TencentAsrSigner.h
 *
 * 腾讯云一句话识别 TC3-HMAC-SHA256 请求签名器（阶段 08，
 * 签名规范 https://cloud.tencent.com/document/api/1093/35640）。
 *
 *   - 内部用 mbedtls HMAC-SHA256（4 段链）+ SHA256（payload / canonical 摘要）
 *   - 仅依赖 <time.h> 系统时间（NTP 已同步前提下有效）
 */

#ifndef EKEYS_VOICE_TENCENT_ASR_SIGNER_H
#define EKEYS_VOICE_TENCENT_ASR_SIGNER_H

#include <stddef.h>

namespace ekeys::voice {

/* 生成 TC3-HMAC-SHA256 头（Authorization / X-TC-Timestamp）所需字段 */
struct SignedRequest {
    char timestamp[16];       // "1700000000"（10 位 unix 秒）
    char authorization[512];  // 完整 Authorization 头值
    char date[16];            // "2026-09-08"（UTC 日期，来自时间戳）
};

/*
 * 对完整 JSON payload 计算签名。
 * payload 必须与后续 http.POST 发送的 body 逐字节一致。
 * 失败（凭证为空 / 系统时间未同步）返回 false。
 */
bool signRequest(const char *secret_id, const char *secret_key,
                 const char *payload, size_t payload_len,
                 SignedRequest &out);

}  // namespace ekeys::voice

#endif  // EKEYS_VOICE_TENCENT_ASR_SIGNER_H
