/*
 * TencentAsrSigner.cpp
 *
 * 见 TencentAsrSigner.h。三步实现（docs/08 §4.2）：
 *   Step1 CanonicalRequest = POST\n/\n<headers>\n\n<signed>\n<sha256(payload)>
 *   Step2 StringToSign     = TC3-HMAC-SHA256\n<ts>\n<date>/asr/tc3_request\n<sha256(canonical)>
 *   Step3 签名链           = HMAC("TC3"+SecretKey, date) → "asr" → "tc3_request" → StringToSign
 */

#include "TencentAsrSigner.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

#include "voice/VoiceConfig.h"

namespace ekeys::voice {

namespace {

constexpr const char *kSignAlgo = "TC3-HMAC-SHA256";
constexpr const char *kSignedHeaders = "content-type;host";
constexpr const char *kService = "asr";
constexpr const char *kCredentialSuffix = "tc3_request";
/* 2020-01-01 之前的系统时间视为 NTP 未同步 */
constexpr time_t kMinValidEpoch = 1577836800;

/* 32 字节摘要 → 64 位小写 hex（out 容量 ≥ 65） */
void digestToHex(const uint8_t digest[32], char out[65])
{
    static const char kHex[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i)
    {
        out[i * 2] = kHex[digest[i] >> 4];
        out[i * 2 + 1] = kHex[digest[i] & 0x0F];
    }
    out[64] = '\0';
}

/* 单次 SHA256 → 小写 hex */
void sha256Hex(const uint8_t *data, size_t len, char out[65])
{
    uint8_t digest[32];
    mbedtls_sha256_ret(data, len, digest, 0); // 0 = SHA-256（非 224）
    digestToHex(digest, out);
}

/* 单消息 HMAC-SHA256（消息均为小字符串，一次性 update 足够） */
void hmacSha256(const uint8_t *key, size_t key_len,
                const uint8_t *msg, size_t msg_len,
                uint8_t out[32])
{
    const mbedtls_md_info_t *info =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1); // 1 = HMAC 模式
    mbedtls_md_hmac_starts(&ctx, key, key_len);
    mbedtls_md_hmac_update(&ctx, msg, msg_len);
    mbedtls_md_hmac_finish(&ctx, out);
    mbedtls_md_free(&ctx);
}

}  // namespace

bool signRequest(const char *secret_id, const char *secret_key,
                 const char *payload, size_t payload_len,
                 SignedRequest &out)
{
    if (secret_id == nullptr || secret_key == nullptr ||
        secret_id[0] == '\0' || secret_key[0] == '\0' ||
        payload == nullptr || payload_len == 0)
    {
        return false;
    }

    const time_t now = time(nullptr);
    if (now < kMinValidEpoch)
    {
        return false; // NTP 未同步，时间戳不可信（签名会被服务端拒绝）
    }
    snprintf(out.timestamp, sizeof(out.timestamp), "%lld",
             static_cast<long long>(now));

    /* UTC 日期（CredentialScope 用，非本地时区） */
    struct tm utc;
    gmtime_r(&now, &utc);
    snprintf(out.date, sizeof(out.date), "%04d-%02d-%02d",
             utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);

    /* Step1：CanonicalRequest（头名小写、字典序，已排序 content-type < host） */
    char payload_hash[65];
    sha256Hex(reinterpret_cast<const uint8_t *>(payload), payload_len,
              payload_hash);
    char canonical[256];
    const int canon_len = snprintf(
        canonical, sizeof(canonical),
        "POST\n/\n"
        "content-type:%s\n"
        "host:%s\n"
        "\n"
        "%s\n"
        "%s",
        kTencentAsrContentType, kTencentAsrHost, kSignedHeaders, payload_hash);
    if (canon_len <= 0 || static_cast<size_t>(canon_len) >= sizeof(canonical))
    {
        return false;
    }

    /* Step2：StringToSign */
    char canonical_hash[65];
    sha256Hex(reinterpret_cast<const uint8_t *>(canonical),
              static_cast<size_t>(canon_len), canonical_hash);
    char string_to_sign[160];
    const int sts_len = snprintf(
        string_to_sign, sizeof(string_to_sign),
        "%s\n%s\n%s/%s/%s\n%s",
        kSignAlgo, out.timestamp, out.date, kService, kCredentialSuffix,
        canonical_hash);
    if (sts_len <= 0 || static_cast<size_t>(sts_len) >= sizeof(string_to_sign))
    {
        return false;
    }

    /* Step3：签名链 */
    char key0[3 + 65]; // "TC3" + SecretKey
    memcpy(key0, "TC3", 3);
    strcpy(key0 + 3, secret_key);
    uint8_t k_date[32], k_service[32], k_signing[32], signature[32];
    hmacSha256(reinterpret_cast<const uint8_t *>(key0), 3 + strlen(secret_key),
               reinterpret_cast<const uint8_t *>(out.date), strlen(out.date),
               k_date);
    hmacSha256(k_date, sizeof(k_date),
               reinterpret_cast<const uint8_t *>(kService), strlen(kService),
               k_service);
    hmacSha256(k_service, sizeof(k_service),
               reinterpret_cast<const uint8_t *>(kCredentialSuffix),
               strlen(kCredentialSuffix), k_signing);
    hmacSha256(k_signing, sizeof(k_signing),
               reinterpret_cast<const uint8_t *>(string_to_sign),
               static_cast<size_t>(sts_len), signature);

    char signature_hex[65];
    digestToHex(signature, signature_hex);
    snprintf(out.authorization, sizeof(out.authorization),
             "%s Credential=%s/%s/%s/%s, SignedHeaders=%s, Signature=%s",
             kSignAlgo, secret_id, out.date, kService, kCredentialSuffix,
             kSignedHeaders, signature_hex);
    return true;
}

}  // namespace ekeys::voice
