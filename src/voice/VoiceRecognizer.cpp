/*
 * VoiceRecognizer.cpp
 *
 * 见 VoiceRecognizer.h。
 */

#include "VoiceRecognizer.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "app/AppContext.h"
#include "audio/Mic.h"
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
            return;
        }

        /* token */
        const char *token = AsrTokenCache::instance().getToken();
        if (token == nullptr)
        {
            LOG_ERROR("ASR", "no valid token, check baidu keys");
            return;
        }

        /* URL：dev_pid / cuid 来自设置 */
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        const char *cuid = (snap.voice_cuid[0] != '\0') ? snap.voice_cuid
                                                        : voice::kDefaultCuid;
        char url[256];
        snprintf(url, sizeof(url), "%s?dev_pid=%u&cuid=%s&token=%s",
                 voice::kAsrUrlBase,
                 static_cast<unsigned>(snap.voice_dev_pid != 0
                                           ? snap.voice_dev_pid
                                           : voice::kDefaultDevPid),
                 cuid, token);

        /* POST raw PCM */
        const size_t byte_len = pcm_len_samples_ * sizeof(int16_t);
        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", voice::kAsrContentType);
        http.setTimeout(10000);
        LOG_INFO("ASR", "recognizing %ums pcm...", static_cast<unsigned>(duration_ms));
        const int code = http.POST(reinterpret_cast<uint8_t *>(pcm_buf_),
                                   static_cast<size_t>(byte_len));
        if (code != 200)
        {
            LOG_ERROR("ASR", "http %d", code);
            http.end();
            return;
        }

        JsonDocument doc;
        const DeserializationError err = deserializeJson(doc, http.getString());
        http.end();
        if (err)
        {
            LOG_ERROR("ASR", "json: %s", err.c_str());
            return;
        }
        if ((doc["err_no"] | -1) != 0)
        {
            LOG_ERROR("ASR", "baidu err %d: %s",
                      doc["err_no"] | -1, doc["err_msg"] | "unknown");
            return;
        }

        const char *text = doc["result"][0] | "";
        if (text[0] == '\0')
        {
            LOG_INFO("ASR", "empty result");
            return;
        }
        LOG_INFO("ASR", "text: %s", text);

        /* 主通道：CMD_VOICE_TEXT 推送 App */
        SerialProtocol::instance().sendVoiceText(text);

        /* 兜底：TCP 未连接且纯 ASCII → HID 注入 */
        const bool tcp_online = TcpChannel::instance().isConnected();
        bool ascii_only = !tcp_online;
        for (const char *p = text; ascii_only && *p != '\0'; ++p)
        {
            if (static_cast<unsigned char>(*p) > 0x7E)
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
                if (snap.voice_auto_enter != 0)
                {
                    kb->press(0x28); // KEY_RETURN
                    kb->release(0x28);
                }
            }
        }
    }

} // namespace ekeys
