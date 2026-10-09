/*
 * HaApiService.cpp
 *
 * HA Native API 服务层实现（docs/11）。
 * 单客户端假设：新连接到来时先关旧连接再接受。
 * 断开时所有键视为 released（HA 侧 binary_sensor 自动转 unavailable，
 * 无需补偿帧）。
 */

#include "ha/HaApiService.h"

#include <ESPmDNS.h>

#include "ha/HaEntities.h"
#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {

        constexpr const char *kHaDeviceName = "ekeys";
        constexpr const char *kHaServerInfo = "ekeys native api";
        constexpr const char *kHaEsphomeVersion = "1.7.0-ekeys";
        constexpr const char *kHaModel = "EKeys ESP32-S3";

        /* 保活超时：约 120s 无任何帧主动断开，等 HA 重连（docs/11 §3.1） */
        constexpr uint32_t kHaKeepaliveTimeoutMs = 120000;
        /* process() 每轮最多读 socket 次数（保护 RGB 30ms 帧，docs/11 §6） */
        constexpr uint8_t kHaMaxReadsPerProcess = 2;
        /* 单次 read 缓冲：握手期帧小，64B 足够且栈无风险 */
        constexpr size_t kHaReadBufSize = 64;

    } // namespace

    HaApiService &HaApiService::instance()
    {
        static HaApiService svc;
        return svc;
    }

    void HaApiService::start()
    {
        if (mdns_started_)
        {
            return;
        }
        if (!MDNS.begin(kHaDeviceName))
        {
            LOG_ERROR("HA", "mdns begin failed");
            return;
        }
        MDNS.addService("_esphomelib", "_tcp", 6053);
        /* HA zeroconf 按 _esphomelib._tcp. 匹配；TXT 与 ESPHome 惯例对齐 */
        MDNS.addServiceTxt("esphomelib", "tcp", "version", "1.7");
        MDNS.addServiceTxt("esphomelib", "tcp", "platform", "ESP32");
        mdns_started_ = true;
        LOG_INFO("HA", "mdns declared (_esphomelib._tcp 6053)");
    }

    void HaApiService::stop()
    {
        setListening(false);
        if (mdns_started_)
        {
            MDNS.end();
            mdns_started_ = false;
            LOG_INFO("HA", "mdns ended");
        }
    }

    void HaApiService::setListening(bool enable)
    {
        if (enable == listening_)
        {
            return;
        }
        if (enable)
        {
            server_.begin();
            listening_ = true;
            LOG_INFO("HA", "listening on 6053");
        }
        else
        {
            /* 两条路径都完整清理，防 socket/内存泄漏 */
            closeClient();
            server_.end();
            listening_ = false;
            LOG_INFO("HA", "listening stopped");
        }
    }

    void HaApiService::closeClient()
    {
        if (static_cast<bool>(client_))
        {
            client_.stop();
        }
        session_ = SessionState::Idle;
        subscribed_ = false;
        decoder_.reset();
    }

    bool HaApiService::hasClientTcp()
    {
        return static_cast<bool>(client_) && client_.connected();
    }

    void HaApiService::process()
    {
        if (!listening_)
        {
            return;
        }

        /* accept：单客户端，新连接顶掉旧连接（先关旧再接受） */
        WiFiClient candidate = server_.available();
        if (candidate)
        {
            if (hasClientTcp())
            {
                LOG_INFO("HA", "new client replaces old");
                client_.stop();
            }
            client_ = candidate;
            client_.setNoDelay(true);
            session_ = SessionState::Connected;
            subscribed_ = false;
            decoder_.reset();
            last_frame_ms_ = millis();
            LOG_INFO("HA", "client connected");
        }

        if (session_ == SessionState::Idle)
        {
            return;
        }

        /* 客户端断开检测 */
        if (!hasClientTcp() && client_.available() == 0)
        {
            LOG_INFO("HA", "client disconnected");
            closeClient();
            return;
        }

        /* 逐帧解析（每轮最多 2 次 read，剩余下轮） */
        uint8_t read_buf[kHaReadBufSize];
        for (uint8_t round = 0; round < kHaMaxReadsPerProcess; ++round)
        {
            if (!hasClientTcp() || client_.available() == 0)
            {
                break;
            }
            const int n = client_.read(read_buf, sizeof(read_buf));
            if (n <= 0)
            {
                break;
            }
            decoder_.feed(read_buf, static_cast<size_t>(n));
            while (decoder_.hasFrame())
            {
                last_frame_ms_ = millis();
                handleFrame();
                decoder_.consume();
            }
            if (session_ == SessionState::Idle)
            {
                break; /* DisconnectRequest 已关连接 */
            }
        }

        /* 保活超时：等 HA 按自身重试周期回连 */
        if (session_ != SessionState::Idle &&
            (millis() - last_frame_ms_) > kHaKeepaliveTimeoutMs)
        {
            LOG_INFO("HA", "keepalive timeout, closing");
            closeClient();
        }
    }

    void HaApiService::handleFrame()
    {
        switch (decoder_.messageType())
        {
        case ha::HaMsgType::HelloRequest:
            /* 新握手：重置订阅状态（HA 可能重连重建会话） */
            session_ = SessionState::Connected;
            subscribed_ = false;
            sendHelloResponse();
            break;

        case ha::HaMsgType::ConnectRequest:
            /* 未配置密码：任意值通过（校验位已留好，docs/11 §2） */
            sendConnectResponse();
            break;

        case ha::HaMsgType::DisconnectRequest:
            sendMessage(ha::HaMsgType::DisconnectResponse, nullptr, 0);
            closeClient(); /* 回完后关连接 */
            break;

        case ha::HaMsgType::PingRequest:
            sendMessage(ha::HaMsgType::PingResponse, nullptr, 0);
            break;

        case ha::HaMsgType::DeviceInfoRequest:
            sendDeviceInfo();
            break;

        case ha::HaMsgType::ListEntitiesRequest:
            sendEntityList();
            break;

        case ha::HaMsgType::SubscribeStatesRequest:
            subscribed_ = true;
            session_ = SessionState::Ready;
            /* 初始状态帧：missing_state=true（HA 建立订阅后收首帧） */
            for (uint8_t k = ha::kHaEntityKeyFirst; k <= ha::kHaEntityKeyLast; ++k)
            {
                sendBinarySensorState(k, false, true);
            }
            break;

        default:
            break; /* 未实现/未知消息忽略 */
        }
    }

    void HaApiService::sendMessage(uint8_t msg_type, const uint8_t *payload,
                                   size_t payload_len)
    {
        if (!hasClientTcp())
        {
            return;
        }
        /* 帧头 ≤5B + 载荷 <200B，栈对象无风险（docs/11 §6 大栈对象自查） */
        uint8_t frame[ha::kHaMaxFrameSize];
        const size_t total = ha::haFrameEncode(msg_type, payload, payload_len,
                                               frame, sizeof(frame));
        if (total == 0)
        {
            LOG_ERROR("HA", "frame encode failed (type=%u)",
                      static_cast<unsigned>(msg_type));
            return;
        }
        const size_t written = client_.write(frame, total);
        if (written != total)
        {
            /* 写失败即认为连接异常，标记断开下轮清理；不重试不缓冲 */
            LOG_ERROR("HA", "write failed (%u/%u), closing",
                      static_cast<unsigned>(written),
                      static_cast<unsigned>(total));
            closeClient();
        }
    }

    void HaApiService::sendHelloResponse()
    {
        ha::HaProtoWriter w;
        /* api_version 为 fixed32（官方 api.proto），声明 1.7 */
        w.addFixed32Field(1, 1); /* api_version_major */
        w.addFixed32Field(2, 7); /* api_version_minor */
        w.addStringField(3, kHaServerInfo);
        w.addStringField(4, kHaDeviceName);
        sendMessage(ha::HaMsgType::HelloResponse, w.data(), w.size());
    }

    void HaApiService::sendConnectResponse()
    {
        ha::HaProtoWriter w;
        w.addBoolField(1, false); /* invalid_password=false */
        sendMessage(ha::HaMsgType::ConnectResponse, w.data(), w.size());
    }

    void HaApiService::sendDeviceInfo()
    {
        ha::HaProtoWriter w;
        w.addBoolField(1, false); /* uses_password */
        w.addStringField(2, kHaDeviceName);
        w.addStringField(3, WiFi.macAddress().c_str());
        w.addStringField(4, kHaEsphomeVersion);
        /* compilation_time / model / has_deep_sleep */
        w.addStringField(5, __DATE__ " " __TIME__);
        w.addStringField(6, kHaModel);
        w.addBoolField(7, false);
        sendMessage(ha::HaMsgType::DeviceInfoResponse, w.data(), w.size());
    }

    void HaApiService::sendEntityList()
    {
        for (uint8_t i = 0; i < ha::kHaEntityCount; ++i)
        {
            const ha::HaEntityDesc &e = ha::kHaEntities[i];
            ha::HaProtoWriter w;
            w.addStringField(1, e.object_id);
            w.addFixed32Field(2, e.key_id);
            w.addStringField(3, e.name);
            w.addStringField(4, e.unique_id);
            sendMessage(ha::HaMsgType::ListEntitiesBinarySensorResponse,
                        w.data(), w.size());
            if (!hasClientTcp())
            {
                return; /* 写失败已关连接 */
            }
        }
        /* 空 protobuf 消息：载荷长度 0（DisconnectResponse / ListEntitiesDoneResponse 同型） */
        sendMessage(ha::HaMsgType::ListEntitiesDoneResponse, nullptr, 0);
    }

    void HaApiService::sendBinarySensorState(uint8_t key_id, bool state,
                                             bool missing_state)
    {
        ha::HaProtoWriter w;
        w.addFixed32Field(1, key_id);
        w.addBoolField(2, state);
        if (missing_state)
        {
            w.addBoolField(3, true);
        }
        sendMessage(ha::HaMsgType::BinarySensorStateResponse, w.data(), w.size());
    }

    void HaApiService::sendKeyState(uint8_t key_id, bool pressed)
    {
        if (session_ != SessionState::Ready || !subscribed_)
        {
            return; /* 就绪前的状态类事件直接忽略 */
        }
        if (key_id < ha::kHaEntityKeyFirst || key_id > ha::kHaEntityKeyLast)
        {
            return;
        }
        sendBinarySensorState(key_id, pressed, false);
    }

} // namespace ekeys
