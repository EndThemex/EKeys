/*
 * AudioPad.cpp
 *
 * 见 AudioPad.h。
 */

#include "AudioPad.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <stdio.h>
#include <string.h>

#include <SimpleIni.h>

#include "../logging/LogManager.h"
#include "../message_types.h"
#include "../services/ConfigStore.h"
#include "../tasks/DisplayTask.h"
#include "Speaker.h"

namespace ekeys
{

    namespace
    {

        /* 名字白名单（与 cmd_audio.cpp 保持一致）：^[a-z0-9_]{1,20}\.(mp3|wav)$ */
        bool validPadName(const char *name)
        {
            if (name == nullptr)
            {
                return false;
            }
            const size_t len = strlen(name);
            if (len < 5 || len > AudioPad::kNameLenMax)
            {
                return false;
            }
            const size_t base_len = len - 4; /* ".mp3" / ".wav" 共 4 字符 */
            for (size_t i = 0; i < base_len; ++i)
            {
                const char c = name[i];
                const bool ok = (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '_';
                if (!ok)
                {
                    return false;
                }
            }
            return strcmp(name + base_len, ".mp3") == 0 ||
                   strcmp(name + base_len, ".wav") == 0;
        }

    } // namespace

    AudioPad &AudioPad::instance()
    {
        static AudioPad inst;
        return inst;
    }

    void AudioPad::load()
    {
        CSimpleIniA ini(/*bIsUtf8=*/true);
        if (!ConfigStore::loadGlobal(kPersistPath, ini))
        {
            LOG_INFO("PAD", "no %s, empty bindings", kPersistPath);
            return;
        }

        uint8_t loaded = 0;
        /* 校验（含 SPIFFS.exists 闪存操作）在临界区外完成，最后一次性写入。
         * 禁止在 taskENTER_CRITICAL 内做 SPIFFS/闪存调用（2026-09-11 修复）。 */
        char valid[kPadCount][kNameLenMax + 1]{};
        for (uint8_t k = 1; k <= kPadCount; ++k)
        {
            char key[8];
            snprintf(key, sizeof(key), "pad%u", static_cast<unsigned>(k));
            const char *val = ini.GetValue("pads", key, "");
            if (val[0] == '\0')
            {
                continue;
            }
            char path[kNameLenMax + 2];
            snprintf(path, sizeof(path), "/%s", val);
            /* 失效条目（文件被重烧 SPIFFS 清掉 / 名字非法）丢弃 */
            if (!validPadName(val) || !SPIFFS.exists(path))
            {
                LOG_WARNING("PAD", "pad%u: drop invalid binding '%s'",
                            static_cast<unsigned>(k), val);
                continue;
            }
            snprintf(valid[k - 1], sizeof(valid[k - 1]), "%s", val);
            ++loaded;
        }
        taskENTER_CRITICAL(&lock_);
        memcpy(bindings_, valid, sizeof(bindings_));
        taskEXIT_CRITICAL(&lock_);
        LOG_INFO("PAD", "loaded %u bindings from %s",
                 static_cast<unsigned>(loaded), kPersistPath);
    }

    bool AudioPad::setBinding(uint8_t key, const char *file)
    {
        if (key < 1 || key > kPadCount || file == nullptr)
        {
            return false;
        }
        if (file[0] != '\0')
        {
            char path[kNameLenMax + 2];
            snprintf(path, sizeof(path), "/%s", file);
            if (!validPadName(file) || !SPIFFS.exists(path))
            {
                LOG_WARNING("PAD", "setBinding key%u: file missing '%s'",
                            static_cast<unsigned>(key), file);
                return false;
            }
        }

        {
            taskENTER_CRITICAL(&lock_);
            snprintf(bindings_[key - 1], sizeof(bindings_[key - 1]), "%s", file);
            taskEXIT_CRITICAL(&lock_);
        }
        postPadMessage(); /* 绑定变更 → 刷音效页（若在屏上） */
        return true;
    }

