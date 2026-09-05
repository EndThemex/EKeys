/*
 * AsrTokenCache.h
 *
 * 百度 ASR access_token 自动获取与缓存（FEATURE_DOC §11.1，
 * 阶段 06 任务 6.11）。
 *
 *   - 凭证来自 DeviceSettings（voice_baidu_api_key / voice_baidu_secret_key）
 *   - 到期前 60s 自动刷新；凭证变更立即重取
 *   - 阻塞 HTTP GET（MainTask 上下文，识别路径上低频调用）
 */

#ifndef EKEYS_VOICE_ASR_TOKEN_CACHE_H
#define EKEYS_VOICE_ASR_TOKEN_CACHE_H

#include <stddef.h>
#include <stdint.h>

namespace ekeys {

class AsrTokenCache {
public:
    static AsrTokenCache &instance();

    AsrTokenCache(const AsrTokenCache &) = delete;
    AsrTokenCache &operator=(const AsrTokenCache &) = delete;

    /*
     * 返回有效 token；失败返回 nullptr。
     * 内部按需刷新（到期 / 凭证变化 / 首次调用）。
     */
    const char *getToken();

    void invalidate();

private:
    AsrTokenCache() = default;

    bool refresh();

    char token_[128] = {0};
    uint32_t expires_at_ms_ = 0;   // millis() 时刻
    char api_key_[65] = {0};       // 上次成功使用的凭证（变更检测）
    char secret_key_[65] = {0};
};

}  // namespace ekeys

#endif  // EKEYS_VOICE_ASR_TOKEN_CACHE_H
