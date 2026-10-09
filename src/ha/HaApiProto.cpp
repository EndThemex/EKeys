/*
 * HaApiProto.cpp
 *
 * ESPHome Native API 明文协议 v1 子集编解码实现（docs/11 §2）。
 * 纯编解码，无 IO 无状态——帧解析状态在 HaFrameDecoder 实例内。
 */

#include "ha/HaApiProto.h"

#include <string.h>

namespace ekeys
{
    namespace ha
    {

        namespace
        {

            constexpr uint8_t kFrameIndicator = 0x00;

            /* wire type（proto3） */
            constexpr uint8_t kWireVarint = 0;
            constexpr uint8_t kWireFixed32 = 5;

            constexpr uint32_t kMaxVarintBytes = 5;

        } // namespace

        size_t haVarintEncode(uint32_t value, uint8_t *buf, size_t cap)
        {
            size_t n = 0;
            while (value >= 0x80U)
            {
                if (n >= cap)
                {
                    return 0;
                }
                buf[n++] = static_cast<uint8_t>(value) | 0x80U;
                value >>= 7;
            }
            if (n >= cap)
            {
                return 0;
            }
            buf[n++] = static_cast<uint8_t>(value);
            return n;
        }

        bool haVarintDecode(const uint8_t *buf, size_t len, size_t &pos, uint32_t &value)
        {
            uint32_t result = 0;
            uint32_t shift = 0;
            for (uint32_t i = 0; i < kMaxVarintBytes; ++i)
            {
                if (pos >= len)
                {
                    return false; /* 数据不足 */
                }
                const uint8_t b = buf[pos++];
                result |= static_cast<uint32_t>(b & 0x7FU) << shift;
                if ((b & 0x80U) == 0)
                {
                    value = result;
                    return true;
                }
                shift += 7;
            }
            return false; /* varint 超长，视为坏帧 */
        }

        size_t haFrameEncode(uint8_t msg_type, const uint8_t *payload,
                             size_t payload_len, uint8_t *out, size_t cap)
        {
            if (payload_len > kHaMaxPayloadSize)
            {
                return 0;
            }

            uint8_t type_varint[kMaxVarintBytes];
            const size_t type_len = haVarintEncode(msg_type, type_varint,
                                                   sizeof(type_varint));
            if (type_len == 0)
            {
                return 0;
            }

            /*
             * size varint = protobuf 载荷字节数，不含类型 varint。
             * （官方 api_frame_helper_plaintext.cpp write_plaintext_header 与
             * aioesphomeapi 解析端同语义；2026-10-09 修复：原实现误把类型
             * varint 计入 size，HA 按 size 读载荷永远差 1 字节，hello 等满
             * 超时后报"无法连接到 ESPHome 设备"）
             */
            const uint32_t content_len = static_cast<uint32_t>(payload_len);

            uint8_t len_varint[kMaxVarintBytes];
            const size_t len_bytes = haVarintEncode(content_len, len_varint,
                                                    sizeof(len_varint));
            const size_t total = 1 + len_bytes + type_len + payload_len;
            if (len_bytes == 0 || total > cap)
            {
                return 0;
            }

            size_t pos = 0;
            out[pos++] = kFrameIndicator;
            memcpy(out + pos, len_varint, len_bytes);
            pos += len_bytes;
            memcpy(out + pos, type_varint, type_len);
            pos += type_len;
            if (payload_len > 0 && payload != nullptr)
            {
                memcpy(out + pos, payload, payload_len);
            }
            return total;
        }

        void HaProtoWriter::addTag(uint32_t field_no, uint8_t wire_type)
        {
            const uint32_t tag = (field_no << 3) | wire_type;
            uint8_t tmp[kMaxVarintBytes];
            const size_t n = haVarintEncode(tag, tmp, sizeof(tmp));
            if (n > 0 && len_ + n <= sizeof(buf_))
            {
                memcpy(buf_ + len_, tmp, n);
                len_ += n;
            }
        }

        void HaProtoWriter::addStringField(uint32_t field_no, const char *value)
        {
            if (value == nullptr)
            {
                value = "";
            }
            const size_t slen = strlen(value);

            addTag(field_no, 2); /* length-delimited */
            uint8_t tmp[kMaxVarintBytes];
            const size_t n = haVarintEncode(static_cast<uint32_t>(slen),
                                            tmp, sizeof(tmp));
            if (n > 0 && len_ + n + slen <= sizeof(buf_))
            {
                memcpy(buf_ + len_, tmp, n);
                len_ += n;
                memcpy(buf_ + len_, value, slen);
                len_ += slen;
            }
        }

        void HaProtoWriter::addVarintField(uint32_t field_no, uint32_t value)
        {
            addTag(field_no, kWireVarint);
            uint8_t tmp[kMaxVarintBytes];
            const size_t n = haVarintEncode(value, tmp, sizeof(tmp));
            if (n > 0 && len_ + n <= sizeof(buf_))
            {
                memcpy(buf_ + len_, tmp, n);
                len_ += n;
            }
        }

