/*
 * TcpChannel.cpp
 *
 * 见 TcpChannel.h。
 */

#include "TcpChannel.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "DiscoveryService.h"
#include "WiFiManager.h"
#include "logging/LogManager.h"
#include "protocol/SerialProtocol.h"

namespace ekeys {

namespace {

constexpr uint32_t kTcpConnectTimeoutMs = 5000;
constexpr uint32_t kTcpReconnectDelayMs = 2000;
/* TCP 断开超过该时长仍未恢复 → 强制重启 WiFi（FEATURE_DOC §7.1） */
constexpr uint32_t kTcpReviveWifiAfterMs = 15000;

/*
 * SerialProtocol 发送钩子：协议帧同步发到 TCP。
 * 注意此回调在 SerialProtocol::sendDocument 内调用，禁止再回调协议层。
 */
void protocolLineSink(const char *line)
{
    TcpChannel::instance().sendLine(line);
}

}  // namespace

TcpChannel &TcpChannel::instance()
{
    static TcpChannel inst;
    return inst;
}

void TcpChannel::connectTo(const char *ip, uint16_t port)
{
    if (ip == nullptr || ip[0] == '\0')
    {
        return;
    }
    if (!WiFiManager::instance().isConnected())
    {
        return;
    }
    snprintf(host_ip_, sizeof(host_ip_), "%s", ip);
    host_port_ = port;
    state_ = State::Connecting;
    connecting_since_ms_ = millis();
    LOG_INFO("TCP", "connecting %s:%u ...", ip, static_cast<unsigned>(port));
}

void TcpChannel::stop()
{
    auto *client = static_cast<WiFiClient *>(impl_);
    if (client != nullptr)
    {
        client->stop();
    }
    state_ = State::Idle;
    /* D6 修复：清空行缓冲，断线重连后无残留半行 */
    line_len_ = 0;
    line_overflow_ = false;
    line_buf_[0] = '\0';
}

bool TcpChannel::isConnected() const
{
    if (state_ != State::Connected)
    {
        return false;
    }
    /*
     * WiFiClient::connected() 未声明 const，但语义上只是查询；
     * 这里 const_cast 仅放宽 this 的 const 限定，不修改对象。
     */
    auto *client = const_cast<WiFiClient *>(static_cast<const WiFiClient *>(impl_));
    return client != nullptr && client->connected();
}

const char *TcpChannel::remoteEndpoint(char *buf, size_t cap) const
{
    snprintf(buf, cap, "%s:%u", host_ip_, static_cast<unsigned>(host_port_));
    return buf;
}

void TcpChannel::process()
{
    if (!WiFiManager::instance().isConnected())
    {
        if (state_ != State::Idle)
        {
            LOG_INFO("TCP", "wifi down, channel stopped");
            stop();
        }
        return;
    }

    auto *client = static_cast<WiFiClient *>(impl_);

    switch (state_)
    {
    case State::Idle:
        /* WiFi 在线但无目标：重新发现 */
        DiscoveryService::instance().start();
        break;

    case State::Connecting:
    {
        if (client == nullptr)
        {
            impl_ = new WiFiClient();
            client = static_cast<WiFiClient *>(impl_);
            SerialProtocol::instance().setLineSink(protocolLineSink);
        }
        if (client->connect(host_ip_, host_port_))
        {
            client->setNoDelay(true);
            state_ = State::Connected;
            connected_since_ms_ = millis();
            LOG_INFO("TCP", "connected to %s:%u", host_ip_,
                     static_cast<unsigned>(host_port_));
        }
        else if ((millis() - connecting_since_ms_) >= kTcpConnectTimeoutMs)
        {
            LOG_WARNING("TCP", "connect timeout, rediscover");
            state_ = State::Idle;
            DiscoveryService::instance().stop();
        }
        break;
    }

    case State::Connected:
    {
        if (client == nullptr || !client->connected())
        {
            LOG_WARNING("TCP", "link lost");
            stop();
            last_disconnect_ms_ = millis();
            break;
        }

        /* 收行 → 协议层 */
        while (client->available() > 0)
        {
            char c = static_cast<char>(client->read());
            if (c == '\n')
            {
                if (line_overflow_)
                {
                    line_overflow_ = false;
                    line_len_ = 0;
                    continue;
                }
                line_buf_[line_len_] = '\0';
                SerialProtocol::instance().handleTcpLine(line_buf_);
                line_len_ = 0;
            }
            else if (c == '\r')
            {
                continue;
            }
            else if (line_len_ + 1 >= sizeof(line_buf_))
            {
                line_overflow_ = true;
                LOG_WARNING("TCP", "line overflow discarded");
            }
            else
            {
                line_buf_[line_len_++] = c;
            }
        }
        break;
    }
    }

    /*
     * TCP 15s 未恢复 → 强制重启 WiFi（FEATURE_DOC §7.1）。
     * last_disconnect_ms_ 非零表示处于"等恢复"窗口。
     */
    if (last_disconnect_ms_ != 0 && state_ != State::Connected)
    {
        if ((millis() - last_disconnect_ms_) >= kTcpReviveWifiAfterMs)
        {
            LOG_WARNING("TCP", "no TCP for %us, restarting wifi",
                        static_cast<unsigned>(kTcpReviveWifiAfterMs / 1000));
            last_disconnect_ms_ = 0;
            WiFiManager::instance().scheduleConnect();
        }
    }
}

bool TcpChannel::sendLine(const char *line)
{
    if (state_ != State::Connected)
    {
        return false;
    }
    auto *client = static_cast<WiFiClient *>(impl_);
    if (client == nullptr || !client->connected())
    {
        return false;
    }
    size_t total = strlen(line) + 1;  // 含 '\n'
    size_t written = 0;
    while (written < total)
    {
        int n = client->write(line + written, total - written);
        if (n <= 0)
        {
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

}  // namespace ekeys
