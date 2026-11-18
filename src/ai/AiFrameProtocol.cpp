/*
 * AiFrameProtocol.cpp
 *
 * 见 AiFrameProtocol.h。字节流状态机：
 *
 *   Idle --A5--> Magic2 --5A--> Header(22B) --校验过--> [type=4: CtlPayload]
 *                                        |            [type=1/2/3: ChunkHeader/Data ×N]
 *                                        +--校验失败--> reset（回 Idle 重新同步）
 *
 * 任一 CRC 失败 / 负载超限 / 序号跳变 → abortFrame()：回 NACK 并复位解析器，
 * 等下一个 magic 重新同步（docs/13 §4.2）。
 */
#include "AiFrameProtocol.h"

#include <Arduino.h>
#include <string.h>

#include "logging/LogManager.h"

namespace ekeys
{
    namespace ai
    {

        namespace
        {
            constexpr uint16_t kMagic = 0x5AA5;
            constexpr uint8_t kMagicFirst = 0xA5; /* 线序：低字节在前 */
            constexpr uint8_t kMagicSecond = 0x5A;
            constexpr uint8_t kVersion = 1;
            constexpr uint32_t kFrameBytes =
                428u * 142u * 2u; /* 全屏 RGB565 帧固定大小 */
        } // namespace

        /* ---------------- CRC ---------------- */

        uint16_t ai_crc16(const uint8_t *data, uint32_t len)
        {
            uint16_t crc = 0xFFFF;
            for (uint32_t i = 0; i < len; ++i)
            {
                crc ^= static_cast<uint16_t>(data[i]) << 8;
                for (int b = 0; b < 8; ++b)
                {
                    crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                         : static_cast<uint16_t>(crc << 1);
                }
            }
            return crc;
        }

        uint32_t ai_crc32(const uint8_t *data, uint32_t len)
        {
            static uint32_t table[256];
            static bool ready = false;
            if (!ready)
            {
                for (uint32_t i = 0; i < 256; ++i)
                {
                    uint32_t c = i;
                    for (int b = 0; b < 8; ++b)
                    {
                        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                    }
                    table[i] = c;
                }
                ready = true;
            }
            uint32_t crc = 0xFFFFFFFFu;
            for (uint32_t i = 0; i < len; ++i)
            {
                crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
            }
            return crc ^ 0xFFFFFFFFu;
        }

        /* ---------------- 解析器 ---------------- */

        void AiFrameProtocol::reset()
        {
            state_ = State::Idle;
            header_got_ = 0;
            chunk_got_ = 0;
            chunk_got_data_ = 0;
            current_chunk_len_ = 0;
            dest_ = nullptr;
            expected_chunk_ = 0;
            payload_written_ = 0;
            header_valid_ = false;
        }

