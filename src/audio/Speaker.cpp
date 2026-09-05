/*
 * Speaker.cpp
 *
 * 见 Speaker.h。
 */

#include "Speaker.h"

#include <Arduino.h>
#include <Audio.h>
#include <SPIFFS.h>

#include "hardware/PinMap.h"
#include "logging/LogManager.h"
#include "voice/VoiceRecognizer.h"

namespace ekeys {

Speaker &Speaker::instance()
{
    static Speaker inst;
    return inst;
}

void Speaker::begin()
{
    if (inited_)
    {
        return;
    }
    auto *audio = new Audio();
    audio->setPinout(kPinI2sBclkSpeaker, kPinI2sLrclkSpeaker, kPinI2sDataSpeaker);
    audio->setVolume(12);  // 默认中等音量（0~21）
    impl_ = audio;
    inited_ = true;
    LOG_INFO("SPK", "MAX98357 ready (bclk=%u lrc=%u dout=%u)",
             kPinI2sBclkSpeaker, kPinI2sLrclkSpeaker, kPinI2sDataSpeaker);
}

void Speaker::loop()
{
    if (inited_)
    {
        static_cast<Audio *>(impl_)->loop();
    }
}

bool Speaker::isRunning() const
{
    /*
     * ESP32-audioI2S 的 Audio::isRunning() 未声明 const，但语义上只是查询。
     * 这里 const_cast 仅用于放宽 this 的 const 限定，不修改对象。
     */
    if (!inited_)
    {
        return false;
    }
    return const_cast<Audio *>(static_cast<const Audio *>(impl_))->isRunning();
}

void Speaker::SetVolume(uint8_t volume_0_21)
{
    if (!inited_)
    {
        begin();
    }
    if (volume_0_21 > 21)
    {
        volume_0_21 = 21;
    }
    static_cast<Audio *>(impl_)->setVolume(volume_0_21);
}

void Speaker::applyDeviceVolume(uint8_t device_volume)
{
    SetVolume(device_volume / 5);
}

bool Speaker::PlayRemoteAudio(const char *url)
{
    if (url == nullptr || url[0] == '\0')
    {
        return false;
    }
    /* BCLK=IO10 与 Mic 互斥（PINOUT §2.7）：录音期间拒绝播放 */
    if (VoiceRecognizer::instance().isCapturing())
    {
        LOG_WARNING("SPK", "mic is recording, reject remote play");
        return false;
    }
    if (!inited_)
    {
        begin();
    }
    LOG_INFO("SPK", "remote: %s", url);
    return static_cast<Audio *>(impl_)->connecttohost(url);
}

bool Speaker::PlayLocalAudio(const char *path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }
    /* BCLK=IO10 与 Mic 互斥（PINOUT §2.7）：录音期间拒绝播放 */
    if (VoiceRecognizer::instance().isCapturing())
    {
        LOG_WARNING("SPK", "mic is recording, reject local play");
        return false;
    }
    if (!inited_)
    {
        begin();
    }
    if (!SPIFFS.exists(path))
    {
        LOG_WARNING("SPK", "local file missing: %s", path);
        return false;
    }
    LOG_INFO("SPK", "local: %s", path);
    return static_cast<Audio *>(impl_)->connecttoFS(SPIFFS, path);
}

void Speaker::Pause()
{
    if (inited_ && static_cast<Audio *>(impl_)->isRunning())
    {
        static_cast<Audio *>(impl_)->pauseResume();
    }
}

void Speaker::Resume()
{
    if (inited_ && !static_cast<Audio *>(impl_)->isRunning())
    {
        static_cast<Audio *>(impl_)->pauseResume();
    }
}

void Speaker::Stop()
{
    if (inited_)
    {
        static_cast<Audio *>(impl_)->stopSong();
    }
}

void Speaker::end()
{
    /*
     * C9 修复：释放 Audio 实例。Audio 析构内部会卸载 I2S 驱动。
     * 不在 begin() 时自动重建——下次需要播放时调用 begin() 重新分配。
     */
    if (!inited_)
    {
        return;
    }
    Audio *audio = static_cast<Audio *>(impl_);
    delete audio; /* Audio 析构处理 I2S uninstall */
    impl_ = nullptr;
    inited_ = false;
    LOG_INFO("SPK", "MAX98357 released");
}

}  // namespace ekeys
