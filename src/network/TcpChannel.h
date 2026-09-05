/*
 * TcpChannel.h
 *
 * TCP 控制通道客户端（FEATURE_DOC §5.1/§7.3，阶段 06 任务 6.3）。
 *
 *   - 主动连接桌面 App 的 30000 端口（IP 来自 DiscoveryService）
 *   - 复用 SerialProtocol 的 JSON 行解析（收到的行喂给
 *     SerialProtocol::handleTcpLine；发送经 SerialProtocol 的 sink 钩子）
 *   - 断线后经 DiscoveryService 重新发现；TCP 15s 未恢复则强制重启 WiFi
 */

#ifndef EKEYS_NETWORK_TCP_CHANNEL_H
#define EKEYS_NETWORK_TCP_CHANNEL_H

#include <stddef.h>
#include <stdint.h>

namespace ekeys {

class TcpChannel {
public:
    static TcpChannel &instance();

    TcpChannel(const TcpChannel &) = delete;
    TcpChannel &operator=(const TcpChannel &) = delete;

    /* 由 DiscoveryService 回调：拿到 App IP 后连接 */
    void connectTo(const char *ip, uint16_t port = 30000);

    /* 断开（WiFi 掉线 / BLE 模式） */
    void stop();

    /* MainTask::loop() 周期驱动：收行 + 断线重连 + 15s 超时重启 WiFi */
    void process();

    bool isConnected() const;

    /* 发送一行 JSON（结尾自动补 '\n'）；返回是否完整写出 */
    bool sendLine(const char *line);

    /* 供 NetDiagnostics / HA 屏 */
    const char *remoteEndpoint(char *buf, size_t cap) const;

private:
    TcpChannel() = default;

    enum class State : uint8_t {
        Idle = 0,
        Connecting = 1,
        Connected = 2,
    };

    State state_ = State::Idle;
    uint32_t connecting_since_ms_ = 0;
    uint32_t connected_since_ms_ = 0;
    uint32_t last_disconnect_ms_ = 0;
    char host_ip_[16]{0};
    uint16_t host_port_ = 30000;
    void *impl_ = nullptr;  // WiFiClient*（避免在头文件引 WiFi.h）

    /*
     * D6 修复：行缓冲提为成员变量，stop() 中重置。
     * 旧实现用函数级 static，断线重连时残留半行可能拼接到新连接的首行触发解析异常。
     */
    static constexpr size_t kTcpLineBufSize = 2048;
    char line_buf_[kTcpLineBufSize]{};
    size_t line_len_ = 0;
    bool line_overflow_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_NETWORK_TCP_CHANNEL_H
