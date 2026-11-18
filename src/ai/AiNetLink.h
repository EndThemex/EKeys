/*
 * AiNetLink.h
 *
 * AI Mode WiFi TCP 通道（docs/13 §4.3）：WiFiServer :30001，仅 1 客户端，
 * 新连接踢掉旧连接。生命周期：仅 AI 页激活且 WiFi 就绪时 begin()，
 * 离开页面 end()。复用现有 STA 配置，不新增 WiFi 状态机。
 *
 * 前置条件不满足时 begin() 返回 false（页面显示 WIFI OFF，不隐式改配置）。
 * TCP 为专用流：无需 '\n'+magic 门控，解析器 Idle 态自行扫描 magic。
 */
#ifndef EKEYS_AI_AI_NET_LINK_H
#define EKEYS_AI_AI_NET_LINK_H

#include <stdint.h>

namespace ekeys
{
    namespace ai
    {

        class AiFrameProtocol;

        class AiNetLink
        {
        public:
            void begin(AiFrameProtocol *protocol) { protocol_ = protocol; }

            /* 生命周期：AI 页激活 + WiFi 连接时 start；离开页面 stop */
            bool start();
            void stop();

            /* MainTask::tick() 每轮调用：accept / 读流 / 半帧超时，非阻塞 */
            void poll();

            /* ai_ctl JSON 行 → 当前 TCP 客户端（无客户端时丢弃） */
            void sendLine(const char *line);

            bool listening() const { return listening_; }
            bool clientConnected() const;

            /* 新客户端接入事件（HELLO 由 AiMode 组装发送），取后清除 */
            bool takeClientJustConnected();

        private:
            AiFrameProtocol *protocol_{nullptr};
            bool listening_{false};
            bool client_just_connected_{false};
            void *server_{nullptr};  /* WiFiServer*，惰性创建（头文件不引 WiFi.h） */
            void *client_{nullptr};  /* WiFiClient* */
        };

    } // namespace ai
} // namespace ekeys

#endif // EKEYS_AI_AI_NET_LINK_H
