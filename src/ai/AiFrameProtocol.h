/*
 * AiFrameProtocol.h
 *
 * AI Mode 二进制帧协议（docs/13 §4.2）：帧头/分片解析、CRC16/CRC32、
 * 帧序回绕检测、状态机与 ACK 组装回调。
 *
 * 通道无关：USB CDC 与 WiFi TCP 各持有一个解析器实例（可靠有序流，
 * 分片严格按 chunk_index 递增到达，不做乱序重组）。
 *
 * 半帧超时（≥300ms 无新字节 → 复位）由链路层（AiSerialLink/AiNetLink）
 * 通过 lastByteMs() 检测后调用 reset()。
 */
#ifndef EKEYS_AI_AI_FRAME_PROTOCOL_H
#define EKEYS_AI_AI_FRAME_PROTOCOL_H

#include <stdint.h>

#include "AiConfig.h"

namespace ekeys
{
    namespace ai
    {

        /*
         * 帧头 24 字节（小端，docs/13 §4.2）：
         *   u16 magic=0x5AA5（线序 A5 5A）| u8 version=1 | u8 type
         *   u16 frame_id | u16 width | u16 height | u8 flags | u8 reserved
         *   u32 payload_len | u32 frame_crc32 | u16 chunk_payload | u16 header_crc16
         */
        struct AiFrameHeader
        {
            uint16_t magic;
            uint8_t version;
            uint8_t type;
            uint16_t frame_id;
            uint16_t width;
            uint16_t height;
            uint8_t flags;
            uint8_t reserved;
            uint32_t payload_len;
            uint32_t frame_crc32;
            uint16_t chunk_payload;
            uint16_t header_crc16;
        } __attribute__((packed));

        /* 分片头 8 字节（小端） */
        struct AiChunkHeader
        {
            uint16_t frame_id;
            uint16_t chunk_index;
            uint16_t chunk_len;
            uint16_t chunk_crc16;
        } __attribute__((packed));

        /* flags bit：JPEG 按 1/2 输出（发送端 856×284 → 设备 428×142） */
        inline constexpr uint8_t kAiFlagScaleHalf = 0x01;
        inline constexpr uint8_t kAiFlagCrc32 = 0x02;

        /*
         * 解析器把负载写到哪、结果回给谁：
         *   beginFrame：帧头校验通过、负载开始前调用。返回负载写入缓冲；
         *               nullptr = 拒收（解析器只消费不写入），*out_result 给出原因。
         *   frameDone：负载收齐且传输层 CRC 通过。ok=false 时（CRC 失败）
         *               sink 需回收 beginFrame 给出的缓冲。
         *   sendCtl  ：传输层结果上报（ACK/NAK），由链路层转成 ai_ctl JSON 行。
         */
        struct AiFrameSink
        {
            void *user{nullptr};
            uint8_t *(*beginFrame)(void *user, uint8_t type, uint32_t payload_len,
                                   uint16_t w, uint16_t h, uint8_t flags,
                                   uint16_t frame_id, uint8_t *out_result){nullptr};
            void (*frameDone)(void *user, uint8_t type, uint16_t frame_id,
                              bool ok){nullptr};
            void (*sendCtl)(void *user, uint8_t result, uint16_t frame_id){nullptr};
        };

        class AiFrameProtocol
        {
        public:
            void setSink(const AiFrameSink &sink) { sink_ = sink; }

            /* 复位到 Idle（扫描 magic），等待下一帧 */
            void reset();

            /* 单字节喂入（MainTask 上下文，非阻塞） */
            void pushByte(uint8_t b);

            /* 最近一次收到字节时刻（millis()），链路层据此判半帧超时 */
            uint32_t lastByteMs() const { return last_byte_ms_; }

            /* 累计接收负载字节（流控/统计用，单调递增） */
            uint32_t totalPayloadBytes() const { return total_payload_bytes_; }

            bool inFrame() const { return state_ != State::Idle; }

        private:
            enum class State : uint8_t
            {
                Idle,        /* 扫描 magic A5 */
                Magic2,      /* 已收 A5，等 5A */
                Header,      /* 收帧头剩余 22 字节 */
                ChunkHeader, /* 收分片头 8 字节 */
                ChunkData,   /* 收分片负载 */
                CtlPayload,  /* 控制帧负载 12 字节 */
            };

            bool validateHeader();
            void startFrame();
            void abortFrame(uint8_t result);
            void completeFrame();
            void handleControlFrame();

            AiFrameSink sink_{};
            State state_{State::Idle};

            uint8_t header_buf_[sizeof(AiFrameHeader)];
            uint8_t header_got_{0};
            uint8_t chunk_buf_[sizeof(AiChunkHeader)];
            uint8_t chunk_got_{0};

            AiFrameHeader hdr_{};
            uint8_t *dest_{nullptr}; /* nullptr = 拒收后只消费字节 */
            uint8_t ctl_payload_[kAiCtlPayloadBytes];

            uint16_t expected_chunk_{0};
            uint32_t payload_written_{0};
            uint16_t current_chunk_len_{0};  /* 本片期望长度 */
            uint16_t chunk_got_data_{0};     /* 本片已收数据字节数 */
            uint16_t chunk_expect_crc16_{0}; /* 本片期望 CRC16（0=不校验） */
            uint8_t header_result_{0};       /* validateHeader 拒收原因 */
            bool header_valid_{false};       /* 帧头已通过校验（NACK 可信） */

            /* frame_id 回绕序号检测（docs/13 §4.2） */
            bool last_seen_valid_{false};
            uint16_t last_seen_fid_{0};

            uint32_t last_byte_ms_{0};
            uint32_t total_payload_bytes_{0};
        };

        /* CRC16-CCITT（初值 0xFFFF，poly 0x1021，与发送端参考实现一致） */
        uint16_t ai_crc16(const uint8_t *data, uint32_t len);
        /* CRC32（IEEE/zlib 多项式，与 zlib.crc32 一致） */
        uint32_t ai_crc32(const uint8_t *data, uint32_t len);

    } // namespace ai
} // namespace ekeys

#endif // EKEYS_AI_AI_FRAME_PROTOCOL_H
