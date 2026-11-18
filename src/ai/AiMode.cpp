/*
 * AiMode.cpp
 *
 * 见 AiMode.h。帧数据流（docs/13 §3）：
 *
 *   主机 → [AiSerialLink / AiNetLink] → AiFrameProtocol（MainTask 上下文）
 *     ├─ RGB565：分片直写 FramePool Writing 槽 → frameDone → commit → AiFrame 消息
 *     └─ JPEG/RLE：分片写 PSRAM 组装缓冲 → frameDone → 任务队列 →
 *        AiTask（Core1）解码 → FramePool 槽 → commit → AiFrame 消息
 *   DisplayTask（唯一 LVGL 线程）：AiFrame → presentSlot + lv_img_set_src
 *
 * ACK/HELLO/统计一律 JSON 行（docs/13 §4.2）：串口 sendCtlLine 走 CDC，
 * TCP 走客户端流；流控为发送端 2 帧未 ACK 限流（设备端只保证及时回 ACK）。
 */
#include "AiMode.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "AiFramePool.h"
#include "AiImageDecoder.h"
#include "AiNetLink.h"
#include "AiSerialLink.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include "tasks/DisplayTask.h"

namespace ekeys
{
    namespace ai
    {

        namespace
        {
            constexpr uint16_t kAiImgW = 428;
            constexpr uint16_t kAiImgH = 142;
            constexpr uint32_t kAiFrameBytes = 428u * 142u * 2u;
        } // namespace

        /* AiTask 解码作业 */
        struct AiDecodeJob
        {
            uint8_t assembler_idx;
            uint8_t type; /* kAiTypeJpeg / kAiTypeRle565 */
            uint8_t flags;
            uint16_t frame_id;
            uint32_t len;
        };

        /* net 通道 tag：地址 1 仅作身份标识，绝不解引用 */
        void *const AiMode::kChannelNet = reinterpret_cast<void *>(1);

        AiMode &AiMode::instance()
        {
            static AiMode inst;
            return inst;
        }

        AiMode::AiMode()
            : assembler_lock_(portMUX_INITIALIZER_UNLOCKED)
        {
            serial_link_.begin(&serial_protocol_);
            net_link_.begin(&net_protocol_);
        }

        /* ---------------- 组装缓冲 ---------------- */

        int AiMode::acquireAssembler()
        {
            int found = -1;
            portENTER_CRITICAL(&assembler_lock_);
            for (int i = 0; i < kAiAssemblerCount; ++i)
            {
                if (!assembler_busy_[i])
                {
                    assembler_busy_[i] = true;
                    found = i;
                    break;
                }
            }
            portEXIT_CRITICAL(&assembler_lock_);
            return found;
        }

        void AiMode::releaseAssembler(int idx)
        {
            portENTER_CRITICAL(&assembler_lock_);
            assembler_busy_[idx] = false;
            portEXIT_CRITICAL(&assembler_lock_);
        }

        /* ---------------- 生命周期 ---------------- */

        bool AiMode::netReady()
        {
            return net_link_.listening();
        }

        void AiMode::setScreenActive(bool active)
        {
            if (active == screen_active_)
            {
                return;
            }
            screen_active_ = active;

            if (active)
            {
                /* PSRAM 资源（帧槽 + 组装缓冲）首次进入分配，常驻不释放 */
                if (!AiFramePool::instance().ensureAllocated())
                {
                    LOG_ERROR("AI", "frame pool alloc failed, AI mode disabled");
                    screen_active_ = false;
                    return;
                }
                if (assemblers_[0] == nullptr)
                {
                    for (int i = 0; i < kAiAssemblerCount; ++i)
                    {
                        assemblers_[i] = static_cast<uint8_t *>(
                            heap_caps_malloc(kAiMaxPayloadBytes, MALLOC_CAP_SPIRAM));
                        if (assemblers_[i] == nullptr)
                        {
                            LOG_ERROR("AI", "assembler %d alloc failed", i);
                            screen_active_ = false;
                            return;
                        }
                    }
                }

                /* 统计与解析器复位 */
                serial_protocol_.reset();
                net_protocol_.reset();
                committed_frames_ = 0;
                dropped_frames_ = 0;
                last_commit_ms_ = 0;
                bytes_mark_ms_ = millis();
                bytes_mark_total_ = 0;

                serial_link_.setActive(true);
                /* USB 侧 hello：AI 页激活即发一帧，主机据此对齐协议（docs/13 §4.2） */
                last_hello_ms_ = 0;
                sendHello(kChannelSerial);

                /* TCP 通道：WiFi 就绪才 start；无 WiFi 时 USB 串口通道照常可用 */
                net_link_.start();

                startTask();
                LOG_INFO("AI", "ai mode active");
            }
            else
            {
                serial_link_.setActive(false);
                net_link_.stop();
                /* 槽态归 Free（像素保留）；AiTask 自检测退出 */
                AiFramePool::instance().resetForIdle();
                portENTER_CRITICAL(&assembler_lock_);
                assembler_busy_[0] = false;
                assembler_busy_[1] = false;
                portEXIT_CRITICAL(&assembler_lock_);
                LOG_INFO("AI", "ai mode inactive (dropped=%u)",
                         static_cast<unsigned>(dropped_frames_));
            }
        }

