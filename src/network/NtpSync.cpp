/*
 * NtpSync.cpp
 *
 * 见 NtpSync.h。
 */

#include "NtpSync.h"

#include <Arduino.h>
#include <esp_sntp.h>

#include "logging/LogManager.h"

namespace ekeys {

NtpSync &NtpSync::instance()
{
    static NtpSync inst;
    return inst;
}

void NtpSync::requestSync()
{
    if (requested_)
    {
        return;
    }
    requested_ = true;

    /* GMT+8（FEATURE_DOC §7.2） */
    setenv("TZ", "CST-8", 1);
    tzset();
    configTzTime("CST-8", "pool.ntp.org", "time.windows.com");
    LOG_INFO("NTP", "sync requested (pool.ntp.org, GMT+8)");
}

void NtpSync::process()
{
    if (requested_ && !synced_ && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
    {
        synced_ = true;
        struct tm tm_now;
        if (getLocalTime(&tm_now, 100))
        {
            LOG_INFO("NTP", "synced: %02d:%02d:%02d",
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        }
    }
}

bool NtpSync::getLocalTimeStr(char *out, size_t cap)
{
    if (!synced_)
    {
        return false;
    }
    struct tm tm_now;
    if (!getLocalTime(&tm_now, 20))
    {
        return false;
    }
    snprintf(out, cap, "%02d:%02d:%02d",
             tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    return true;
}

}  // namespace ekeys