        void AiFrameProtocol::pushByte(uint8_t b)
        {
            last_byte_ms_ = millis();

            switch (state_)
            {
            case State::Idle:
                /* 扫描 magic：串口链路已有 '\n'+A5 门控，TCP 直接喂也可容错 */
                if (b == kMagicFirst)
                {
                    header_buf_[0] = b;
                    header_got_ = 1;
                    state_ = State::Magic2;
                }
                break;

            case State::Magic2:
                if (b == kMagicSecond)
                {
                    header_buf_[1] = b;
                    header_got_ = 2;
                    state_ = State::Header;
                }
                else
                {
                    /* 伪 magic：回 Idle 重新扫描 */
                    state_ = State::Idle;
                }
                break;

            case State::Header:
                header_buf_[header_got_++] = b;
                if (header_got_ >= sizeof(header_buf_))
                {
                    memcpy(&hdr_, header_buf_, sizeof(hdr_));
                    if (validateHeader())
                    {
                        startFrame();
                    }
                    else
                    {
                        abortFrame(header_result_);
                    }
                }
                break;

            case State::ChunkHeader:
                chunk_buf_[chunk_got_++] = b;
                if (chunk_got_ >= sizeof(chunk_buf_))
                {
                    AiChunkHeader ch;
                    memcpy(&ch, chunk_buf_, sizeof(ch));
                    chunk_got_ = 0;

                    const bool idx_ok = (ch.frame_id == hdr_.frame_id &&
                                         ch.chunk_index == expected_chunk_);
                    const uint32_t remain = hdr_.payload_len - payload_written_;
                    const bool len_ok = (ch.chunk_len > 0 &&
                                         ch.chunk_len <= hdr_.chunk_payload &&
                                         ch.chunk_len <= remain);
                    if (!idx_ok || !len_ok)
                    {
                        LOG_WARNING("AI", "chunk bad: idx=%u exp=%u len=%u remain=%u",
                                    static_cast<unsigned>(ch.chunk_index),
                                    static_cast<unsigned>(expected_chunk_),
                                    static_cast<unsigned>(ch.chunk_len),
                                    static_cast<unsigned>(remain));
                        abortFrame(kAiResultCrc);
                        break;
                    }
                    current_chunk_len_ = ch.chunk_len;
                    chunk_got_data_ = 0;
                    /* chunk_crc16 期望值暂存（0 = 不校验） */
                    chunk_expect_crc16_ = ch.chunk_crc16;
                    state_ = State::ChunkData;
                }
                break;

            case State::ChunkData:
                if (dest_ != nullptr)
                {
                    dest_[payload_written_] = b;
                }
                ++payload_written_;
                ++total_payload_bytes_;
                ++chunk_got_data_;

                if (chunk_got_data_ >= current_chunk_len_)
                {
                    /* 本片收满：验证片 CRC（若启用） */
                    if (dest_ != nullptr && chunk_expect_crc16_ != 0)
                    {
                        const uint32_t off = payload_written_ - chunk_got_data_;
                        const uint16_t crc16 = ai_crc16(dest_ + off, chunk_got_data_);
                        if (crc16 != chunk_expect_crc16_)
                        {
                            LOG_WARNING("AI", "chunk crc16 mismatch");
                            abortFrame(kAiResultCrc);
                            break;
                        }
                    }
                    ++expected_chunk_;
                    chunk_got_data_ = 0;

                    /* 负载已收满：本片即最后一片（docs/13 §4.2）。
                     * 必须先判满再切状态，否则 state_ 已变 ChunkHeader，
                     * 帧永远不收尾 → 不回 ACK，后续字节被当分片头解析。 */
                    if (payload_written_ >= hdr_.payload_len)
                    {
                        completeFrame();
                        break;
                    }
                    state_ = State::ChunkHeader;
                }
                break;

            case State::CtlPayload:
                ctl_payload_[payload_written_++] = b;
                if (payload_written_ >= hdr_.payload_len)
                {
                    handleControlFrame();
                }
                break;
            }
        }

        bool AiFrameProtocol::validateHeader()
        {
            header_result_ = kAiResultCrc;

            /* header_crc16 覆盖前 22 字节 */
            const uint16_t calc = ai_crc16(header_buf_, 22);
            if (calc != hdr_.header_crc16)
            {
                LOG_WARNING("AI", "header crc16 mismatch");
                return false;
            }
            if (hdr_.magic != kMagic || hdr_.version != kVersion)
            {
                return false;
            }

            if (hdr_.type == kAiTypeControl)
            {
                return hdr_.payload_len == kAiCtlPayloadBytes;
            }
            if (hdr_.type != kAiTypeJpeg && hdr_.type != kAiTypeRgb565 &&
                hdr_.type != kAiTypeRle565)
            {
                LOG_WARNING("AI", "bad frame type %u", static_cast<unsigned>(hdr_.type));
                return false;
            }

            /* 负载上限按 type 区分（docs/13 §4.2）：RGB565 直写帧槽走 128KB 限 */
            const uint32_t limit = (hdr_.type == kAiTypeRgb565)
                                       ? kAiMaxRawPayloadBytes
                                       : kAiMaxPayloadBytes;
            if (hdr_.payload_len == 0 || hdr_.payload_len > limit)
            {
                LOG_WARNING("AI", "payload over limit: %u (type %u)",
                            static_cast<unsigned>(hdr_.payload_len),
                            static_cast<unsigned>(hdr_.type));
                header_result_ = kAiResultSize;
                return false;
            }
            if (hdr_.type == kAiTypeRgb565 && hdr_.payload_len > kFrameBytes)
            {
                header_result_ = kAiResultSize;
                return false;
            }

            /* 尺寸适配（docs/13 §5.4）：JPEG 可 856×284 + scale_half，其余必须 428×142 */
            const bool half = (hdr_.flags & kAiFlagScaleHalf) != 0;
            const bool size_ok =
                (hdr_.width == 428 && hdr_.height == 142) ||
                (hdr_.type == kAiTypeJpeg && half &&
                 hdr_.width == 856 && hdr_.height == 284);
            if (!size_ok)
            {
                LOG_WARNING("AI", "frame size %ux%u != 428x142",
                            static_cast<unsigned>(hdr_.width),
                            static_cast<unsigned>(hdr_.height));
                header_result_ = kAiResultSize;
                return false;
            }

            /* chunk_payload 上限 */
            if (hdr_.chunk_payload == 0 || hdr_.chunk_payload > kAiChunkLenMax)
            {
                header_result_ = kAiResultSize;
                return false;
            }

            /* frame_id 回绕序号检测：diff = new - last，仅 0（重发）/1 接受。
             * 更新 last_seen_ 放在判定之后：跳变帧拒收但不阻塞后续帧。 */
            if (last_seen_valid_)
            {
                const uint16_t diff =
                    static_cast<uint16_t>(hdr_.frame_id - last_seen_fid_);
                if (diff != 0 && diff != 1)
                {
                    LOG_WARNING("AI", "frame_id jump %u -> %u, dropped",
                                static_cast<unsigned>(last_seen_fid_),
                                static_cast<unsigned>(hdr_.frame_id));
                    header_result_ = kAiResultSize;
                    last_seen_fid_ = hdr_.frame_id;
                    return false;
                }
            }
            last_seen_fid_ = hdr_.frame_id;
            last_seen_valid_ = true;
            return true;
        }

