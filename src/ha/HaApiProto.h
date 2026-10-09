/*
 * HaApiProto.h
 *
 * ESPHome Native API 明文协议 v1 子集——纯编解码层（docs/11 §2/§3）。
 *
 *   - 无状态、无 IO：帧封装/解析 + varint + 各消息字段编码
 *   - 帧格式：0x00 指示字节 + varint(载荷长度) + varint(消息类型) + protobuf 载荷
 *     （载荷长度只含 protobuf 载荷字节数、不含类型 varint，与官方
 *      api_frame_helper_plaintext.cpp / aioesphomeapi 解析端一致；
 *      载荷长度 0 合法——PingRequest/DisconnectRequest 为空载荷帧）
 *   - protobuf 手写编解码（proto3：字段号 + wire type），不引入 nanopb
 *   - 请求类消息（HelloRequest/ConnectRequest 等）载荷内容全部忽略：
 *     帧本身是长度分界的，跳过载荷不会失步；未配置密码时 ConnectRequest
 *     任意值通过（校验位已留好，远期加 ha_api_password 时再解析 password 字段）
 *
 * 可独立单测（不依赖 Arduino）。
 */

#ifndef EKEYS_HA_HA_API_PROTO_H
#define EKEYS_HA_HA_API_PROTO_H

#include <stddef.h>
#include <stdint.h>

namespace ekeys
{
    namespace ha
    {

        /* 消息类型号（对照官方 api.proto，本方案最小集合，docs/11 §2） */
        enum HaMsgType : uint8_t
        {
            HelloRequest = 1,
            HelloResponse = 2,
            ConnectRequest = 3,
            ConnectResponse = 4,
            DisconnectRequest = 5,
            DisconnectResponse = 6,
            PingRequest = 7,
            PingResponse = 8,
            DeviceInfoRequest = 9,
            DeviceInfoResponse = 10,
            ListEntitiesRequest = 11,
            ListEntitiesBinarySensorResponse = 12,
            ListEntitiesDoneResponse = 19,
            SubscribeStatesRequest = 20,
            BinarySensorStateResponse = 21,
        };

        /*
         * 缓冲上限：最大发送载荷为 DeviceInfoResponse（5 个 string + 2 个 bool，
         * 约 120B）。超出上限的入站帧直接丢弃重同步（正常 HA 客户端握手帧
         * 远小于该上限）。
         */
        constexpr size_t kHaMaxPayloadSize = 192;
        /* 帧总长上限：指示字节 1 + 长度 varint ≤5 + 类型 varint ≤2 + 载荷 */
        constexpr size_t kHaMaxFrameSize = kHaMaxPayloadSize + 8;

        /* varint 编码：返回写入字节数（≤5），buf 容量不足返回 0 */
        size_t haVarintEncode(uint32_t value, uint8_t *buf, size_t cap);

        /*
         * varint 解码：从 buf[pos] 起解析，成功写回 value 并前移 pos；
         * 数据不足返回 false（pos 可能已前移，调用方按失败忽略本帧剩余）。
         */
        bool haVarintDecode(const uint8_t *buf, size_t len, size_t &pos, uint32_t &value);

        /*
         * 帧封装：0x00 + varint(类型varint+载荷总长) + varint(type) + payload。
         * 返回写入总字节数；cap 不足返回 0（不产生半帧）。
         */
        size_t haFrameEncode(uint8_t msg_type, const uint8_t *payload,
                             size_t payload_len, uint8_t *out, size_t cap);

        /*
         * protobuf 字段编码器（仅覆盖本方案用到的三种 wire type）：
         *   - string/bytes: wire type 2（0x0A 形式 tag）
         *   - bool/varint:  wire type 0
         *   - fixed32:      wire type 5（0x0D 形式 tag，4 字节 LE）
         */
        class HaProtoWriter
        {
        public:
            void reset() { len_ = 0; }

            void addStringField(uint32_t field_no, const char *value);
            void addVarintField(uint32_t field_no, uint32_t value);
            void addBoolField(uint32_t field_no, bool value);
            void addFixed32Field(uint32_t field_no, uint32_t value);

            const uint8_t *data() const { return buf_; }
            size_t size() const { return len_; }

        private:
            void addTag(uint32_t field_no, uint8_t wire_type);

            uint8_t buf_[kHaMaxPayloadSize];
            size_t len_{0};
        };

        /*
         * 帧解析器（流式累积，逐字节喂入）：
         *   - feed() 内部完成重同步（跳过指示字节前的噪声）与超长帧丢弃
         *   - 解析出完整帧后 hasFrame()=true；consume() 移除已消费字节并
         *     立即重解析（支持一次 read 带回多帧的流水线场景）
         */
        class HaFrameDecoder
        {
        public:
            void reset();

            void feed(const uint8_t *data, size_t len);

            bool hasFrame() const { return frame_ready_; }
            uint8_t messageType() const { return msg_type_; }
            const uint8_t *payload() const { return buf_ + payload_offset_; }
            size_t payloadSize() const { return payload_size_; }

            void consume();

        private:
            void tryParse();

            uint8_t buf_[kHaMaxFrameSize];
            size_t fill_{0};
            bool frame_ready_{false};
            bool overflowed_{false};
            size_t payload_offset_{0};
            size_t payload_size_{0};
            uint8_t msg_type_{0};
        };

    } // namespace ha
} // namespace ekeys

#endif // EKEYS_HA_HA_API_PROTO_H
