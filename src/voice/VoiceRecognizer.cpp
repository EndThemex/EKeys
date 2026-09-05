/*
 * VoiceRecognizer.cpp
 *
 * 见 VoiceRecognizer.h。
 */

#include "VoiceRecognizer.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/AppContext.h"
#include "audio/Mic.h"
#include "audio/Speaker.h"
#include "config/Configuration.h"
#include "keymap/KeyNameTable.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include "network/TcpChannel.h"
#include "network/WiFiManager.h"
#include "protocol/SerialProtocol.h"
#include "tasks/DisplayTask.h"
#include "voice/AsrTokenCache.h"
#include "voice/VoiceConfig.h"

namespace ekeys
{

    VoiceRecognizer &VoiceRecognizer::instance()
    {
        static VoiceRecognizer inst;
        return inst;
    }

    bool VoiceRecognizer::canWork() const
    {
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        return snap.work_mode == 0 && // 仅 USB 模式（§11.3）
               snap.voice_enable != 0 &&
               WiFiManager::instance().isConnected() &&
               !suspended_;
    }

    /*
     * I2S0 Mic 与 I2S1 Speaker 共享 IO10 BCLK（PINOUT §2.7）。
     * 若 Speaker 正在播放（Audio::isRunning()），Mic.begin() 会导致
     * 两个 I2S 外设同时驱动 IO10，电平冲突 / 录音数据损坏。
     * 因此录音 / 频谱前必须先 stop Speaker；反过来录音期间
     * Speaker::PlayRemoteAudio / PlayLocalAudio 也会被 Mic 守卫拒绝。
     */
    void VoiceRecognizer::prepareI2sForMicCapture()
    {
        if (Speaker::instance().isRunning())
        {
            LOG_INFO("ASR", "speaker is active, stopping before mic capture");
            Speaker::instance().Stop();
        }
    }

    void VoiceRecognizer::postRecordingState(bool recording)
    {
        DisplayMessage msg;
        msg.type = DisplayMessageType::AsrRecording;
        msg.asr_recording = recording;
        DisplayTask::instance().post(msg, 0);
    }

    bool VoiceRecognizer::startCapture()
    {
        if (capturing_)
        {
            return false;
        }

        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        if (!canWork())
        {
            if (snap.work_mode != 0)
            {
                LOG_INFO("ASR", "voice only available in USB mode");
            }
            return false;
        }

        /* BCLK=IO10 与 Speaker 互斥（PINOUT §2.7）：先停 Speaker 再 begin Mic */
        prepareI2sForMicCapture();

        if (!Mic::instance().begin())
        {
            return false;
        }

        /* PCM 缓冲（PSRAM）：按 voice_max_record_ms 一次性分配 */
        if (pcm_buf_ == nullptr)
        {
            uint32_t max_ms = snap.voice_max_record_ms;
            if (max_ms == 0)
            {
                max_ms = 5000;
            }
            if (max_ms > voice::kMaxRecordMsCap)
            {
                max_ms = voice::kMaxRecordMsCap;
            }
            const size_t cap_samples =
                static_cast<size_t>(voice::kPcmSampleRate * max_ms / 1000U);
            pcm_buf_ = static_cast<int16_t *>(ps_malloc(cap_samples * sizeof(int16_t)));
            if (pcm_buf_ == nullptr)
            {
                LOG_ERROR("ASR", "pcm buffer alloc failed");
                Mic::instance().end();
                return false;
            }
            pcm_cap_samples_ = cap_samples;
        }

        pcm_len_samples_ = 0;
        capture_start_ms_ = millis();
        captured_work_mode_ = snap.work_mode;
        capturing_ = true;
        postRecordingState(true);
        LOG_INFO("ASR", "recording started (cap=%us)",
                 static_cast<unsigned>(pcm_cap_samples_ / voice::kPcmSampleRate));
        return true;
    }

    void VoiceRecognizer::feedCapture()
    {
        if (!capturing_)
        {
            return;
        }

        if (pcm_len_samples_ >= pcm_cap_samples_)
        {
            /* 达到最大录音时长，自动结束识别 */
            LOG_WARNING("ASR", "buffer full, auto finish");
            finishCapture();
            return;
        }

        static int16_t chunk[voice::kFeedChunkSamples];
        const size_t n = Mic::instance().Read(chunk, voice::kFeedChunkSamples);
        const size_t space = pcm_cap_samples_ - pcm_len_samples_;
        const size_t copy = (n < space) ? n : space;
        if (copy > 0)
        {
            memcpy(pcm_buf_ + pcm_len_samples_, chunk, copy * sizeof(int16_t));
            pcm_len_samples_ += copy;
        }
    }

