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
#include <mbedtls/base64.h>

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
#include "voice/TencentAsrSigner.h"
#include "voice/VoiceConfig.h"

namespace ekeys
{

    VoiceRecognizer &VoiceRecognizer::instance()
    {
        static VoiceRecognizer inst;
        return inst;
    }

    /*
     * F3 修复：capturing_ 跨任务读写，外部读取（DisplayTask 频谱 / HA 屏 /
     * cmd_config 副作用判断）都走临界区；写入侧（startCapture/finishCapture）配对。
     */
    bool VoiceRecognizer::isCapturing() const
    {
        bool v;
        taskENTER_CRITICAL(&asr_lock_);
        v = capturing_;
        taskEXIT_CRITICAL(&asr_lock_);
        return v;
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
        /* F3 修复：capturing_ 跨任务访问，加锁 */
        bool expected = false;
        taskENTER_CRITICAL(&asr_lock_);
        if (capturing_)
        {
            taskEXIT_CRITICAL(&asr_lock_);
            return false;
        }
        capturing_ = true;
        taskEXIT_CRITICAL(&asr_lock_);

        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        if (!canWork())
        {
            /* 输出每条具体失败原因，便于诊断语音未触发问题 */
            if (snap.work_mode != 0)
            {
                LOG_WARNING("ASR", "rejected: work_mode=%u (need USB=0)", snap.work_mode);
            }
            if (snap.voice_enable == 0)
            {
                LOG_WARNING("ASR", "rejected: voice_enable=0");
            }
            if (!WiFiManager::instance().isConnected())
            {
                LOG_WARNING("ASR", "rejected: wifi not connected");
            }
            if (suspended_)
            {
                LOG_WARNING("ASR", "rejected: suspended (music screen)");
            }
            /* F3 修复：失败回滚 capturing_ */
            taskENTER_CRITICAL(&asr_lock_);
            capturing_ = false;
            taskEXIT_CRITICAL(&asr_lock_);
            return false;
        }

        /* BCLK=IO10 与 Speaker 互斥（PINOUT §2.7）：先停 Speaker 再 begin Mic */
        prepareI2sForMicCapture();

        if (!Mic::instance().begin())
        {
            taskENTER_CRITICAL(&asr_lock_);
            capturing_ = false;
            taskEXIT_CRITICAL(&asr_lock_);
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
                taskENTER_CRITICAL(&asr_lock_);
                capturing_ = false;
                taskEXIT_CRITICAL(&asr_lock_);
                return false;
            }
            pcm_cap_samples_ = cap_samples;
        }

