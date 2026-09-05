/*
 * DiscoveryService.cpp
 *
 * 见 DiscoveryService.h。
 */

#include "DiscoveryService.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "config/Configuration.h"
#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {

        constexpr uint16_t kDiscoveryPort = 30001;
        constexpr const char *kProbeMsg = "FUNKEYBOARD_DISCOVER";
        constexpr const char *kReplyMsg = "FUNKEYBOARD_HERE";
        constexpr uint32_t kProbeTimeoutMs = 800; // FEATURE_DOC §5.2
        constexpr uint32_t kMaxProbeRounds = 3;   // 最多 3 轮，之后回退固定 IP
        constexpr const char *kFallbackIp = "192.168.31.1";
        constexpr uint8_t kWorkModeBluetooth = 1;

        WiFiUDP *s_udp = nullptr;

    } // namespace

    DiscoveryService &DiscoveryService::instance()
    {
        static DiscoveryService inst;
        return inst;
    }

    void DiscoveryService::start()
    {
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        if (snap.connect_host == 0 || snap.work_mode == kWorkModeBluetooth)
        {
            LOG_INFO("DISC", "connect_host=0 or BLE mode, discovery skipped");
            return;
        }
        if (state_ == State::Probing)
        {
            return;
        }

        if (s_udp == nullptr)
        {
            s_udp = new WiFiUDP();
        }
        if (!s_udp->begin(kDiscoveryPort))
        {
            LOG_ERROR("DISC", "udp bind %u failed", static_cast<unsigned>(kDiscoveryPort));
            delete s_udp;
            s_udp = nullptr;
            return;
        }
        state_ = State::Probing;
        probe_count_ = 0;
        probe_started_ms_ = 0;
        LOG_INFO("DISC", "discovery started (port %u)", static_cast<unsigned>(kDiscoveryPort));
    }

    void DiscoveryService::stop()
    {
        if (s_udp != nullptr)
        {
            s_udp->stop();
            delete s_udp;
            s_udp = nullptr;
        }
        state_ = State::Idle;
    }

    void DiscoveryService::sendProbe()
    {
        if (s_udp == nullptr)
        {
            return;
        }
        s_udp->beginPacket(IPAddress(255, 255, 255, 255), kDiscoveryPort);
        s_udp->write(reinterpret_cast<const uint8_t *>(kProbeMsg), strlen(kProbeMsg));
        s_udp->endPacket();
        probe_started_ms_ = millis();
        ++probe_count_;
    }

    void DiscoveryService::finishWith(const char *ip)
    {
        snprintf(discovered_ip_, sizeof(discovered_ip_), "%s", ip);
        state_ = State::Finished;
        LOG_INFO("DISC", "host at %s", ip);
        if (on_discovered_ != nullptr)
        {
            on_discovered_(discovered_ip_);
        }
    }

    void DiscoveryService::process()
    {
        if (state_ != State::Probing || s_udp == nullptr)
        {
            return;
        }

        const uint32_t now = millis();

        /* 收应答 */
        char packet[64];
        int len = s_udp->parsePacket();
        if (len > 0)
        {
            int n = s_udp->read(packet, sizeof(packet) - 1);
            if (n > 0)
            {
                packet[n] = '\0';
                if (strncmp(packet, kReplyMsg, strlen(kReplyMsg)) == 0)
                {
                    finishWith(s_udp->remoteIP().toString().c_str());
                    return;
                }
            }
        }

        /* 发送 / 超时 */
        if (probe_started_ms_ == 0 ||
            (now - probe_started_ms_) >= kProbeTimeoutMs)
        {
            if (probe_count_ >= kMaxProbeRounds)
            {
                LOG_WARNING("DISC", "no reply after %u rounds, fallback to %s",
                            static_cast<unsigned>(probe_count_), kFallbackIp);
                finishWith(kFallbackIp);
                return;
            }
            sendProbe();
        }
    }

} // namespace ekeys
