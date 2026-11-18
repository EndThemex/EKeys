/*
 * AiSerialLink.cpp
 *
 * 见 AiSerialLink.h。
 */
#include "AiSerialLink.h"

#include <Arduino.h>

#include "AiFrameProtocol.h"
#include "protocol/SerialProtocol.h"
#include "logging/LogManager.h"

namespace ekeys
{
    namespace ai
    {

        void AiSerialLink::setActive(bool active)
        {
            if (active == active_)
            {
                return;
            }
            active_ = active;
            if (!active)
            {
                /* 离开 AI 页：复位解析器并解除挂起（防残留二进制态卡死 JSON 通道） */
                leaveBinaryMode();
                expect_magic_ = false;
                handshake_request_ = false;
                if (protocol_ != nullptr)
                {
                    protocol_->reset();
                }
            }
        }

        void AiSerialLink::leaveBinaryMode()
        {
            if (mode_ == Mode::Binary)
            {
                mode_ = Mode::Line;
                frame_started_ = false;
                SerialProtocol::instance().setSuspendForAiBinary(false);
            }
        }

        void AiSerialLink::poll()
        {
            if (!active_ || protocol_ == nullptr)
            {
                return;
            }

            /* 半帧超时（≥300ms 无新字节）→ 复位解析器回行态（docs/13 §4.2） */
            if (mode_ == Mode::Binary && protocol_->inFrame() &&
                protocol_->lastByteMs() != 0 &&
                (millis() - protocol_->lastByteMs()) >= kAiHalfFrameTimeoutMs)
            {
                LOG_WARNING("AI", "serial half-frame timeout, resync");
                protocol_->reset();
                leaveBinaryMode();
                expect_magic_ = false;
            }

            /* 帧收满（解析器回 Idle）→ 立即回行态，帧间隙恢复 JSON 通道 */
            if (mode_ == Mode::Binary && frame_started_ && !protocol_->inFrame())
            {
                leaveBinaryMode();
            }

            const int avail = Serial.available();
            if (avail <= 0)
            {
                return;
            }

            if (mode_ == Mode::Binary)
            {
                /* 二进制态必须排空本次 available 的全部字节（docs/13 §4.4 规则 2），
                 * 否则 8KB 预算外溢的字节会被 SerialProtocol 当 JSON 行垃圾 */
                while (Serial.available() > 0)
                {
                    protocol_->pushByte(static_cast<uint8_t>(Serial.read()));
                }
                return;
            }

            /* 行态：每 tick 最多消费 8KB（5ms tick 折算 ~1.6MB/s） */
            uint32_t budget = kAiSerialBudgetPerTick;
            while (Serial.available() > 0 && budget > 0)
            {
                const uint8_t b = static_cast<uint8_t>(Serial.read());
                --budget;

                if (expect_magic_)
                {
                    if (b == '\n')
                    {
                        /*
                         * 连续定界符：主机（Web Serial 面板）会先发裸 '\n'
                         * 探针，再在新帧前发 '\n'。第二个 '\n' 不能清掉
                         * 期望标志，否则紧随其后的 0xA5 会被当 JSON 垃圾。
                         */
                        handshake_request_ = true;
                        continue;
                    }
                    expect_magic_ = false;
                    if (b == 0xA5)
                    {
                        /* 进入二进制态：挂起 SerialProtocol，首字节（0xA5）喂解析器 */
                        mode_ = Mode::Binary;
                        SerialProtocol::instance().setSuspendForAiBinary(true);
                        protocol_->pushByte(b);
                        frame_started_ = true;
                        handshake_request_ = false; /* 帧已到达，无需再补 hello */
                        /* 二进制态内剩余字节本轮继续排空 */
                        while (Serial.available() > 0)
                        {
                            protocol_->pushByte(static_cast<uint8_t>(Serial.read()));
                        }
                        return;
                    }
                    /* 非 0xA5：清标志并透传（普通 JSON 完全不受影响） */
                    SerialProtocol::instance().feedByte(static_cast<char>(b));
                    continue;
                }

                if (b == '\n')
                {
                    if (!SerialProtocol::instance().isLineBufferEmpty())
                    {
                        /* 行缓冲非空 → JSON 行结束符，回喂 */
                        SerialProtocol::instance().feedByte('\n');
                    }
                    else
                    {
                        /* 行缓冲为空 → 吞掉，期望下一字节为 magic 首字节（跨轮保持），
                         * 同时记录一次握手请求（主机打开串口后可能晚于本页激活） */
                        expect_magic_ = true;
                        handshake_request_ = true;
                    }
                    continue;
                }

                SerialProtocol::instance().feedByte(static_cast<char>(b));
            }
        }

        void AiSerialLink::sendLine(const char *line)
        {
            Serial.println(line);
        }

    } // namespace ai
} // namespace ekeys