        pcm_len_samples_ = 0;
        capture_start_ms_ = millis();
        last_heartbeat_ms_ = 0;
        captured_work_mode_ = snap.work_mode;
        /* capturing_ 已在入口置位 */
        postRecordingState(true);
        LOG_INFO("ASR", "recording started (cap=%us)",
                 static_cast<unsigned>(pcm_cap_samples_ / voice::kPcmSampleRate));
        return true;
    }

    void VoiceRecognizer::feedCapture()
    {
        /* F3 修复：capturing_ 加锁读 */
        bool active;
        taskENTER_CRITICAL(&asr_lock_);
        active = capturing_;
        taskEXIT_CRITICAL(&asr_lock_);
        if (!active)
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
        /* 每 2s 打一次心跳，确认录音仍在进行（避免只看到末尾"capture finished"难以判断时长） */
        const uint32_t now = millis();
        if ((now - capture_start_ms_) - last_heartbeat_ms_ >= 2000U)
        {
            last_heartbeat_ms_ = (now - capture_start_ms_);
            LOG_DEBUG("ASR", "recording... %ums / %ums (%u/%u samples)",
                      static_cast<unsigned>((now - capture_start_ms_)),
                      static_cast<unsigned>(pcm_cap_samples_ / voice::kPcmSampleRate * 1000U),
                      static_cast<unsigned>(pcm_len_samples_),
                      static_cast<unsigned>(pcm_cap_samples_));
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
        /* F3 修复：capturing_ 加锁，flip 一次确保与 isCapturing 一致 */
        taskENTER_CRITICAL(&asr_lock_);
        if (!capturing_)
        {
            taskEXIT_CRITICAL(&asr_lock_);
            return;
        }
        capturing_ = false;
        taskEXIT_CRITICAL(&asr_lock_);

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

        /* 抓取设备快照（凭证 / cuid / auto_enter），后台识别时 Configuration 可能变更 */
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);

        /* 移交 PCM 至后台任务：当前录音缓冲所有权移交给队列，
         * 后续 startCapture() 因 pcm_buf_==nullptr 会重新分配。 */
        AsrJob job;
        job.pcm = pcm_buf_;
        job.samples = pcm_len_samples_;
        job.duration_ms = duration_ms;
        job.auto_enter = (snap.voice_auto_enter != 0);
        strncpy(job.secret_id, snap.voice_tencent_secret_id,
                sizeof(job.secret_id) - 1);
        job.secret_id[sizeof(job.secret_id) - 1] = '\0';
        strncpy(job.secret_key, snap.voice_tencent_secret_key,
                sizeof(job.secret_key) - 1);
        job.secret_key[sizeof(job.secret_key) - 1] = '\0';
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
        /* F3 修复：asr_queue_* 与 asr_job_pending_ 由 MainTask 写、ASR Task 读，加锁 */
        taskENTER_CRITICAL(&asr_lock_);
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
        taskEXIT_CRITICAL(&asr_lock_);

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
            /* 12KB 栈（阶段 08）：base64 编码临时区 + HTTPClient TLS +
             * TencentAsrSigner（canonical / stringToSign 缓冲）。
             * 腾讯云域名 TLS 由 ESP-IDF mbedtls 处理，需要略大栈。 */
            12288,
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
                /* F3 修复：队列与 pending 出队加锁，避免 MainTask 写时丢 job */
                AsrJob job;
                uint8_t idx;
                taskENTER_CRITICAL(&asr_lock_);
                if (!asr_job_pending_)
                {
                    taskEXIT_CRITICAL(&asr_lock_);
                    break;
                }
                idx = asr_queue_head_;
                job = asr_queue_[idx];
                asr_queue_[idx] = AsrJob{};
                asr_queue_head_ = (asr_queue_head_ + 1) % kAsrQueueDepth;
                asr_job_pending_ = false;
                taskEXIT_CRITICAL(&asr_lock_);

                /* 空凭证短路（App 未配置腾讯云 SecretId/Key） */
                if (job.secret_id[0] == '\0' || job.secret_key[0] == '\0')
                {
                    LOG_ERROR("ASR", "no secret, skip (set voice_tencent_secret_id/key)");
                    free(job.pcm);
                    continue;
                }

                /* 构造请求 JSON：prefix + base64(PCM) + suffix，PSRAM 单缓冲
                 * 直写，避免中间 base64 副本（30s → base64 ≈ 1.28MB） */
                const size_t pcm_bytes = job.samples * sizeof(int16_t);
                const size_t b64_len = ((pcm_bytes + 2) / 3) * 4;
                /* prefix ≈ 90B（引擎/格式字段） + suffix ≈ 24B（DataLen 收尾） */
                const size_t json_overhead = 160;
                char *payload = static_cast<char *>(
                    ps_malloc(json_overhead + b64_len + 1));
                if (payload == nullptr)
                {
                    LOG_ERROR("ASR", "payload alloc failed (%uKB)",
                              static_cast<unsigned>((b64_len + json_overhead) / 1024));
                    free(job.pcm);
                    continue;
                }
                const int prefix_len = snprintf(
                    payload, json_overhead,
                    "{\"EngSerViceType\":\"%s\",\"SourceType\":1,"
                    "\"VoiceFormat\":\"%s\",\"Data\":\"",
                    voice::kTencentAsrEngine, voice::kTencentAsrVoiceFormat);
                size_t b64_written = 0;
                const int b64_rc = mbedtls_base64_encode(
                    reinterpret_cast<unsigned char *>(payload + prefix_len),
                    b64_len + 1, &b64_written,
                    reinterpret_cast<const unsigned char *>(job.pcm), pcm_bytes);
                const int suffix_len =
                    snprintf(payload + prefix_len + b64_written,
                             json_overhead - prefix_len,
                             "\",\"DataLen\":%u}",
                             static_cast<unsigned>(pcm_bytes));
                if (prefix_len <= 0 || b64_rc != 0 || suffix_len <= 0)
                {
                    LOG_ERROR("ASR", "payload build failed (b64=%d)", b64_rc);
                    free(payload);
                    free(job.pcm);
                    continue;
                }
                const size_t payload_len =
                    static_cast<size_t>(prefix_len) + b64_written +
                    static_cast<size_t>(suffix_len);

                /* TC3 签名（失败：凭证空 / NTP 未同步） */
                voice::SignedRequest sig;
                if (!voice::signRequest(job.secret_id, job.secret_key,
                                        payload, payload_len, sig))
                {
                    LOG_ERROR("ASR", "sign failed (no secret / ntp not synced)");
                    free(payload);
                    free(job.pcm);
                    continue;
                }

                /* POST JSON（Host 头由 HTTPClient 按 URL 自动携带，勿重复添加） */
                char url[64];
                snprintf(url, sizeof(url), "https://%s", voice::kTencentAsrHost);
                HTTPClient http;
                http.begin(url);
                http.addHeader("Content-Type", voice::kTencentAsrContentType);
                http.addHeader("X-TC-Action", voice::kTencentAsrAction);
                http.addHeader("X-TC-Timestamp", sig.timestamp);
                http.addHeader("X-TC-Version", voice::kTencentAsrVersion);
                http.addHeader("X-TC-Region", voice::kTencentAsrRegion);
                http.addHeader("Authorization", sig.authorization);
                http.setTimeout(voice::kTencentAsrTimeoutMs);
                LOG_INFO("ASR", "recognizing %ums pcm (%uKB payload)...",
                         static_cast<unsigned>(job.duration_ms),
                         static_cast<unsigned>(payload_len / 1024));
                const int code = http.POST(
                    reinterpret_cast<uint8_t *>(payload),
                    static_cast<size_t>(payload_len));
                free(payload);
                if (code <= 0)
                {
                    LOG_ERROR("ASR", "http %d", code);
                    http.end();
                    free(job.pcm);
                    continue;
                }

                /* 错误响应（4xx）也带 JSON body，一并读取解析 */
                JsonDocument doc;
                const DeserializationError err =
                    deserializeJson(doc, http.getString());
                http.end();
                if (err)
                {
                    LOG_ERROR("ASR", "json: %s", err.c_str());
                    free(job.pcm);
                    continue;
                }
                const char *err_code =
                    doc["Response"]["Error"]["Code"] | "";
                if (err_code[0] != '\0')
                {
                    LOG_ERROR("ASR", "tencent err %s: %s", err_code,
                              doc["Response"]["Error"]["Message"] | "unknown");
                    free(job.pcm);
                    continue;
                }
                if (code != 200)
                {
                    LOG_ERROR("ASR", "http %d", code);
                    free(job.pcm);
                    continue;
                }

                const char *text = doc["Response"]["Result"] | "";
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
                        unsigned injected = 0;
                        for (const char *p = text; *p != '\0'; ++p)
                        {
                            const char c = *p;
                            if (c == ' ')
                            {
                                kb->press(0x2C);
                                kb->release(0x2C);
                                ++injected;
                            }
                            else if (c >= 0x21 && c <= 0x7E)
                            {
                                char name[2] = {c, '\0'};
                                const uint8_t keycode = resolveKeyName(String(name));
                                if (keycode != 0)
                                {
                                    kb->press(keycode);
                                    kb->release(keycode);
                                    ++injected;
                                }
                            }
                            delay(voice::kAsciiInjectDelayMs);
                        }
                        if (job.auto_enter)
                        {
                            kb->press(0x28); // KEY_RETURN
                            kb->release(0x28);
                        }
                        LOG_INFO("ASR", "hid injected %u chars%s",
                                 injected, job.auto_enter ? " +enter" : "");
                    }
                    else
                    {
                        LOG_WARNING("ASR", "hid keyboard unavailable, inject skipped");
                    }
                }
                else
                {
                    /* TCP 在线时文本已走 0x0c 推送；离线且含非 ASCII 只能丢弃 */
                    LOG_INFO("ASR", "hid inject skipped (%s)",
                             tcp_online ? "app channel online" : "non-ascii & tcp offline");
                }

                free(job.pcm);
            }
        }
    }

} // namespace ekeys
