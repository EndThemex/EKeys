/*
 * WiFiManager.h
 *
 * WiFi STA 连接状态机（FEATURE_DOC §7.1，阶段 06 任务 6.1）。
 *
 *   - 重连策略：每 5s 重试一次，单次连接超时 10s
 *   - BLE 模式禁止开启（stopReconnect 强制回到 Idle）
 *   - 连上后触发 NTP 同步 + UDP 自动发现（经回调注入，避免循环依赖）
 *
 * 由 MainTask::loop() 周期调用 process()；WiFi 事件不使用中断回调。
 */

#ifndef EKEYS_NETWORK_WIFI_MANAGER_H
#define EKEYS_NETWORK_WIFI_MANAGER_H

#include <stdint.h>

namespace ekeys {

class WiFiManager {
public:
    static WiFiManager &instance();

    WiFiManager(const WiFiManager &) = delete;
    WiFiManager &operator=(const WiFiManager &) = delete;

    void begin();

    /*
     * 请求连接（wifi_switch=1 且非 BLE 模式才真正调度）。
     * 配置变更 / TCP 断线 15s / 启动时调用。
     */
    void scheduleConnect();

    /* 停止重连并断开（BLE 模式 / wifi_switch=0） */
    void stopReconnect();

    /* MainTask::loop() 周期驱动 */
    void process();

    bool isConnected() const;
    /* 当前配置是否允许开启 WiFi（wifi_switch 且非 BLE 模式） */
    bool isEnabled() const;
    /* 已连接时的 RSSI（dBm，未连接返回 0） */
    int rssi() const;

    void setOnConnected(void (*cb)()) { on_connected_ = cb; }

private:
    WiFiManager() = default;

    enum class State : uint8_t {
        Idle = 0,
        Connecting = 1,
        Connected = 2,
        WaitingRetry = 3,
    };

    void startConnect(uint32_t now);

    State state_ = State::Idle;
    uint32_t state_entered_ms_ = 0;
    uint32_t last_connect_attempt_ms_ = 0;
    uint32_t last_request_serial_ = 0;  // scheduleConnect 去重（配置 seq）
    void (*on_connected_)() = nullptr;
    bool logged_fail_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_NETWORK_WIFI_MANAGER_H
