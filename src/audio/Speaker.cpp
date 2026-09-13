/*
 * Speaker.cpp
 *
 * 见 Speaker.h。
 */

#include "Speaker.h"

#include <Arduino.h>
#include <Audio.h>
#include <SPIFFS.h>
#include <driver/i2s.h>

#include "hardware/PinMap.h"
#include "logging/LogManager.h"
#include "voice/VoiceRecognizer.h"

namespace ekeys {

namespace {

/*
 * 空闲静音泵（消除冷启动开头"呲"声，2026-09-13）：
 *
 * 现象：上一段播放结束并空闲一段时间后再触发播放，开头必有一声"呲"；
 *       不等播放完毕就触发下一段（I2S DMA 链从未空闲）则无此声。
 *       此前的 150ms 音量淡入完全无效——库内 volumetable[0]=0，vol=0
 *       是真静音，说明爆音根本不在数字样本域，淡入方向从根上就错。
 *
 * 根因：库从不调用 i2s_stop，BCLK 常开；但播放结束后 TX DMA 描述符链
 *       （16×512 帧）耗尽进入 done/idle 态。下次播放的第一笔 i2s_write
 *       重启 DMA 链，BCLK/LRCLK 时序毛刺被 MAX98357 解析成垃圾帧 →
 *       模拟域爆音，任何数字音量处理都压不住。热启动（连续播放）时
 *       链从未空闲，无重启毛刺，所以干净。
 *
 * 对策：非播放期按实时节奏持续向 I2S 写零样本，让描述符链永不空闲，
 *       冷启动在 I2S 层面与热启动完全一致。每 tick 写 rate/180 帧
 *       （略超实时 ~11% 保证永不欠载），超出实时部分由 i2s_write 阻塞
 *       吸收（≤0.6ms/tick，与播放期阻塞同级）。副作用：暂停后恢复时
 *       内容需排空 DMA 内最多 ~186ms 的零样本，恢复略迟滞，可接受。
 */
constexpr uint32_t kSilencePumpDenom = 180;
constexpr size_t kSilencePumpMaxFrames = 288; /* 48kHz/180 ≈ 267 帧，留余量 */

int16_t s_silenceBuf[kSilencePumpMaxFrames * 2]; /* 立体声帧，静态零初始化 */

void pumpSilence(i2s_port_t port, uint32_t sample_rate)
{
    if (sample_rate == 0)
    {
        sample_rate = 16000; /* Audio 库 driver_install 的默认采样率 */
    }
    size_t frames = sample_rate / kSilencePumpDenom;
    if (frames < 1)
    {
        frames = 1;
    }
    else if (frames > kSilencePumpMaxFrames)
    {
        frames = kSilencePumpMaxFrames;
    }
    size_t written = 0;
    /* 20ms 超时防异常长阻塞；正常时 DMA 有余量立即入队返回 */
    i2s_write(port, s_silenceBuf, frames * 2 * sizeof(int16_t),
              &written, pdMS_TO_TICKS(20));
}

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
    impl_ = audio;
    inited_ = true;
    LOG_INFO("SPK", "MAX98357 ready (bclk=%u lrc=%u dout=%u)",
             kPinI2sBclkSpeaker, kPinI2sLrclkSpeaker, kPinI2sDataSpeaker);
}

void Speaker::loop()
{
    if (!inited_)
    {
        return;
    }
    Audio *audio = static_cast<Audio *>(impl_);
    if (audio->isRunning())
    {
        audio->loop();
    }
    else
    {
        /* 空闲静音泵：保持 TX DMA 链活跃（见文件头静音泵说明）。
         * 采样率跟随库内保留值（上次播放的速率 / 首播前 16000）。 */
        pumpSilence(I2S_NUM_1, audio->getSampleRate());
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
    /* 0~100 四舍五入映射到库的 0~21（22 档）。
     * 旧 /5 映射 100% 只到 20 档 = 58/64 ≈ -0.9dB，永远到不了满幅。 */
    SetVolume((device_volume * 21 + 50) / 100);
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
    return static_cast<Audio *>(impl_)->connecttohost(url);
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
