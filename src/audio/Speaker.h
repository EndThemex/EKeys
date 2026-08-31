/*
 * Speaker.h
 *
 * MAX98357A I2S 功放封装（FEATURE_DOC §10.1，阶段 06 任务 6.7）。
 *
 *   - 引脚（PINOUT §2.7）：BCLK=IO10 / LRCLK=IO9 / DOUT=IO14
 *   - 音量 SetVolume(0~21)，device_volume/5 由调用方换算
 *   - PlayRemoteAudio(url) / PlayLocalAudio(path) / Pause / Resume / Stop
 *   - loop() 由 MainTask 周期喂（解码驱动）
 *
 * 底层库：ESP32-audioI2S@3.0.11（与 arduino-esp32 2.0.11 / IDF4.4 兼容）。
 */

#ifndef EKEYS_AUDIO_SPEAKER_H
#define EKEYS_AUDIO_SPEAKER_H

#include <stdint.h>

namespace ekeys {

class Speaker {
public:
    static Speaker &instance();

    Speaker(const Speaker &) = delete;
    Speaker &operator=(const Speaker &) = delete;

    /* 首次调用时 setPinout + 音量初始化；重复调用无害 */
    void begin();

    /* MainTask 周期调用（解码器喂流） */
    void loop();

    bool isRunning() const;

    /* device_volume(0~100)/5 → SetVolume(0~21) */
    void applyDeviceVolume(uint8_t device_volume);

    void SetVolume(uint8_t volume_0_21);

    bool PlayRemoteAudio(const char *url);
    bool PlayLocalAudio(const char *path);

    void Pause();
    void Resume();
    void Stop();

private:
    Speaker() = default;

    void *impl_ = nullptr;  // Audio*（避免头文件引 ESP32-audioI2S）
    bool inited_ = false;
};

}  // namespace ekeys

#endif  // EKEYS_AUDIO_SPEAKER_H
