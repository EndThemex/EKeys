/*
 * AudioPad.h
 *
 * 音效板（Sound Pad）：11 个矩阵键各自的本地音频绑定与播放控制。
 *
 * - 绑定表：bindings_[key-1] = 文件名（无路径，如 "kick.mp3"），
 *   空串 = 未绑定。持久化到 SPIFFS /audio_pad.ini（SimpleIni，[pads] 节）。
 * - 文件位于 SPIFFS 根目录，名字白名单 ^[a-z0-9_]{1,20}\.(mp3|wav)$
 *   （见 cmd_audio.cpp），播放路径 = "/" + 文件名。
 * - 播放复用 Speaker::PlayLocalAudio()（ESP32-audioI2S connecttoFS，
 *   按扩展名选解码器）；录音互斥由 Speaker 内部守卫兜底（录音期间拒绝播放）。
 * - 音量全局复用 device_volume（applySetting → Speaker），本模块不管音量。
 *
 * 线程模型：
 *   - load()/setBinding()/persist()/协议 handler：MainTask 上下文
 *     （SerialProtocol::poll 在 MainTask::loop 中执行）。
 *   - trigger()/stop()：DisplayTask 上下文（矩阵键 ActionInput / 旋钮 ENTER）。
 *   - loop()：MainTask tick，检测播放结束清高亮。
 *   bindings_/playing_key_/playing_name_ 跨任务读写，portMUX 临界区保护；
 *   Speaker 调用（SPIFFS/解码，毫秒级）一律在临界区外。
 *
 * 扩展预留：后续"播放网络音频"复用 Speaker::PlayRemoteAudio(url)，
 * 在 cmd_audio 0x17 增加 op（或 play 携带 url 字段）即可，本模块结构不受影响。
 */

#ifndef EKEYS_AUDIO_AUDIO_PAD_H
#define EKEYS_AUDIO_AUDIO_PAD_H

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace ekeys
{

    class AudioPad
    {
    public:
        static constexpr uint8_t kPadCount = 11;
        /* 文件名上限：20 基名 + '.' + 3 扩展 = 24 字符（不含 '/'） */
        static constexpr size_t kNameLenMax = 24;
        /* 绑定表持久化路径（SPIFFS 根目录） */
        static constexpr const char *kPersistPath = "/audio_pad.ini";

        static AudioPad &instance();

        AudioPad(const AudioPad &) = delete;
        AudioPad &operator=(const AudioPad &) = delete;

        /* AppContext::init() 调用：读 /audio_pad.ini；文件缺失 = 全空表。
         * 已失效条目（文件不存在 / 名字非法）丢弃并记日志。 */
        void load();

        /* 绑定 key(1~11) → file；file = "" 清除。
         * 只改内存缓存 + 投递 DisplayMessage 刷 UI；落盘走 persist()。
         * file 非空时要求已在 SPIFFS（cmd_audio 侧已校验，此处兜底拒绝）。 */
        bool setBinding(uint8_t key, const char *file);

        /* 把当前缓存写回 /audio_pad.ini（全量重写）。失败仅返回 false，
         * 内存缓存保持新值（掉电窗口内最多丢一次绑定）。 */
        bool persist();

        /* 清除所有绑定到 file 的键；返回被清除的 key 数量。
         * 0x16 delete 文件后调用（内存），随后 persist()。 */
        uint8_t clearBindingsOf(const char *file);

        /* UI 按键触发（DisplayTask 上下文）：未绑定 no-op；已绑定则
         * 先停当前播放再播该键文件。成功返回 true（调用方亮键位高亮）。 */
        bool trigger(uint8_t key);

        /* App 试播指定文件（playing_key_ = 0，不亮键位高亮）。 */
        bool playFile(const char *file);

        /* 停止播放并清键位高亮（投递 AudioPad 消息）。 */
        void stop();

        /* MainTask tick：playing_key_ != 0 且 Speaker 已播完 → 清高亮。 */
        void loop();

        /* 拷出当前绑定表（out 按键 1~11 顺序，含 '\0' 结尾）。 */
        void snapshotBindings(char out[kPadCount][kNameLenMax + 1]) const;

        /* 该文件是否正在播放（0x16 delete 前置检查）。 */
        bool isPlayingFile(const char *file) const;

        /* 当前是否在播键位触发的音效（playing_key_ != 0）。 */
        bool isPlayingKey() const;

    private:
        AudioPad() = default;

        /* 内部：实际启动播放（临界区外调用，name 已拷贝） */
        bool startPlayback(const char *name, uint8_t key);
        /* 内部：投递 AudioPad DisplayMessage（当前缓存 + playing_key_） */
        void postPadMessage();
        /* 内部：拷出单个绑定（带锁） */
        void copyBinding(uint8_t key, char *out, size_t out_len) const;

        char bindings_[kPadCount][kNameLenMax + 1]{};
        uint8_t playing_key_ = 0; /* 1~11；0 = 无键位播放（试播/空闲） */
        char playing_name_[kNameLenMax + 1]{}; /* 当前播放文件（含试播） */
        mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    };

} // namespace ekeys

/* C 链接入口：ui_AudioScreen.c 在旋钮 ENTER 时调用（停止播放） */
extern "C" void ui_audio_pad_stop(void);

#endif // EKEYS_AUDIO_AUDIO_PAD_H
