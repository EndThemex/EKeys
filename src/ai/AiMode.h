/*
 * AiMode.h
 *
 * AI Mode 门面（docs/13 §3/§9.3）：页面激活、链路/任务生命周期、
 * 帧接收 sink、统计与状态投递。
 *
 * 并发模型：
 *   - setScreenActive：DisplayTask（Core0）激活钩子调用，启停标志 +
 *     网络链接开关 + AiTask 创建（不在此做耗时解码/网络工作）；
 *   - pollSerial / pollNetwork：MainTask 上下文泵（非阻塞）；
 *   - AiTask（Core1，优先级 1）：JPEG/RLE 解码 → PSRAM 帧槽 → 投递
 *     AiFrame 消息；绝不调用 LVGL。
 */
#ifndef EKEYS_AI_AI_MODE_H
#define EKEYS_AI_AI_MODE_H

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/queue.h>

#include <stdint.h>

#include "AiConfig.h"
#include "AiFrameProtocol.h"
#include "AiNetLink.h"
#include "AiSerialLink.h"

namespace ekeys
{

    struct AiStatusInfo;

    namespace ai
    {

        class AiMode
        {
        public:
            static AiMode &instance();

            AiMode(const AiMode &) = delete;
            AiMode &operator=(const AiMode &) = delete;

            /* DisplayTask 激活钩子（run() 主循环 tag 变化比对，docs/13 §7.2） */
            void setScreenActive(bool active);
            bool isActive() const { return screen_active_; }

            /* MainTask::loop()：USB CDC 分流泵（SerialProtocol::poll() 之前） */
            void pollSerial();
            /* MainTask::tick()：TCP accept/读流泵（非阻塞） */
            void pollNetwork();

            /* WiFi 前置条件是否满足（AI 页 WAITING / SERIAL 状态判定） */
            bool netReady();

            /* 统计快照（AiTask 2Hz 投递 AiStatus） */
            void statusSnapshot(AiStatusInfo &out);

            /* 通道 id（帧 sink 回调 user 参数）
             * 整型→指针转换不是常量表达式（gcc8 按 reinterpret_cast 拒绝，
             * constexpr/C 风格转换均不可用），kChannelNet 只能类外定义 */
            static constexpr void *kChannelSerial = nullptr;
            static void *const kChannelNet;

            /* 帧接收 sink（AiFrameProtocol 回调，MainTask 上下文） */
            static uint8_t *sinkBeginFrame(void *user, uint8_t type,
                                           uint32_t payload_len, uint16_t w,
                                           uint16_t h, uint8_t flags,
                                           uint16_t frame_id, uint8_t *out_result);
            static void sinkFrameDone(void *user, uint8_t type, uint16_t frame_id,
                                      bool ok);
            static void sinkSendCtl(void *user, uint8_t result, uint16_t frame_id);

            /* ai_ctl JSON 行组装 → 对应链路输出（serial：JSON 行；net：TCP 行） */
            void sendCtlLine(void *user, const char *kind, uint8_t result,
                             uint16_t frame_id);
            void sendHello(void *user);

        private:
            AiMode();

            void startTask();
            static void taskEntry(void *arg);

            /* JPEG/RLE 组装缓冲（PSRAM 2×64KB 乒乓） */
            int acquireAssembler();
            void releaseAssembler(int idx);

            bool screen_active_{false};
            bool task_started_{false};
            TaskHandle_t task_handle_{nullptr};
            QueueHandle_t job_queue_{nullptr};

            /* 帧协议（串口 / TCP 各一） */
            AiFrameProtocol serial_protocol_;
            AiFrameProtocol net_protocol_;

            /* 链接（值成员，构造即挂好协议指针） */
            AiSerialLink serial_link_{};
            AiNetLink net_link_{};

            /* 组装缓冲 */
            uint8_t *assemblers_[kAiAssemblerCount]{nullptr, nullptr};
            bool assembler_busy_[kAiAssemblerCount]{false, false};
            portMUX_TYPE assembler_lock_;

            /* 正在接收的帧上下文（beginFrame 记录 → frameDone 使用） */
            int rx_write_slot_{-1};       /* RGB565 直写帧槽 */
            int pending_assembler_{-1};   /* JPEG/RLE 组装缓冲 */
            uint32_t pending_payload_len_{0};
            uint8_t pending_flags_{0};

            /* 统计（跨核读写，32 位/字节天然原子） */
            volatile uint32_t committed_frames_{0};
            volatile uint32_t dropped_frames_{0};
            volatile uint32_t last_commit_ms_{0};
            volatile uint16_t last_width_{428};
            volatile uint16_t last_height_{142};
            volatile uint8_t last_format_{0};
            uint32_t bytes_mark_ms_{0};
            uint32_t bytes_mark_total_{0};
            /* 最近一次 hello 输出时刻（主机晚开串口时按 1s 间隔补发） */
            volatile uint32_t last_hello_ms_{0};
        };

    } // namespace ai
} // namespace ekeys

#endif // EKEYS_AI_AI_MODE_H