    /*
     * C3 修复：原 finishCapture() 同步 HTTP 10s+ 阻塞 MainTask，
     * 现拆分为两步：
     *   1) finishCapture() —— 截断 PCM + Mic::end() + 投递 ASR 任务，立即返回；
     *   2) asrTaskLoop() —— 后台消费队列，执行 HTTP POST + JSON 解析 + 上报。
     * 这样录音结束 MainTask 立即恢复 5ms tick，长录音场景不再卡死键盘/TCP。
     */
    void VoiceRecognizer::finishCapture()
    {
        if (!capturing_)
        {
            return;
        }
        capturing_ = false;
        postRecordingState(false);

        const uint32_t duration_ms = millis() - capture_start_ms_;
        Mic::instance().end();

        if (duration_ms < voice::kMinRecordMs || pcm_len_samples_ == 0)
        {
            LOG_INFO("ASR", "record too short (%ums), discarded",
                     static_cast<unsigned>(duration_ms));
            free(pcm_buf_);
            pcm_buf_ = nullptr;
            pcm_cap_samples_ = 0;
            pcm_len_samples_ = 0;
            return;
        }

        /* 抓取设备快照（cuid / dev_pid / auto_enter），后台识别时 Configuration 可能变更 */
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);

        /* 移交 PCM 至后台任务：当前录音缓冲所有权移交给队列，
         * 后续 startCapture() 因 pcm_buf_==nullptr 会重新分配。 */
        AsrJob job;
        job.pcm = pcm_buf_;
        job.samples = pcm_len_samples_;
        job.duration_ms = duration_ms;
        job.auto_enter = (snap.voice_auto_enter != 0);
        job.dev_pid = (snap.voice_dev_pid != 0) ? snap.voice_dev_pid
                                                : voice::kDefaultDevPid;
        if (snap.voice_cuid[0] != '\0')
        {
            strncpy(job.cuid, snap.voice_cuid, sizeof(job.cuid) - 1);
            job.cuid[sizeof(job.cuid) - 1] = '\0';
        }
        else
        {
            strncpy(job.cuid, voice::kDefaultCuid, sizeof(job.cuid) - 1);
            job.cuid[sizeof(job.cuid) - 1] = '\0';
        }

        if (!ensureAsrTask())
        {
            LOG_ERROR("ASR", "asr task start failed, drop job");
            free(job.pcm);
            pcm_buf_ = nullptr;
            pcm_cap_samples_ = 0;
            pcm_len_samples_ = 0;
            return;
        }

        /* 队列容量=1；若上一段识别未完成，直接丢弃旧任务（提示重发） */
        if (asr_job_pending_)
        {
            LOG_WARNING("ASR", "previous job still running, drop oldest");
            free(asr_queue_[asr_queue_head_].pcm);
            asr_queue_[asr_queue_head_] = AsrJob{};
            asr_queue_head_ = (asr_queue_head_ + 1) % kAsrQueueDepth;
        }
        asr_queue_[asr_queue_tail_] = job;
        asr_queue_tail_ = (asr_queue_tail_ + 1) % kAsrQueueDepth;
        asr_job_pending_ = true;

        /* 移交所有权：录音缓冲清零，下一次 startCapture 重新分配 */
        pcm_buf_ = nullptr;
        pcm_cap_samples_ = 0;
        pcm_len_samples_ = 0;

