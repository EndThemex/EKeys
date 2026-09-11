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

namespace {
/* 淡入时长：覆盖 MP3 首帧解码伪影 / 采样率切换瞬态（几十 ms 量级），
 * 又短到人耳不会察觉是"渐入"。 */
constexpr uint32_t kRampDurationMs = 150;
}  // namespace

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
    /* I2S 端口必须用 I2S_NUM_1：Audio 库默认 I2S_NUM_0，与 Mic.cpp 的
     * I2S0 冲突，会导致频谱 / ASR 的 i2s_driver_install 报
     * "register I2S object to platform failed"（每 tick 重试刷屏）。 */
    auto *audio = new Audio(false, 3, I2S_NUM_1);
    audio->setPinout(kPinI2sBclkSpeaker, kPinI2sLrclkSpeaker, kPinI2sDataSpeaker);
    audio->setVolume(12);  // 默认中等音量（0~21）
    target_volume_ = 12;
    ramping_ = false;  // end() 重建后不残留旧淡入状态
    impl_ = audio;
    inited_ = true;
    LOG_INFO("SPK", "MAX98357 ready (bclk=%u lrc=%u dout=%u)",
             kPinI2sBclkSpeaker, kPinI2sLrclkSpeaker, kPinI2sDataSpeaker);
}

void Speaker::loop()
{
    if (inited_)
    {
        tickVolumeRamp();
        static_cast<Audio *>(impl_)->loop();
    }
}

void Speaker::startVolumeRamp()
{
    Audio *audio = static_cast<Audio *>(impl_);
    audio->setVolume(0);
    ramp_start_ms_ = millis();
    ramping_ = true;
}

void Speaker::tickVolumeRamp()
{
    if (!ramping_)
    {
        return;
    }
    uint32_t elapsed = millis() - ramp_start_ms_;
    if (elapsed >= kRampDurationMs)
    {
        ramping_ = false;
        static_cast<Audio *>(impl_)->setVolume(target_volume_);
    }
    else
    {
        static_cast<Audio *>(impl_)->setVolume(
            static_cast<uint8_t>(target_volume_ * elapsed / kRampDurationMs));
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
    if (ramping_)
    {
        /* 淡入进行中：只更新目标值，淡入结束自动落到新目标，
         * 直接打断爬升会产生音量跳变。 */
        target_volume_ = volume_0_21;
        return;
    }
    target_volume_ = volume_0_21;
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
    /* 录音期间拒绝播放（资源保护：I2S 引脚已独立，但音频通路仍互斥） */
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
    bool ok = static_cast<Audio *>(impl_)->connecttohost(url);
    if (ok)
    {
        startVolumeRamp();  /* 压掉流启动瞬态（本地/网络同样适用） */
    }
    return ok;
}

bool Speaker::PlayLocalAudio(const char *path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }
    /* 录音期间拒绝播放（同 PlayRemoteAudio） */
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
    bool ok = static_cast<Audio *>(impl_)->connecttoFS(SPIFFS, path);
    if (ok)
    {
        startVolumeRamp();  /* 抑制 MP3 开头轻微电流音（首帧解码伪影/瞬态） */
    }
    return ok;
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
        ramping_ = false;  /* 停止即取消淡入，否则 SetVolume 会被悬空延迟 */
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

/*
 * ESP32-audioI2S 弱符号 audio_info（全局命名空间，见 Audio.h L72）的强定义：
 * 启用库内诊断输出——WAV 头解析 FormatCode/BitsPerSample/Audio-Length、
 * RIFF/WAVE 校验失败原因、"stream ready"/"End of file" 时机等。
 * 不定义时这些信息全部被吞，WAV/MP3 播放问题无从排查。
 * 调用上下文：MainTask（Speaker::loop）或 DisplayTask（AudioPad::trigger
 * → PlayLocalAudio → connecttoFS），与项目内其它跨任务 LOG 用法一致。
 */
void audio_info(const char *msg)
{
    LOG_INFO("SPK", "%s", msg);
}