    bool AudioPad::persist()
    {
        CSimpleIniA ini(/*bIsUtf8=*/true);
        for (uint8_t k = 1; k <= kPadCount; ++k)
        {
            char key[8];
            snprintf(key, sizeof(key), "pad%u", static_cast<unsigned>(k));
            char name[kNameLenMax + 1];
            copyBinding(k, name, sizeof(name));
            ini.SetValue("pads", key, name, /*aComment=*/nullptr, /*aForceCreate=*/true);
        }
        if (!ConfigStore::saveGlobal(kPersistPath, ini))
        {
            LOG_ERROR("PAD", "persist %s failed", kPersistPath);
            return false;
        }
        return true;
    }

    uint8_t AudioPad::clearBindingsOf(const char *file)
    {
        if (file == nullptr || file[0] == '\0')
        {
            return 0;
        }
        uint8_t cleared = 0;
        taskENTER_CRITICAL(&lock_);
        for (uint8_t k = 0; k < kPadCount; ++k)
        {
            if (strcmp(bindings_[k], file) == 0)
            {
                bindings_[k][0] = '\0';
                ++cleared;
            }
        }
        taskEXIT_CRITICAL(&lock_);
        return cleared;
    }

    bool AudioPad::startPlayback(const char *name, uint8_t key)
    {
        Speaker &spk = Speaker::instance();
        /* 换播先停旧流（同库单 Audio 实例，connecttoFS 前必须 stop） */
        if (spk.isRunning())
        {
            spk.Stop();
        }
        char path[kNameLenMax + 2];
        snprintf(path, sizeof(path), "/%s", name);
        /* 录音互斥 / 文件缺失 / 解码启动失败 → 拒播（Speaker 内部守卫） */
        if (!spk.PlayLocalAudio(path))
        {
            LOG_WARNING("PAD", "play '%s' rejected", path);
            return false;
        }
        taskENTER_CRITICAL(&lock_);
        playing_key_ = key;
        snprintf(playing_name_, sizeof(playing_name_), "%s", name);
        taskEXIT_CRITICAL(&lock_);
        return true;
    }

    bool AudioPad::trigger(uint8_t key)
    {
        if (key < 1 || key > kPadCount)
        {
            return false;
        }
        char name[kNameLenMax + 1];
        copyBinding(key, name, sizeof(name));
        if (name[0] == '\0')
        {
            LOG_DEBUG("PAD", "key%u unbound", static_cast<unsigned>(key));
            return false;
        }
        return startPlayback(name, key);
    }

    bool AudioPad::playFile(const char *file)
    {
        if (file == nullptr || file[0] == '\0' || !validPadName(file))
        {
            return false;
        }
        char path[kNameLenMax + 2];
        snprintf(path, sizeof(path), "/%s", file);
        if (!SPIFFS.exists(path))
        {
            LOG_WARNING("PAD", "playFile missing '%s'", path);
            return false;
        }
        return startPlayback(file, 0);
    }

    void AudioPad::stop()
    {
        uint8_t was_playing;
        taskENTER_CRITICAL(&lock_);
        was_playing = playing_key_;
        playing_key_ = 0;
        playing_name_[0] = '\0';
        play_req_pending_ = false; /* 连带取消未消费的播放请求 */
        taskEXIT_CRITICAL(&lock_);
        Speaker::instance().Stop();
        if (was_playing != 0)
        {
            postPadMessage(); /* 清键位高亮 */
        }
    }

    bool AudioPad::requestTrigger(uint8_t key)
    {
        if (key < 1 || key > kPadCount)
        {
            return false;
        }
        char name[kNameLenMax + 1];
        copyBinding(key, name, sizeof(name));
        if (name[0] == '\0')
        {
            LOG_DEBUG("PAD", "key%u unbound", static_cast<unsigned>(key));
            return false;
        }
        taskENTER_CRITICAL(&lock_);
        play_req_key_ = key;
        play_req_pending_ = true;
        taskEXIT_CRITICAL(&lock_);
        return true;
    }

    void AudioPad::requestStop()
    {
        uint8_t was_playing;
        taskENTER_CRITICAL(&lock_);
        was_playing = playing_key_;
        playing_key_ = 0;
        playing_name_[0] = '\0';
        stop_req_pending_ = true;
        taskEXIT_CRITICAL(&lock_);
        if (was_playing != 0)
        {
            postPadMessage(); /* 清键位高亮 */
        }
    }