        void AiFrameProtocol::startFrame()
        {
            header_valid_ = true;

            if (hdr_.type == kAiTypeControl)
            {
                payload_written_ = 0;
                state_ = State::CtlPayload;
                return;
            }

            uint8_t result = kAiResultOk;
            dest_ = (sink_.beginFrame != nullptr)
                        ? sink_.beginFrame(sink_.user, hdr_.type, hdr_.payload_len,
                                           hdr_.width, hdr_.height, hdr_.flags,
                                           hdr_.frame_id, &result)
                        : nullptr;
            if (dest_ == nullptr)
            {
                if (result == kAiResultOk)
                {
                    result = kAiResultBusy; /* sink 缺失按 busy 处理 */
                }
                LOG_INFO("AI", "frame %u rejected (result=%u)",
                         static_cast<unsigned>(hdr_.frame_id),
                         static_cast<unsigned>(result));
                if (sink_.sendCtl != nullptr)
                {
                    sink_.sendCtl(sink_.user, result, hdr_.frame_id);
                }
                /* dest_ == nullptr：保持 ChunkHeader/Data 状态只消费不写入，
                 * 防止后续字节流错位 */
            }

            expected_chunk_ = 0;
            payload_written_ = 0;
            chunk_got_ = 0;
            chunk_got_data_ = 0;
            state_ = State::ChunkHeader;
        }

        void AiFrameProtocol::abortFrame(uint8_t result)
        {
            const bool had_dest = (dest_ != nullptr);
            LOG_WARNING("AI", "frame aborted (result=%u, got %u/%u bytes)",
                        static_cast<unsigned>(result),
                        static_cast<unsigned>(payload_written_),
                        static_cast<unsigned>(hdr_.payload_len));
            /* header 校验失败时 frame_id 不可信，不回 NACK（避免误导发送端） */
            if (header_valid_ && sink_.sendCtl != nullptr)
            {
                sink_.sendCtl(sink_.user, result, hdr_.frame_id);
            }
            if (had_dest && sink_.frameDone != nullptr)
            {
                sink_.frameDone(sink_.user, hdr_.type, hdr_.frame_id, false);
            }
            reset();
        }

        void AiFrameProtocol::completeFrame()
        {
            /* 帧头阶段已被 sink 拒收（NACK 已在 startFrame 回过）：
             * 负载只消费不落盘，收满后静默复位，不重复回 ACK。 */
            if (dest_ == nullptr)
            {
                reset();
                return;
            }

            /* 负载 CRC32（flags bit1 且字段非 0） */
            if ((hdr_.flags & kAiFlagCrc32) && hdr_.frame_crc32 != 0)
            {
                if (ai_crc32(dest_, hdr_.payload_len) != hdr_.frame_crc32)
                {
                    abortFrame(kAiResultCrc);
                    return;
                }
            }

            if (sink_.sendCtl != nullptr)
            {
                sink_.sendCtl(sink_.user, kAiResultOk, hdr_.frame_id);
            }
            if (sink_.frameDone != nullptr)
            {
                sink_.frameDone(sink_.user, hdr_.type, hdr_.frame_id, true);
            }
            reset();
        }

        void AiFrameProtocol::handleControlFrame()
        {
            /* 主机→设备控制帧：kind=1 PING → ACK（统计由 AiMode 在 sendCtl 里带出） */
            const uint8_t kind = ctl_payload_[0];
            if (kind == kAiCtlKindPing && sink_.sendCtl != nullptr)
            {
                sink_.sendCtl(sink_.user, kAiResultOk, hdr_.frame_id);
            }
            reset();
        }

    } // namespace ai
} // namespace ekeys
