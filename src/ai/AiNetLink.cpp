/*
 * AiNetLink.cpp
 *
 * 见 AiNetLink.h。WiFiServer/WiFiClient 以 void* 惰性持有，实现在此处
 * 展开（避免头文件把 WiFi.h 拖进所有 include 方）。
 */
#include "AiNetLink.h"

#include <Arduino.h>
#include <WiFi.h>

#include "AiFrameProtocol.h"
#include "logging/LogManager.h"
#include "network/WiFiManager.h"

namespace ekeys
{
    namespace ai
    {

        bool AiNetLink::start()
        {
            if (listening_)
            {
                return true;
            }
            /* 前置条件：WiFiManager::isEnabled() 且已连接（docs/13 §4.3），
             * 不满足时 AI 页显示 WIFI OFF，不做隐式改配置 */
            if (!WiFiManager::instance().isEnabled() ||
                !WiFiManager::instance().isConnected())
            {
                return false;
            }

            auto *srv = new WiFiServer(kAiTcpPort);
            srv->begin();
            srv->setNoDelay(true);
            server_ = srv;
            client_ = nullptr;
            client_just_connected_ = false;
            listening_ = true;
            LOG_INFO("AI", "tcp server listening on %u", kAiTcpPort);
            return true;
        }

        void AiNetLink::stop()
        {
            if (!listening_)
            {
                return;
            }
            auto *c = static_cast<WiFiClient *>(client_);
            if (c != nullptr)
            {
                c->stop();
                delete c;
                client_ = nullptr;
            }
            auto *srv = static_cast<WiFiServer *>(server_);
            if (srv != nullptr)
            {
                srv->stop();
                delete srv;
                server_ = nullptr;
            }
            listening_ = false;
            client_just_connected_ = false;
            LOG_INFO("AI", "tcp server stopped");
        }

        bool AiNetLink::clientConnected() const
        {
            auto *c = static_cast<WiFiClient *>(client_);
            return c != nullptr && c->connected();
        }

        bool AiNetLink::takeClientJustConnected()
        {
            const bool v = client_just_connected_;
            client_just_connected_ = false;
            return v;
        }

        void AiNetLink::poll()
        {
            if (!listening_ || protocol_ == nullptr)
            {
                return;
            }
            auto *srv = static_cast<WiFiServer *>(server_);

            /* accept：仅 1 客户端，新连接踢掉旧连接 */
            WiFiClient fresh = srv->available();
            if (fresh)
            {
                auto *c = static_cast<WiFiClient *>(client_);
                if (c != nullptr)
                {
                    c->stop();
                    delete c;
                }
                client_ = new WiFiClient(fresh);
                client_just_connected_ = true;
                LOG_INFO("AI", "tcp client connected");
            }

            auto *c = static_cast<WiFiClient *>(client_);
            if (c == nullptr || !c->connected())
            {
                return;
            }

            /* 读流 → 帧解析器（专用流，无 '\n' 门控；HELLO 由 AiMode
             * 检测 takeClientJustConnected() 后发送）。批量读（512B/次）
             * 避免逐字节 read 的 syscall 开销成为吞吐瓶颈。 */
            uint8_t rbuf[512];
            for (;;)
            {
                const int n = c->read(rbuf, sizeof(rbuf));
                if (n <= 0)
                {
                    break;
                }
                for (int i = 0; i < n; ++i)
                {
                    protocol_->pushByte(rbuf[i]);
                }
            }

            /* 半帧超时 → 复位解析器（等待下一 magic 重新同步） */
            if (protocol_->inFrame() && protocol_->lastByteMs() != 0 &&
                (millis() - protocol_->lastByteMs()) >= kAiHalfFrameTimeoutMs)
            {
                LOG_WARNING("AI", "tcp half-frame timeout, resync");
                protocol_->reset();
            }
        }

        void AiNetLink::sendLine(const char *line)
        {
            auto *c = static_cast<WiFiClient *>(client_);
            if (c != nullptr && c->connected())
            {
                c->println(line);
            }
        }

    } // namespace ai
} // namespace ekeys