    void AudioPad::service()
    {
        /*
         * 消费 DisplayTask（Core 0）的播放/停止请求。本方法在 MainTask
         * （与 Speaker::loop 同任务）执行，Audio 实例启停与喂流天然串行；
         * 先处理 stop 再处理 play（换播 = 先停旧流再启新流）。
         */
        bool do_stop;
        bool do_play;
        uint8_t key;
        taskENTER_CRITICAL(&lock_);
        do_stop = stop_req_pending_;
        stop_req_pending_ = false;
        do_play = play_req_pending_;
        play_req_pending_ = false;
        key = play_req_key_;
        taskEXIT_CRITICAL(&lock_);

        if (do_stop)
        {
            Speaker::instance().Stop();
        }
        if (!do_play)
        {
            return;
        }
        char name[kNameLenMax + 1];
        copyBinding(key, name, sizeof(name));
        if (name[0] == '\0')
        {
            return; /* 请求后绑定被清除（cmd_audio stop/改绑） */
        }
        startPlayback(name, key);
        /* 重投消息同步高亮：播放被拒（录音互斥等）时纠正 requestTrigger
         * 的乐观高亮（postPadMessage 携带当前真实 playing_key_） */
        postPadMessage();
    }

    void AudioPad::loop()
    {
        uint8_t key;
        char name[kNameLenMax + 1];
        taskENTER_CRITICAL(&lock_);
        key = playing_key_;
        memcpy(name, playing_name_, sizeof(name));
        taskEXIT_CRITICAL(&lock_);

        if (key == 0)
        {
            return; /* 无键位播放（试播结束无需刷 UI） */
        }
        if (Speaker::instance().isRunning())
        {
            return; /* 还在播 */
        }
        /* 播完：清 playing_key_ + 高亮。试播（playing_name_ 非空但 key=0）
         * 播完无需动作；这里只处理键位播放。 */
        taskENTER_CRITICAL(&lock_);
        if (playing_key_ != 0 && strcmp(playing_name_, name) == 0)
        {
            /* 期间未被 stop()/新播放覆盖才清，避免误清新播放的高亮 */
            playing_key_ = 0;
            playing_name_[0] = '\0';
            taskEXIT_CRITICAL(&lock_);
            postPadMessage();
        }
        else
        {
            taskEXIT_CRITICAL(&lock_);
        }
    }

    void AudioPad::snapshotBindings(char out[kPadCount][kNameLenMax + 1]) const
    {
        taskENTER_CRITICAL(&lock_);
        memcpy(out, bindings_, sizeof(bindings_));
        taskEXIT_CRITICAL(&lock_);
    }

    bool AudioPad::isPlayingFile(const char *file) const
    {
        if (file == nullptr)
        {
            return false;
        }
        /* 先拷名再查 Speaker（库调用不放临界区） */
        char name[kNameLenMax + 1];
        taskENTER_CRITICAL(&lock_);
        memcpy(name, playing_name_, sizeof(name));
        taskEXIT_CRITICAL(&lock_);
        return name[0] != '\0' && Speaker::instance().isRunning() &&
               strcmp(name, file) == 0;
    }

    bool AudioPad::isPlayingKey() const
    {
        taskENTER_CRITICAL(&lock_);
        const bool v = playing_key_ != 0;
        taskEXIT_CRITICAL(&lock_);
        return v;
    }

    void AudioPad::copyBinding(uint8_t key, char *out, size_t out_len) const
    {
        if (key < 1 || key > kPadCount)
        {
            out[0] = '\0';
            return;
        }
        taskENTER_CRITICAL(&lock_);
        snprintf(out, out_len, "%s", bindings_[key - 1]);
        taskEXIT_CRITICAL(&lock_);
    }

    void AudioPad::postPadMessage()
    {
        DisplayMessage msg;
        msg.type = DisplayMessageType::AudioPad;
        snapshotBindings(msg.audio_pad.files);
        taskENTER_CRITICAL(&lock_);
        msg.audio_pad.playing_key = playing_key_;
        taskEXIT_CRITICAL(&lock_);
        DisplayTask::instance().post(msg, 0);
    }

} // namespace ekeys

extern "C" void ui_audio_pad_stop(void)
{
    /* DisplayTask（Core 0）上下文：只置请求，Speaker::Stop 由
     * MainTask::loop 的 service() 执行（Audio 实例跨核串行保护） */
    ekeys::AudioPad::instance().requestStop();
}