        void AiMode::startTask()
        {
            if (task_started_)
            {
                return;
            }
            if (job_queue_ == nullptr)
            {
                job_queue_ = xQueueCreate(kAiAssemblerCount, sizeof(AiDecodeJob));
                if (job_queue_ == nullptr)
                {
                    LOG_ERROR("AI", "job queue create failed");
                    return;
                }
            }
            serial_protocol_.setSink(AiFrameSink{kChannelSerial,
                                                 &AiMode::sinkBeginFrame,
                                                 &AiMode::sinkFrameDone,
                                                 &AiMode::sinkSendCtl});
            net_protocol_.setSink(AiFrameSink{kChannelNet,
                                              &AiMode::sinkBeginFrame,
                                              &AiMode::sinkFrameDone,
                                              &AiMode::sinkSendCtl});

            if (xTaskCreatePinnedToCore(&AiMode::taskEntry, "AiTask",
                                        kAiTaskStackBytes, this,
                                        kAiTaskPriority, &task_handle_,
                                        kAiTaskCore) != pdPASS)
            {
                LOG_ERROR("AI", "AiTask create failed");
                return;
            }
            task_started_ = true;
            LOG_INFO("AI", "AiTask started on core %d", kAiTaskCore);
        }

        /* ---------------- MainTask 泵 ---------------- */

        void AiMode::pollSerial()
        {
            if (!screen_active_)
            {
                return;
            }
            serial_link_.poll();

            /*
             * 补发 hello：Web 面板通常在本页激活后才打开串口，激活时那次
             * hello 会被错过。主机随后发裸 '\n' 探针（AiSerialLink 置位），
             * 这里按 1s 限流补发一次；流式期间（近期有提交帧）不发，避免
             * 与 ACK/日志互相干扰。
             */
            if (serial_link_.takeHandshakeRequest())
            {
                const uint32_t now = millis();
                const bool idle = (last_commit_ms_ == 0) ||
                                  ((now - last_commit_ms_) >= 1000);
                if (idle && (last_hello_ms_ == 0 || (now - last_hello_ms_) >= 1000))
                {
                    sendHello(kChannelSerial);
                }
            }
        }

        void AiMode::pollNetwork()
        {
            if (!screen_active_ || !net_link_.listening())
            {
                return;
            }
            net_link_.poll();
            /* HELLO：TCP 客户端接入后先发（docs/13 §4.2） */
            if (net_link_.takeClientJustConnected())
            {
                sendHello(kChannelNet);
            }
        }

        /* ---------------- 帧接收 sink（MainTask 上下文） ---------------- */

        uint8_t *AiMode::sinkBeginFrame(void *user, uint8_t type,
                                        uint32_t payload_len, uint16_t w,
                                        uint16_t h, uint8_t flags,
                                        uint16_t frame_id, uint8_t *out_result)
        {
            AiMode &self = instance();
            (void)user;
            (void)w;
            (void)h;
            (void)frame_id;

            if (type == kAiTypeRgb565)
            {
                /* 分片直写帧槽（docs/13 §4.2）：不占组装缓冲 */
                const int slot = AiFramePool::instance().acquireWriteSlot();
                if (slot < 0)
                {
                    *out_result = kAiResultBusy;
                    return nullptr;
                }
                self.rx_write_slot_ = slot;
                return AiFramePool::instance().px(slot);
            }

            /* JPEG / RLE565：组装缓冲（≤64KB），AiTask 解码（docs/13 §4.2） */
            const int idx = self.acquireAssembler();
            if (idx < 0)
            {
                *out_result = kAiResultBusy;
                return nullptr;
            }
            /* 记录帧上下文：frameDone 组装 AiDecodeJob 时需要 len/flags */
            self.pending_assembler_ = idx;
            self.pending_payload_len_ = payload_len;
            self.pending_flags_ = flags;
            return self.assemblers_[idx];
        }

