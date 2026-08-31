/*
 * DiscoveryService.h
 *
 * UDP 自动发现（FEATURE_DOC §5.2，阶段 06 任务 6.3）。
 *
 *   - 设备在 UDP 30001 广播 "FUNKEYBOARD_DISCOVER"
 *   - 桌面 App 应答 "FUNKEYBOARD_HERE"
 *   - 超时 800ms 未收到应答则回退 IP 192.168.31.1
 *   - 拿到 IP 后回调注入的 on_discovered(ip)（→ TcpChannel::connectTo）
 */

#ifndef EKEYS_NETWORK_DISCOVERY_SERVICE_H
#define EKEYS_NETWORK_DISCOVERY_SERVICE_H

#include <stdint.h>

namespace ekeys {

class DiscoveryService {
public:
    static DiscoveryService &instance();

    DiscoveryService(const DiscoveryService &) = delete;
    DiscoveryService &operator=(const DiscoveryService &) = delete;

    /* WiFi 连上后调用（connect_host=1 才实际启动） */
    void start();

    /* 停止（WiFi 断开 / BLE 模式） */
    void stop();

    /* MainTask::loop() 周期驱动 */
    void process();

    void setOnDiscovered(void (*cb)(const char *ip)) { on_discovered_ = cb; }

private:
    DiscoveryService() = default;

    enum class State : uint8_t {
        Idle = 0,
        Probing = 1,      // 已发广播，等应答 / 超时
        Finished = 2,     // 已得到 IP（或已回退）
    };

    void sendProbe();
    void finishWith(const char *ip);

    State state_ = State::Idle;
    uint32_t probe_started_ms_ = 0;
    uint32_t probe_count_ = 0;
    void (*on_discovered_)(const char *ip) = nullptr;
    char discovered_ip_[16]{0};
};

}  // namespace ekeys

#endif  // EKEYS_NETWORK_DISCOVERY_SERVICE_H