        /* 唤醒后台任务 */
        TaskHandle_t h = static_cast<TaskHandle_t>(asr_task_handle_);
        if (h != nullptr)
        {
            xTaskNotifyGive(h);
        }
        LOG_INFO("ASR", "capture finished (%ums), job queued",
                 static_cast<unsigned>(duration_ms));
    }

    bool VoiceRecognizer::ensureAsrTask()
    {
        if (asr_task_handle_ != nullptr)
        {
            return true;
        }
        BaseType_t ok = xTaskCreate(
            &VoiceRecognizer::asrTaskEntry,
            "EKeysAsr",
            /* 8KB 栈：HTTPClient + ArduinoJson + snprintf 足够；
             * 北京 ASR 域名 TLS 由 ESP-IDF mbedtls 处理，需要略大栈。 */
            8192,
            this,
            /* 优先级低于 MainTask（1）但不阻塞 UI 主循环 */
            1,
            reinterpret_cast<TaskHandle_t *>(&asr_task_handle_));
        return ok == pdPASS;
    }

    void VoiceRecognizer::asrTaskEntry(void *arg)
    {
        VoiceRecognizer *self = static_cast<VoiceRecognizer *>(arg);
        if (self != nullptr)
        {
            self->asrTaskLoop();
        }
        vTaskDelete(nullptr);
    }

    void VoiceRecognizer::asrTaskLoop()
    {
        for (;;)
        {
            /* 等待 finishCapture() 的唤醒（无限等待，无任务时挂起不耗 CPU） */
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            while (asr_job_pending_)
            {
                const uint8_t idx = asr_queue_head_;
                AsrJob job = asr_queue_[idx];
                asr_queue_[idx] = AsrJob{};
                asr_queue_head_ = (asr_queue_head_ + 1) % kAsrQueueDepth;
                asr_job_pending_ = false;

                /* token */
                const char *token = AsrTokenCache::instance().getToken();
                if (token == nullptr)
                {
                    LOG_ERROR("ASR", "no valid token, check baidu keys");
                    free(job.pcm);
                    continue;
                }

                /* URL：dev_pid / cuid 来自抓取时的快照 */
                char url[256];
                snprintf(url, sizeof(url), "%s?dev_pid=%u&cuid=%s&token=%s",
                         voice::kAsrUrlBase,
                         static_cast<unsigned>(job.dev_pid),
                         job.cuid, token);

                /* POST raw PCM */
                const size_t byte_len = job.samples * sizeof(int16_t);
                HTTPClient http;
                http.begin(url);
                http.addHeader("Content-Type", voice::kAsrContentType);
                http.setTimeout(10000);
                LOG_INFO("ASR", "recognizing %ums pcm...",
                         static_cast<unsigned>(job.duration_ms));
                const int code = http.POST(reinterpret_cast<uint8_t *>(job.pcm),
                                           byte_len);
                if (code != 200)
                {
                    LOG_ERROR("ASR", "http %d", code);
                    http.end();
                    free(job.pcm);
                    continue;
                }

                JsonDocument doc;
                const DeserializationError err = deserializeJson(doc, http.getString());
                http.end();
                if (err)
                {
                    LOG_ERROR("ASR", "json: %s", err.c_str());
                    free(job.pcm);
                    continue;
                }
                if ((doc["err_no"] | -1) != 0)
                {
                    LOG_ERROR("ASR", "baidu err %d: %s",
                              doc["err_no"] | -1, doc["err_msg"] | "unknown");
                    free(job.pcm);
                    continue;
                }

                const char *text = doc["result"][0] | "";
                if (text[0] == '\0')
                {
                    LOG_INFO("ASR", "empty result");
                    free(job.pcm);
                    continue;
                }
                LOG_INFO("ASR", "text: %s", text);

                /* 主通道：CMD_VOICE_TEXT 推送 App */
                SerialProtocol::instance().sendVoiceText(text);

                /* 兜底：TCP 未连接且纯 ASCII → HID 注入 */
                const bool tcp_online = TcpChannel::instance().isConnected();
                bool ascii_only = !tcp_online;
                for (const char *p = text; ascii_only && *p != '\0'; ++p)
                {
                    const unsigned char c = static_cast<unsigned char>(*p);
                    /* D1 修复：控制字符 (<0x20) 也视为非 ASCII，避免破坏 "纯 ASCII 注入" 语义 */
                    if (c < 0x20 || c > 0x7E)
                    {
                        ascii_only = false;
                    }
                }
                if (ascii_only)
                {
                    IKeyboard *kb = AppContext::instance().keyboard();
                    if (kb != nullptr)
                    {
                        for (const char *p = text; *p != '\0'; ++p)
                        {
                            const char c = *p;
                            if (c == ' ')
                            {
                                kb->press(0x2C);
                                kb->release(0x2C);
                            }
                            else if (c >= 0x21 && c <= 0x7E)
                            {
                                char name[2] = {c, '\0'};
                                const uint8_t keycode = resolveKeyName(String(name));
                                if (keycode != 0)
                                {
                                    kb->press(keycode);
                                    kb->release(keycode);
                                }
                            }
                            delay(voice::kAsciiInjectDelayMs);
                        }
                        if (job.auto_enter)
                        {
                            kb->press(0x28); // KEY_RETURN
                            kb->release(0x28);
                        }
                    }
                }

                free(job.pcm);
            }
        }
    }

} // namespace ekeys
