/*
 * AsrTokenCache.cpp
 *
 * 见 AsrTokenCache.h。
 */

#include "AsrTokenCache.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config/Configuration.h"
#include "logging/LogManager.h"
#include "voice/VoiceConfig.h"

namespace ekeys {

AsrTokenCache &AsrTokenCache::instance()
{
    static AsrTokenCache inst;
    return inst;
}

const char *AsrTokenCache::getToken()
{
    DeviceSettings snap;
    Configuration::instance().snapshot(snap);

    /* 未配置凭证 */
    if (snap.voice_baidu_api_key[0] == '\0' ||
        snap.voice_baidu_secret_key[0] == '\0')
    {
        return nullptr;
    }

    const bool credChanged =
        strcmp(api_key_, snap.voice_baidu_api_key) != 0 ||
        strcmp(secret_key_, snap.voice_baidu_secret_key) != 0;

    if (token_[0] != '\0' && !credChanged &&
        millis() + voice::kTokenRefreshMarginMs < expires_at_ms_)
    {
        return token_;
    }
    return refresh() ? token_ : nullptr;
}

void AsrTokenCache::invalidate()
{
    token_[0] = '\0';
    expires_at_ms_ = 0;
}

bool AsrTokenCache::refresh()
{
    DeviceSettings snap;
    Configuration::instance().snapshot(snap);

    char url[320];
    snprintf(url, sizeof(url), voice::kTokenUrl,
             snap.voice_baidu_api_key, snap.voice_baidu_secret_key);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    const int code = http.GET();
    if (code != 200)
    {
        LOG_ERROR("ASR", "token http %d", code);
        http.end();
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err)
    {
        LOG_ERROR("ASR", "token json: %s", err.c_str());
        return false;
    }

    const char *access = doc["access_token"] | "";
    const long expires_in = doc["expires_in"] | 0;
    if (access[0] == '\0' || expires_in <= 0)
    {
        LOG_ERROR("ASR", "token response invalid");
        return false;
    }

    snprintf(token_, sizeof(token_), "%s", access);
    expires_at_ms_ = millis() + static_cast<uint32_t>(expires_in) * 1000U;
    /*
     * A4 修复：用本次 refresh() 入口处的 snap 回填凭证（TOCTOU）。
     * 之前 refresh() 拿到响应后又 snapshot 一次，期间凭证可能被
     * CMD_CONFIG_SET 改掉，导致 token 与 api_key_/secret_key_ 错配。
     */
    snprintf(api_key_, sizeof(api_key_), "%s", snap.voice_baidu_api_key);
    snprintf(secret_key_, sizeof(secret_key_), "%s", snap.voice_baidu_secret_key);
    LOG_INFO("ASR", "token refreshed (expires_in=%lds)", expires_in);
    return true;
}

}  // namespace ekeys