        void AiMode::sinkFrameDone(void *user, uint8_t type, uint16_t frame_id,
                                   bool ok)
        {
            AiMode &self = instance();
            (void)user;

            if (!ok)
            {
                /* CRC 失败等传输层中止：回收资源（解析器已回 NACK） */
                if (type == kAiTypeRgb565 && self.rx_write_slot_ >= 0)
                {
                    AiFramePool::instance().abandonWriteSlot(self.rx_write_slot_);
                    self.rx_write_slot_ = -1;
                }
                if (type != kAiTypeRgb565 && self.pending_assembler_ >= 0)
                {
                    self.releaseAssembler(self.pending_assembler_);
                    self.pending_assembler_ = -1;
                }
                ++self.dropped_frames_;
                return;
            }

            self.last_width_ = 428;
            self.last_height_ = 142;
            self.last_format_ = type;

            if (type == kAiTypeRgb565)
            {
                const int slot = self.rx_write_slot_;
                self.rx_write_slot_ = -1;
                if (slot >= 0 && AiFramePool::instance().commitWriteSlot(slot))
                {
                    ++self.committed_frames_;
                    self.last_commit_ms_ = millis();
                }
                else
                {
                    /* 投递失败（DisplayTask 队列满）→ 槽回 Free 防泄漏 */
                    if (slot >= 0)
                    {
                        AiFramePool::instance().abandonWriteSlot(slot);
                    }
                    ++self.dropped_frames_;
                }
                return;
            }

            /* JPEG/RLE：投递解码作业（组装缓冲由 AiTask 释放） */
            AiDecodeJob job{};
            job.assembler_idx = static_cast<uint8_t>(self.pending_assembler_);
            job.type = type;
            job.flags = self.pending_flags_;
            job.frame_id = frame_id;
            job.len = self.pending_payload_len_;
            self.pending_assembler_ = -1;

            if (xQueueSend(self.job_queue_, &job, 0) != pdTRUE)
            {
                /* 作业队列满（两个组装缓冲都在解码）→ 丢帧 */
                self.releaseAssembler(job.assembler_idx);
                ++self.dropped_frames_;
                LOG_INFO("AI", "job queue full, frame %u dropped",
                         static_cast<unsigned>(frame_id));
            }
        }

        void AiMode::sinkSendCtl(void *user, uint8_t result, uint16_t frame_id)
        {
            instance().sendCtlLine(user,
                                   result == kAiResultOk ? "ack" : "nack",
                                   result, frame_id);
        }

        /* ---------------- ai_ctl JSON 行 ---------------- */

        void AiMode::sendHello(void *user)
        {
            last_hello_ms_ = millis();
            char line[384];
            snprintf(line, sizeof(line),
                     "{\"ai_ctl\":{\"kind\":\"hello\",\"result\":0,\"frame_id\":0,"
                     "\"w\":428,\"h\":142,\"formats\":[1,2,3],"
                     "\"chunk_payload\":%u,\"max_payload\":%u,"
                     "\"max_raw_payload\":%u,\"version\":1,\"free_slots\":%u,"
                     "\"dropped\":%u,\"fps_x10\":0,\"bytes_per_sec\":0}}",
                     (user == kChannelSerial) ? kAiChunkUsb : kAiChunkTcp,
                     static_cast<unsigned>(kAiMaxPayloadBytes),
                     static_cast<unsigned>(kAiMaxRawPayloadBytes),
                     static_cast<unsigned>(AiFramePool::instance().freeSlotCount()),
                     static_cast<unsigned>(dropped_frames_));
            if (user == kChannelSerial)
            {
                serial_link_.sendLine(line);
            }
            else
            {
                net_link_.sendLine(line);
            }
        }

        void AiMode::sendCtlLine(void *user, const char *kind, uint8_t result,
                                 uint16_t frame_id)
        {
            AiStatusInfo st;
            statusSnapshot(st);

            char line[256];
            snprintf(line, sizeof(line),
                     "{\"ai_ctl\":{\"kind\":\"%s\",\"result\":%u,\"frame_id\":%u,"
                     "\"free_slots\":%u,\"dropped\":%u,\"fps_x10\":%u,"
                     "\"bytes_per_sec\":%u}}",
                     kind, static_cast<unsigned>(result),
                     static_cast<unsigned>(frame_id),
                     static_cast<unsigned>(AiFramePool::instance().freeSlotCount()),
                     static_cast<unsigned>(dropped_frames_),
                     static_cast<unsigned>(st.fps_x10),
                     static_cast<unsigned>(st.bytes_per_s));
            if (user == kChannelSerial)
            {
                serial_link_.sendLine(line);
            }
            else
            {
                net_link_.sendLine(line);
            }
        }

