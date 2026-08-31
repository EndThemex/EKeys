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

namespace ekeys {

namespace {

constexpr uint32_t kWifiRetryIntervalMs = 5000;    // FEATURE_DOC §7.1
constexpr uint32_t kWifiConnectTimeoutMs = 10000;
constexpr uint8_t kWorkModeBluetooth = 1;

}  // namespace

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
        return;  // 正在连接同一目标
    }
    last_request_serial_ = serial;
    state_ = State::WaitingRetry;
    state_entered_ms_ = 0;  // 下一拍 process() 立即尝试
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

void WiFiManager::process()
{
    const uint32_t now = millis();

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
        if (WiFi.status() != WL_CONNECTED)
        {
            if (!logged_fail_)
            {
                LOG_WARNING("WIFI", "link lost, reconnecting");
                logged_fail_ = true;
            }
            state_ = State::WaitingRetry;
            state_entered_ms_ = now;
            last_connect_attempt_ms_ = now;
        }
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

}  // namespace ekeys
