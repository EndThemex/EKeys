/*
 * AiSerialLink.h
 *
 * USB CDC 二进制分流器（docs/13 §4.4）。单读者 + 二进制优先：
 *
 *   - MainTask::loop() 先调 pollSerial()（本类），再调 SerialProtocol::poll()；
 *   - 仅 AI 页激活时工作；行态每 tick 最多消费 8KB；
 *   - '\n' 归属规则：行缓冲非空 → JSON 行结束符，回喂 SerialProtocol；
 *     行缓冲为空 → 吞掉并置"期望 magic"标志（跨轮保持）；连续 '\n'
 *     保持期望（主机可能先发裸 '\n' 探针、再发帧前 '\n'）；下一字节
 *     0xA5 → 进入二进制态，否则清标志回喂（ASCII JSON 不可能以 0xA5 开头）；
 *   - 裸 '\n' 同时置"握手请求"标志，AiMode 据此补发 hello（主机晚于
 *     本页激活才打开串口时也能完成握手）；
 *   - 二进制态排空本次 Serial.available() 全部字节，并挂起
 *     SerialProtocol::poll()；帧收满或 300ms 超时后回行态；
 *   - 设备→主机方向不分流：ai_ctl 走 JSON 行（sendLine）。
 */
#ifndef EKEYS_AI_AI_SERIAL_LINK_H
#define EKEYS_AI_AI_SERIAL_LINK_H

#include <stdint.h>

#include "AiConfig.h"

namespace ekeys
{
    namespace ai
    {

        class AiFrameProtocol;

        class AiSerialLink
        {
        public:
            void begin(AiFrameProtocol *protocol) { protocol_ = protocol; }

            /* AI 页激活/离开（DisplayTask 激活钩子 → AiMode 转发） */
            void setActive(bool active);

            /* MainTask::loop() 每轮调用（SerialProtocol::poll() 之前），非阻塞 */
            void poll();

            /* ai_ctl JSON 行输出（与日志共用 CDC 输出流） */
            void sendLine(const char *line);

            bool inBinaryMode() const { return mode_ == Mode::Binary; }

            /* 主机发来裸 '\n'（握手探针 / 帧起点）→ 取走一次握手请求 */
            bool takeHandshakeRequest()
            {
                const bool requested = handshake_request_;
                handshake_request_ = false;
                return requested;
            }

        private:
            enum class Mode : uint8_t
            {
                Line,        /* 正常 JSON/日志行模式 */
                Binary,      /* 帧内：全部字节归帧解析器 */
            };

            void leaveBinaryMode();

            AiFrameProtocol *protocol_{nullptr};
            bool active_{false};
            Mode mode_{Mode::Line};
            bool expect_magic_{false}; /* '\n'+空行缓冲 → 期望 A5（跨轮保持） */
            bool frame_started_{false}; /* 二进制态内已开始收帧 */
            bool handshake_request_{false}; /* 收到裸 '\n'，AiMode 可补发 hello */
        };

    } // namespace ai
} // namespace ekeys

#endif // EKEYS_AI_AI_SERIAL_LINK_H
