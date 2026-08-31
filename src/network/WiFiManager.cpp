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
        constexpr uint8_t kWorkModeBluetooth = 1;

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
        LOG_INFO("WIFI", "manager ready (default off)");
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

        /* 同一配置（ssid+password）重复调度去重：简单用 ssid 首地址+长度粗判 */
        uint32_t serial = 0;
        for (size_t i = 0; snap.wifi_ssid[i] != '\0'; ++i)
        {
            serial = serial * 31 + static_cast<uint8_t>(snap.wifi_ssid[i]);
        }

        if (state_ == State::Connecting && serial == last_request_serial_)
        {
            return; // 正在连接同一目标
        }
        last_request_serial_ = serial;
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
        WiFi.disconnect(true);
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
                return;
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
            break;

        case State::Connecting:
            if (WiFi.status() == WL_CONNECTED)
            {
                state_ = State::Connected;
                link_lost_ms_ = 0;
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
