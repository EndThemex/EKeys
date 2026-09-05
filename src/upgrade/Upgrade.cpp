/*
 * Upgrade.cpp
 *
 * 见 Upgrade.h。HTTP 明文 + MD5 完整性校验（局域网场景，docs/07 7.3）。
 */

#include "Upgrade.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/md5.h>

#include "logging/LogManager.h"
#include "network/WiFiManager.h"

namespace ekeys {

namespace {

constexpr uint32_t kHttpTimeoutMs = 10000;
constexpr size_t kStreamBufSize = 1024;

/* 32 位十六进制 MD5 字符串 → 16 字节摘要；格式非法返回 false */
bool hexToDigest(const char *hex, uint8_t out[16])
{
    if (strlen(hex) != 32)
    {
        return false;
    }
    for (int i = 0; i < 16; ++i)
    {
        char buf[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        char *end = nullptr;
        const long v = strtol(buf, &end, 16);
        if (end != buf + 2)
        {
            return false;
        }
        out[i] = static_cast<uint8_t>(v);
    }
    return true;
}

}  // namespace

Upgrade &Upgrade::instance()
{
    static Upgrade inst;
    return inst;
}

void Upgrade::requestStart(const char *url, const char *checksum)
{
    snprintf(url_, sizeof(url_), "%s", url ? url : "");
    snprintf(checksum_, sizeof(checksum_), "%s", checksum ? checksum : "");
    pending_ = url_[0] != '\0';
}

void Upgrade::performOta()
{
    pending_ = false;

    if (url_[0] == '\0')
    {
        LOG_WARNING("OTA", "no url");
        return;
    }
    if (!WiFiManager::instance().isConnected())
    {
        LOG_ERROR("OTA", "wifi not connected");
        return;
    }

    /* checksum 必须提供（docs/07 备注：未校验禁止覆盖固件） */
    uint8_t expect[16];
    if (!hexToDigest(checksum_, expect))
    {
        LOG_ERROR("OTA", "invalid/missing md5 checksum");
        return;
    }

    LOG_INFO("OTA", "downloading %s", url_);
    HTTPClient http;
    http.begin(url_);
    http.setTimeout(kHttpTimeoutMs);
    const int code = http.GET();
    if (code != 200)
    {
        LOG_ERROR("OTA", "http %d", code);
        http.end();
        return;
    }

    const int content_len = http.getSize();
    if (content_len <= 0)
    {
        LOG_ERROR("OTA", "unknown content length");
        http.end();
        return;
    }
    if (!Update.begin(content_len))
    {
        LOG_ERROR("OTA", "Update.begin failed (%u bytes)",
                  static_cast<unsigned>(content_len));
        http.end();
        return;
    }

    /* 流式下载 + 边写边算 MD5 */
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts_ret(&ctx);

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[kStreamBufSize];
    size_t written = 0;
    bool io_ok = true;
    /*
     * F7 修复：服务端停滞（available()==0 且 connected()==true）只 delay(1)
     * 会永久挂起、阻塞 MainTask 直到看门狗复位。增加整体 idle 超时：
     * 连续 kOtaIdleTimeoutMs 无新数据则放弃。
     */
    constexpr uint32_t kOtaIdleTimeoutMs = 15000;
    constexpr uint32_t kOtaTotalTimeoutMs = 120000;
    const uint32_t ota_start_ms = millis();
    uint32_t last_progress_ms = ota_start_ms;
    while (written < static_cast<size_t>(content_len) && io_ok)
    {
        const uint32_t now_ms = millis();
        if ((now_ms - ota_start_ms) >= kOtaTotalTimeoutMs)
        {
            LOG_ERROR("OTA", "total timeout (%u/%u)",
                      static_cast<unsigned>(written),
                      static_cast<unsigned>(content_len));
            io_ok = false;
            break;
        }
        const size_t avail =
            stream->available() ? stream->available() : 0;
        if (avail == 0)
        {
            if (!stream->connected())
            {
                LOG_ERROR("OTA", "stream closed early (%u/%d)",
                          static_cast<unsigned>(written), content_len);
                io_ok = false;
                break;
            }
            if ((now_ms - last_progress_ms) >= kOtaIdleTimeoutMs)
            {
                LOG_ERROR("OTA", "idle timeout (%ums no data)",
                          static_cast<unsigned>(kOtaIdleTimeoutMs));
                io_ok = false;
                break;
            }
            delay(1);
            continue;
        }
        const size_t to_read =
            (avail < sizeof(buf)) ? avail : sizeof(buf);
        const int n = stream->readBytes(buf, to_read);
        if (n <= 0)
        {
            LOG_ERROR("OTA", "stream read failed");
            io_ok = false;
            break;
        }
        if (Update.write(buf, static_cast<size_t>(n)) !=
            static_cast<size_t>(n))
        {
            LOG_ERROR("OTA", "flash write failed");
            io_ok = false;
            break;
        }
        mbedtls_md5_update_ret(&ctx, buf, static_cast<size_t>(n));
        written += static_cast<size_t>(n);
        last_progress_ms = millis();
    }

    if (!io_ok)
    {
        mbedtls_md5_free(&ctx);
        Update.abort();
        http.end();
        LOG_ERROR("OTA", "aborted, firmware unchanged");
        return;
    }

    /* 校验通过才 finalize（不通过 abort，当前固件不受影响） */
    uint8_t digest[16];
    mbedtls_md5_finish_ret(&ctx, digest);
    mbedtls_md5_free(&ctx);
    http.end();

    if (memcmp(digest, expect, sizeof(digest)) != 0)
    {
        Update.abort();
        LOG_ERROR("OTA", "md5 mismatch, firmware unchanged");
        return;
    }

    if (!Update.end())
    {
        LOG_ERROR("OTA", "Update.end failed: %s",
                  Update.errorString());
        Update.abort();
        return;
    }

    LOG_INFO("OTA", "ok (%u bytes), rebooting...",
             static_cast<unsigned>(written));
    delay(500); // 等日志/CDC flush
    ESP.restart();
}

}  // namespace ekeys
