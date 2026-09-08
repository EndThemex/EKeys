/*
 * WiFiManager.cpp
 *
 * 见 WiFiManager.h。
 */

#include "WiFiManager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config/Configuration.h"
#include "logging/LogManager.h"

namespace ekeys
{

    namespace
        {

        constexpr uint32_t kWifiRetryIntervalMs = 5000; // FEATURE_DOC §7.1
        constexpr uint32_t kWifiConnectTimeoutMs = 10000;
        constexpr uint32_t kWifiLinkGraceMs = 15000; // 断链自恢复宽限（docs/07 7.6）
        constexpr uint32_t kWifiHeartbeatMs = 30000; // 等待/连接中的节流心跳，避免反复重试时刷屏
        constexpr uint8_t kWorkModeBluetooth = 1;

        uint32_t s_last_wifi_log_ms = 0;  // process() 心跳节流
        bool s_logged_disabled = false;   // wifi_switch=0 已经打过日志，避免每次 loop 输出
        bool s_logged_ble = false;        // BLE 模式已经打过日志

    } // namespace

    WiFiManager &WiFiManager::instance()
    {
        static WiFiManager inst;
        return inst;
    }

    void WiFiManager::begin()
    {
        /* 不在 begin 时自动连接；由 MainTask 根据配置调度 */
        WiFi.mode(WIFI_OFF);
        state_ = State::Idle;
        s_last_wifi_log_ms = 0;
        s_logged_disabled = false;
        s_logged_ble = false;
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        LOG_INFO("WIFI", "manager ready (mode_off, wifi_switch=%u work_mode=%u ssid=%s)",
                 snap.wifi_switch, snap.work_mode,
                 snap.wifi_ssid[0] ? snap.wifi_ssid : "(empty)");
    }

    bool WiFiManager::isEnabled() const
    {
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        return (snap.wifi_switch != 0) && (snap.work_mode != kWorkModeBluetooth);
    }

    void WiFiManager::scheduleConnect()
    {
        if (!isEnabled())
        {
            if (state_ != State::Idle)
            {
                stopReconnect();
            }
            return;
        }

        DeviceSettings snap;
        Configuration::instance().snapshot(snap);

        /* C1 修复：空 SSID 直接拒绝，避免进入 WaitingRetry 再被 startConnect 回退 */
        if (snap.wifi_ssid[0] == '\0')
        {
            LOG_WARNING("WIFI", "empty ssid, schedule ignored");
            if (state_ != State::Idle)
            {
                stopReconnect();
            }
            return;
        }

        /* 同一配置（ssid+password）重复调度去重：串行化 SSID+密码，密码变更不会被吞 */
        uint32_t serial = 0;
        for (size_t i = 0; snap.wifi_ssid[i] != '\0'; ++i)
        {
            serial = serial * 31 + static_cast<uint8_t>(snap.wifi_ssid[i]);
        }
        serial = serial * 31 + 0xFF; /* 分隔常量，避免 ssid/password 拼接碰撞 */
        for (size_t i = 0; snap.wifi_password[i] != '\0'; ++i)
        {
            serial = serial * 31 + static_cast<uint8_t>(snap.wifi_password[i]);
        }

        if (state_ == State::Connecting && serial == last_request_serial_)
        {
            return; // 正在连接同一目标
        }
        last_request_serial_ = serial;
        s_last_wifi_log_ms = 0; // 重新调度后立即允许下一次心跳输出
        state_ = State::WaitingRetry;
        state_entered_ms_ = 0; // 下一拍 process() 立即尝试
        LOG_INFO("WIFI", "connect scheduled (ssid=%s)", snap.wifi_ssid);
    }

    void WiFiManager::stopReconnect()
    {
        if (state_ != State::Idle)
        {
            LOG_INFO("WIFI", "stopped");
        }
        state_ = State::Idle;
        /*
         * C10 修复：先 WiFi.mode(WIFI_STA) 把射频切回 STA 再 disconnect，
         * 避免 arduino-esp32 2.0.11 在 OFF 模式下 disconnect 触发的告警。
         */
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
    }

    void WiFiManager::startConnect(uint32_t now)
    {
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        if (snap.wifi_ssid[0] == '\0')
        {
            LOG_WARNING("WIFI", "empty ssid, skip");
            state_ = State::Idle;
            return;
        }

        WiFi.mode(WIFI_STA);
        WiFi.begin(snap.wifi_ssid, snap.wifi_password);
        state_ = State::Connecting;
        state_entered_ms_ = now;
        last_connect_attempt_ms_ = now;
        logged_fail_ = false;
        s_last_wifi_log_ms = 0;
        LOG_INFO("WIFI", "connecting to %s ...", snap.wifi_ssid);
    }