        void HaProtoWriter::addBoolField(uint32_t field_no, bool value)
        {
            addTag(field_no, kWireVarint);
            if (len_ + 1 <= sizeof(buf_))
            {
                buf_[len_++] = value ? 1U : 0U;
            }
        }

        void HaProtoWriter::addFixed32Field(uint32_t field_no, uint32_t value)
        {
            addTag(field_no, kWireFixed32);
            if (len_ + 4 <= sizeof(buf_))
            {
                buf_[len_++] = static_cast<uint8_t>(value & 0xFFU);
                buf_[len_++] = static_cast<uint8_t>((value >> 8) & 0xFFU);
                buf_[len_++] = static_cast<uint8_t>((value >> 16) & 0xFFU);
                buf_[len_++] = static_cast<uint8_t>((value >> 24) & 0xFFU);
            }
        }

        void HaFrameDecoder::reset()
        {
            fill_ = 0;
            frame_ready_ = false;
            overflowed_ = false;
            payload_offset_ = 0;
            payload_size_ = 0;
            msg_type_ = 0;
        }

        void HaFrameDecoder::feed(const uint8_t *data, size_t len)
        {
            if (overflowed_)
            {
                return; /* 坏帧状态保持到 reset()（新客户端连接时调用） */
            }
            /*
             * 一次 read 可能带回多帧（粘包）：数据全部入缓冲，不因
             * frame_ready_ 提前中断——consume() 移除已消费帧后会重解析
             * 缓冲内的剩余字节。
             */
            while (len > 0)
            {
                if (fill_ == sizeof(buf_))
                {
                    /* 缓冲已满仍无法组成合法帧：丢弃全部字节重同步 */
                    overflowed_ = true;
                    return;
                }
                const size_t space = sizeof(buf_) - fill_;
                const size_t n = (len < space) ? len : space;
                memcpy(buf_ + fill_, data, n);
                fill_ += n;
                data += n;
                len -= n;
                tryParse();
            }
        }

        void HaFrameDecoder::tryParse()
        {
            for (;;)
            {
                /* 1. 重同步：丢弃指示字节前的噪声 */
                size_t start = 0;
                while (start < fill_ && buf_[start] != kFrameIndicator)
                {
                    ++start;
                }
                if (start > 0)
                {
                    memmove(buf_, buf_ + start, fill_ - start);
                    fill_ -= start;
                }
                if (fill_ == 0)
                {
                    return;
                }

                /* 2. 载荷长度 varint（起始于 1）：只含 protobuf 载荷字节数，
                 *    类型 varint 与载荷依次排在其后 */
                size_t pos = 1;
                uint32_t content_len = 0;
                if (!haVarintDecode(buf_, fill_, pos, content_len))
                {
                    return; /* 数据不足，等下轮 feed */
                }
                /* 类型 varint 起点 = 载荷长度 varint 之后（可能占 2 字节） */
                const size_t content_start = pos;

                /* 3. 超长载荷：丢弃本指示字节，继续扫描（防坏帧卡死解析）。
                 *    长度 0 合法（PingRequest/DisconnectRequest 等空载荷帧） */
                if (content_len > kHaMaxPayloadSize)
                {
                    memmove(buf_, buf_ + 1, fill_ - 1);
                    fill_ -= 1;
                    continue;
                }

                /* 4. 类型 varint */
                uint32_t type = 0;
                if (!haVarintDecode(buf_, fill_, pos, type))
                {
                    return; /* 数据不足 */
                }
                if (type > 0xFFU)
                {
                    /* 类型号超出本实现范围：丢弃整个帧后继续扫描 */
                    const size_t type_len = pos - content_start;
                    const size_t skip = content_start + type_len + content_len;
                    const size_t drop = (fill_ < skip) ? fill_ : skip;
                    memmove(buf_, buf_ + drop, fill_ - drop);
                    fill_ -= drop;
                    continue;
                }

                /* 5. 载荷 = content_len 字节（size 语义不含类型 varint） */
                if (fill_ - pos < content_len)
                {
                    return; /* 载荷未收全，等下轮 feed */
                }

                msg_type_ = static_cast<uint8_t>(type);
                payload_offset_ = pos;
                payload_size_ = content_len;
                frame_ready_ = true;
                return;
            }
        }

        void HaFrameDecoder::consume()
        {
            if (!frame_ready_)
            {
                return;
            }
            const size_t consumed = payload_offset_ + payload_size_;
            if (consumed > 0 && consumed <= fill_)
            {
                memmove(buf_, buf_ + consumed, fill_ - consumed);
                fill_ -= consumed;
            }
            else
            {
                fill_ = 0;
            }
            frame_ready_ = false;
            tryParse(); /* 残余字节可能已含下一帧 */
        }

    } // namespace ha
} // namespace ekeys
