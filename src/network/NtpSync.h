/*
 * NtpSync.h
 *
 * NTP 时间同步（FEATURE_DOC §7.2，阶段 06 任务 6.2）。
 *
 *   - 服务器 pool.ntp.org，时区 GMT+8
 *   - WiFi 连上后 requestSync()，process() 轮询同步完成
 *   - getTime() 供 MainTask 的 TIME_UPDATE 与 UI 状态条使用
 *
 *   - setEpoch() 由 USB 协议（CMD_TIME_SET）注入，优先级高于 NTP：
 *     已通过 USB 注入时不再被 NTP 覆盖，避免来回校时。
 */

#ifndef EKEYS_NETWORK_NTP_SYNC_H
#define EKEYS_NETWORK_NTP_SYNC_H

#include <stdint.h>
#include <time.h>

namespace ekeys {

class NtpSync {
public:
    static NtpSync &instance();

    NtpSync(const NtpSync &) = delete;
    NtpSync &operator=(const NtpSync &) = delete;

    /* WiFi 连上后调用一次；重复调用无害 */
    void requestSync();

    /* MainTask::loop() 周期驱动（检查同步是否完成） */
    void process();

    bool synced() const { return synced_; }

    /* 同步成功时输出本地时间 "HH:MM:SS"；未同步返回 false */
    bool getLocalTimeStr(char *out, size_t cap);

    /*
     * USB 注入时间（CMD_TIME_SET 走这里）。
     *   - epoch：自 1970-01-01 UTC 起的秒数
     *   - tz：POSIX TZ 字符串，nullptr/"" 表示保持当前 TZ（默认 CST-8）
     * 注入后 synced_ 置 true；后续 NTP 同步不会覆盖（除非重新 requestSync）。
     */
    void setEpoch(int64_t epoch, const char *tz);

private:
    NtpSync() = default;

    bool requested_ = false;
    bool synced_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_NETWORK_NTP_SYNC_H