    void WiFiManager::processWiFiReconnect(uint32_t now)
    {
        if (state_ != State::Connected)
        {
            link_lost_ms_ = 0;
            return;
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            link_lost_ms_ = 0;
            return;
        }

        /* 断链：先给 15s 宽限等 STA 自动重连，未恢复再强制重启射频 */
        if (link_lost_ms_ == 0)
        {
            link_lost_ms_ = now;
            if (!logged_fail_)
            {
                LOG_WARNING("WIFI", "link lost, waiting %us for auto-reconnect",
                            static_cast<unsigned>(kWifiLinkGraceMs / 1000));
                logged_fail_ = true;
            }
            return;
        }
        if ((now - link_lost_ms_) < kWifiLinkGraceMs)
        {
            return; // 宽限期内等待自恢复
        }

        LOG_WARNING("WIFI", "link lost > %us, force restart wifi",
                    static_cast<unsigned>(kWifiLinkGraceMs / 1000));
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        link_lost_ms_ = 0;
        state_ = State::WaitingRetry;
        state_entered_ms_ = now;
        last_connect_attempt_ms_ = now - kWifiRetryIntervalMs; // 下一拍立即重连
    }

    void WiFiManager::process()
    {
        const uint32_t now = millis();

        /* BLE 模式整体短路（docs/07 7.6）：不启动 / 不维持任何 WiFi 活动 */
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            if (snap.work_mode == kWorkModeBluetooth)
            {
                if (state_ != State::Idle)
                {
                    stopReconnect();
                }
                if (!s_logged_ble)
                {
                    LOG_INFO("WIFI", "BLE mode, wifi permanently off");
                    s_logged_ble = true;
                }
                s_logged_disabled = false;
                return;
            }
            /* 非 BLE：wifi_switch=0 → 用户主动关闭，打一次日志便于诊断 WiFi 不工作 */
            s_logged_ble = false;
            if (snap.wifi_switch == 0)
            {
                if (!s_logged_disabled)
                {
                    LOG_INFO("WIFI", "wifi_switch=0, wifi disabled by config");
                    s_logged_disabled = true;
                }
            }
            else
            {
                /* 开关重新打开后允许再次输出禁用日志 */
                s_logged_disabled = false;
            }
        }

        processWiFiReconnect(now);

        switch (state_)
        {
        case State::Idle:
            break;

        case State::WaitingRetry:
            /* scheduleConnect 后下一拍立即尝试；重试期间按 5s 间隔 */
            if (state_entered_ms_ == 0 ||
                (now - last_connect_attempt_ms_) >= kWifiRetryIntervalMs)
            {
                startConnect(now);
            }
            else if ((now - s_last_wifi_log_ms) >= kWifiHeartbeatMs)
            {
                s_last_wifi_log_ms = now;
                DeviceSettings snap;
                Configuration::instance().snapshot(snap);
                LOG_INFO("WIFI", "waiting retry (%lus, next in %lus, ssid=%s)",
                         static_cast<unsigned long>((now - state_entered_ms_) / 1000U),
                         static_cast<unsigned long>(
                             (kWifiRetryIntervalMs - (now - last_connect_attempt_ms_)) / 1000U),
                         snap.wifi_ssid);
            }
            break;

        case State::Connecting:
            if (WiFi.status() == WL_CONNECTED)
            {
                state_ = State::Connected;
                link_lost_ms_ = 0;
                s_last_wifi_log_ms = 0;
                LOG_INFO("WIFI", "connected, ip=%s rssi=%d",
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());
                if (on_connected_ != nullptr)
                {
                    on_connected_();
                }
            }
            else if ((now - state_entered_ms_) >= kWifiConnectTimeoutMs)
            {
                LOG_WARNING("WIFI", "connect timeout, retry in %us",
                            static_cast<unsigned>(kWifiRetryIntervalMs / 1000));
                WiFi.disconnect();
                state_ = State::WaitingRetry;
                state_entered_ms_ = now;
                last_connect_attempt_ms_ = now;
                s_last_wifi_log_ms = 0;
            }
            else if ((now - s_last_wifi_log_ms) >= kWifiHeartbeatMs)
            {
                s_last_wifi_log_ms = now;
                LOG_INFO("WIFI", "connecting... %lus elapsed",
                         static_cast<unsigned long>((now - state_entered_ms_) / 1000U));
            }
            break;

        case State::Connected:
            /* 断链处理全部交给 processWiFiReconnect（15s 宽限 + 强制重启） */
            break;
        }
    }

    bool WiFiManager::isConnected() const
    {
        return state_ == State::Connected && WiFi.status() == WL_CONNECTED;
    }

    int WiFiManager::rssi() const
    {
        return isConnected() ? WiFi.RSSI() : 0;
    }

} // namespace ekeys
