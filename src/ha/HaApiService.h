/*
 * HaApiService.h
 *
 * HA Native API 服务层（docs/11 §1/§3）：
 *
 *   - mDNS 声明：WiFi 已连接即声明一次（_esphomelib._tcp 端口 6053），
 *     仅广播"设备存在"，不随屏幕切换反复 begin/end
 *   - TCP 监听 + 连接：仅在活动屏为 HA 屏（UI_SCREEN_HA / HA_SECONDARY）
 *     且 WiFi 已连时监听 6053 并接受 HA 连接；离开 HA 屏立即断开并停监听
 *   - 协议：ESPHome 明文协议 v1 子集，对外声明 api_version 1.7
 *   - 按键路由：MainTask 5ms tick 调 sendKeyState(key_id, pressed)
 *
 * 由 MainTask::tick() 周期驱动 process()（accept / 逐帧解析 / 超时检测）。
 * 全程非阻塞（available()/connected() 轮询），保护 RGB 30ms 帧：
 * sendKeyState 写失败直接丢弃不重试，process() 每轮最多读 2 次 socket。
 */

#ifndef EKEYS_HA_HA_API_SERVICE_H
#define EKEYS_HA_HA_API_SERVICE_H

#include <WiFi.h>

#include <stdint.h>

#include "ha/HaApiProto.h"

namespace ekeys
{

    class HaApiService
    {
    public:
        static HaApiService &instance();

        HaApiService(const HaApiService &) = delete;
        HaApiService &operator=(const HaApiService &) = delete;

        /* WiFi 连上后调用（WiFiManager on_connected 回调）：mDNS 声明，幂等 */
        void start();

        /* WiFi 断开时调用：关监听/客户端 + MDNS.end 善后，幂等 */
        void stop();

        /*
         * HA 屏进入/离开边沿：listen 6053 / 断开连接并停监听。
         * 重复传入相同值无副作用；两条路径都做完整清理（client.stop +
         * server.end）防 socket 泄漏，状态机统一回 Idle。
         */
        void setListening(bool enable);
        bool listening() const { return listening_; }

        /* MainTask::tick() 驱动：accept / 逐帧解析 / 保活超时检测 */
        void process();

        /*
         * 按键边沿 → BinarySensorStateResponse（MainTask 上下文，非阻塞）。
         * 未连上 HA（或未完成 SubscribeStates）时丢弃——按键是边沿事件，
         * HA 重连后以 missing_state 初始帧恢复，无需本地缓冲。
         */
        void sendKeyState(uint8_t key_id, bool pressed);

        /* HA 屏状态显示：已接受 HA 客户端 TCP 连接（握手进行中也算） */
        bool isClientConnected() const { return session_ != SessionState::Idle; }

    private:
        HaApiService() = default;

        /* 会话状态：Idle=无客户端，Connected=已握手 Hello，Ready=已订阅状态 */
        enum class SessionState : uint8_t
        {
            Idle = 0,
            Connected = 1,
            Ready = 2,
        };

        void closeClient();
        /* 注意：WiFiClient::connected() 非 const，本方法不可加 const */
        bool hasClientTcp();
        void handleFrame();
        void sendMessage(uint8_t msg_type, const uint8_t *payload, size_t payload_len);
        void sendHelloResponse();
        void sendConnectResponse();
        void sendDeviceInfo();
        void sendEntityList();
        void sendBinarySensorState(uint8_t key_id, bool state, bool missing_state);

        WiFiServer server_{6053};
        WiFiClient client_;
        ha::HaFrameDecoder decoder_;

        bool mdns_started_{false};
        bool listening_{false};
        SessionState session_{SessionState::Idle};
        bool subscribed_{false};
        uint32_t last_frame_ms_{0}; /* 保活：约 120s 无任何帧主动断开 */
    };

} // namespace ekeys

#endif // EKEYS_HA_HA_API_SERVICE_H
