/*
 * Mic.h
 *
 * ICS43434 数字 MEMS 麦克风封装（FEATURE_DOC §10.2，阶段 06 任务 6.8）。
 *
 *   - 引脚（PINOUT §2.7）：SCK=IO13 / WS=IO12 / SDOUT→ESP32=IO11
 *     （I2S0 RX 主模式；时钟走专用 MIC_SCK=IO13，避开功放 BCLK=IO10）
 *   - 16kHz / 16bit / mono，内部 512 samples 缓冲
 *   - Read() 阻塞读（VoiceRecognizer 录音期间在 MainTask 上下文调用）
 */

#ifndef EKEYS_AUDIO_MIC_H
#define EKEYS_AUDIO_MIC_H

#include <stddef.h>
#include <stdint.h>

namespace ekeys {

class Mic {
public:
    static Mic &instance();

    Mic(const Mic &) = delete;
    Mic &operator=(const Mic &) = delete;

    /* 安装 I2S0 RX 驱动；已在录音中时返回 true */
    bool begin();

    /* 卸载驱动（录音结束 / suspend） */
    void end();

    bool inited() const { return inited_; }

    /*
     * F4 修复：录音与频谱共享 I2S0 / Mic 硬件，靠时序不可靠。
     * 暴露 take/give 以便 VoiceRecognizer / DisplayTask 显式互斥。
     * begin() 内部已 take；end() 内部已 give。
     */
    bool take();
    void give();

    /*
     * 阻塞读取 PCM 样本（int16 单声道）。
     * 返回实际读取样本数；驱动未初始化返回 0。
     */
    size_t Read(int16_t *out, size_t max_samples);

    static constexpr uint32_t kSampleRate = 16000;
    static constexpr size_t kDmaSamples = 512;

private:
    Mic() = default;

    bool inited_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_AUDIO_MIC_H
