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
 * 线程模型（2026-09-11 INT WDT 修复后）：
 *   - load()/setBinding()/persist()/trigger()/stop()/协议 handler/service()：
 *     仅 MainTask 上下文（SerialProtocol::poll 与 MainTask tick 同任务）。
 *   - DisplayTask（Core 0）只允许 requestTrigger()/requestStop() 置请求标志，
 *     由 MainTask::loop 的 service() 消费执行。ESP32-audioI2S 的 Audio 实例
 *     非线程安全，启停必须与 Speaker::loop 喂流同任务串行；此前 DisplayTask
 *     直接调 trigger/stop 与喂流跨核竞争，损坏 File/解码器状态导致
 *     CPU1 关中断自旋（Interrupt wdt timeout on CPU1）。
 *   - loop()：MainTask tick，检测播放结束清高亮。
 *   bindings_/playing_key_/playing_name_/请求标志跨任务读写，portMUX 临界区
 *   保护；Speaker/SPIFFS 调用（SPIFFS/解码，毫秒级）一律在临界区外。
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

        /* MainTask 上下文按键触发（cmd_audio 0x17）：未绑定 no-op；已绑定则
         * 先停当前播放再播该键文件。成功返回 true。
         * DisplayTask（Core 0）禁止调用本方法，改用 requestTrigger()。 */
        bool trigger(uint8_t key);

        /* DisplayTask（Core 0）请求按键触发：仅做绑定检查 + 置请求标志，
         * 实际启停由 MainTask::loop 的 service() 执行；绑定存在即返回 true
         * （调用方据此亮键位高亮，播放被拒时 service 重投消息纠正）。 */
        bool requestTrigger(uint8_t key);

        /* DisplayTask（Core 0）请求停止：清键位状态 + 置停止标志，
         * Speaker::Stop 由 service() 执行。 */
        void requestStop();

        /* MainTask::loop 调用：消费 requestTrigger()/requestStop() 请求。
         * 必须先于 Speaker::loop 调用，保证 Audio 实例启停与喂流同任务串行。 */
        void service();

        /* App 试播指定文件（playing_key_ = 0，不亮键位高亮）。 */
        bool playFile(const char *file);

        /* 停止播放并清键位高亮（投递 AudioPad 消息）。
         * 仅 MainTask 上下文（cmd_audio 0x17 stop）；Core 0 用 requestStop()。 */
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
        /* Core 0 → MainTask 播放/停止请求（lock_ 保护） */
        bool play_req_pending_ = false;
        uint8_t play_req_key_ = 0;
        bool stop_req_pending_ = false;
        mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    };

} // namespace ekeys

/* C 链接入口：ui_AudioScreenSecondary.c 在旋钮 ENTER 时调用（停止播放） */
extern "C" void ui_audio_pad_stop(void);

#endif // EKEYS_AUDIO_AUDIO_PAD_H