        void AiMode::statusSnapshot(AiStatusInfo &out)
        {
            /* 字节速率：以协议层累计负载字节差分（1s 窗） */
            const uint32_t now = millis();
            const uint32_t total = serial_protocol_.totalPayloadBytes() +
                                   net_protocol_.totalPayloadBytes();
            const uint32_t dt = now - bytes_mark_ms_;
            if (dt >= kAiStatsWindowMs && bytes_mark_total_ != 0)
            {
                out.bytes_per_s =
                    static_cast<uint32_t>(
                        (uint64_t)(total - bytes_mark_total_) * 1000 / dt);
                bytes_mark_ms_ = now;
                bytes_mark_total_ = total;
            }
            else if (bytes_mark_total_ == 0)
            {
                bytes_mark_ms_ = now;
                bytes_mark_total_ = total;
            }

            /* fps：1s 窗内提交帧数（AiTask 侧推进 committed_frames_） */
            static uint32_t s_frames_mark = 0;
            static uint32_t s_frames_mark_ms = 0;
            if (now - s_frames_mark_ms >= kAiStatsWindowMs)
            {
                out.fps_x10 = static_cast<uint16_t>(
                    (committed_frames_ - s_frames_mark) * 10 * 1000 /
                    (now - s_frames_mark_ms));
                s_frames_mark = committed_frames_;
                s_frames_mark_ms = now;
            }

            /* 状态：近期有提交帧 → STREAMING；否则 WiFi 未就绪 → SERIAL（USB CDC） */
            if (last_commit_ms_ != 0 && (now - last_commit_ms_) < 1500)
            {
                out.state = kAiStateStreaming;
            }
            else if (netReady())
            {
                out.state = kAiStateWaiting;
            }
            else
            {
                out.state = kAiStateSerial;
            }
            out.format = last_format_;
            out.dropped = static_cast<uint16_t>(dropped_frames_ & 0xFFFF);
            out.width = last_width_;
            out.height = last_height_;
        }

        /* ---------------- AiTask（Core1，解码） ---------------- */

        void AiMode::taskEntry(void *arg)
        {
            auto *self = static_cast<AiMode *>(arg);
            AiDecodeJob job;
            uint32_t last_status_ms = 0;

            for (;;)
            {
                if (!self->screen_active_)
                {
                    break;
                }

                AiFramePool &pool = AiFramePool::instance();

                if (xQueueReceive(self->job_queue_, &job,
                                  pdMS_TO_TICKS(20)) == pdTRUE)
                {
                    const int slot = pool.acquireWriteSlot();
                    if (slot < 0)
                    {
                        /* 三槽均忙（丢帧） */
                        self->releaseAssembler(job.assembler_idx);
                        ++self->dropped_frames_;
                        continue;
                    }

                    const uint32_t t0 = millis();
                    AiDecodeResult rc = AiDecodeResult::Failed;
                    if (job.type == kAiTypeJpeg)
                    {
                        rc = aiDecodeJpeg(self->assemblers_[job.assembler_idx],
                                          job.len, pool.px(slot),
                                          kAiImgW, kAiImgH,
                                          (job.flags & kAiFlagScaleHalf) != 0);
                    }
                    else if (job.type == kAiTypeRle565)
                    {
                        rc = aiDecodeRle565(self->assemblers_[job.assembler_idx],
                                            job.len, pool.px(slot),
                                            kAiFrameBytes);
                    }
                    const uint32_t decode_ms = millis() - t0;

                    self->releaseAssembler(job.assembler_idx);

                    if (rc == AiDecodeResult::Ok &&
                        pool.commitWriteSlot(slot))
                    {
                        ++self->committed_frames_;
                        self->last_commit_ms_ = millis();
                    }
                    else
                    {
                        pool.abandonWriteSlot(slot);
                        ++self->dropped_frames_;
                    }

                    if (decode_ms > 0 && (self->committed_frames_ % 10 == 0))
                    {
                        LOG_INFO("AI", "frame %u decode %lu ms",
                                 static_cast<unsigned>(job.frame_id),
                                 static_cast<unsigned long>(decode_ms));
                    }
                }

                /* AiStatus 2Hz → DisplayTask（HUD 统计） */
                const uint32_t now = millis();
                if (now - last_status_ms >= kAiStatusPeriodMs)
                {
                    last_status_ms = now;
                    AiStatusInfo st;
                    self->statusSnapshot(st);
                    DisplayMessage msg;
                    msg.type = DisplayMessageType::AiStatus;
                    msg.ai_status = st;
                    DisplayTask::instance().post(msg, 0);
                }
            }

            self->task_started_ = false;
            self->task_handle_ = nullptr;
            LOG_INFO("AI", "AiTask exit");
            vTaskDelete(nullptr);
        }

    } // namespace ai
} // namespace ekeys
