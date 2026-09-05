/*
 * VoiceRecognizer.h
 *
 * 语音识别状态机（FEATURE_DOC §11，阶段 06 任务 6.10/6.12/6.13）。
 *
 * 触发流程（§11.2）：
 *   1. voice_trigger_key 按下 → startCapture()（Mic + PSRAM PCM 缓冲）
 *   2. MainTask 周期 feedCapture() 持续攒 PCM
 *   3. 触发键松开 → finishCapture() 仅截断 PCM 并投递给后台 ASR Task
 *   4. 后台 Task 调百度 ASR REST → 结果经 CMD_VOICE_TEXT(0x0c) 推送 App；
 *      TCP 未连接且文本为 ASCII 时兜底 HID 注入（voice_auto_enter 追加回车）
 *
 * 仅在 WIRED_KEYBOARD_MODE + WiFi 已连 + voice_enable=1 时工作；
 * 其余模式 startCapture() 直接忽略（§11.3）。
 *
 * C3 修复：HTTP 同步阻塞搬离 MainTask 上下文，录音结束立即返回；
 *   后台任务执行识别 + 上报，MainTask tick 不再被 10s+ HTTP 阻塞。
 */

#ifndef EKEYS_VOICE_VOICE_RECOGNIZER_H
#define EKEYS_VOICE_VOICE_RECOGNIZER_H

#include <stddef.h>
#include <stdint.h>

#include "ui/ui_settings_types.h"

namespace ekeys {

class VoiceRecognizer {
public:
    static VoiceRecognizer &instance();

    VoiceRecognizer(const VoiceRecognizer &) = delete;
    VoiceRecognizer &operator=(const VoiceRecognizer &) = delete;

    /* 录音开始；条件不满足返回 false（调用方不打断 HID 行为） */
    bool startCapture();

    /* MainTask 周期调用：攒 PCM */
    void feedCapture();

    /*
     * 录音结束：仅做截断 + 投递 ASR 任务，HTTP 在后台线程执行。
     * 若录音过长缓冲满（feedCapture 自动调用本函数），同样立即返回。
     */
    void finishCapture();

    bool isCapturing() const { return capturing_; }
    bool isSuspended() const { return suspended_; }

    /* 音乐屏进入 suspend / 离开 resume（docs/06 备注） */
    void suspend() { suspended_ = true; }
    void resume() { suspended_ = false; }

    /*
     * 录音 / 频谱前停掉 Speaker（I2S BCLK=IO10 共用互斥）。
     * 公开为静态以便 DisplayTask 进入频谱前也能调用。
     */
    static void prepareI2sForMicCapture();

private:
    VoiceRecognizer() = default;

    /* work_mode / wifi / voice_enable / suspend 综合判断 */
    bool canWork() const;

    void postRecordingState(bool recording);

    /* 后台 ASR 任务入口与生命周期管理 */
    static void asrTaskEntry(void *arg);
    void asrTaskLoop();
    bool ensureAsrTask();

    /* ASR 后台任务的输入单元 */
    struct AsrJob
    {
        int16_t *pcm = nullptr;     // PSRAM，由后台任务 free
        size_t samples = 0;
        uint32_t duration_ms = 0;
        bool auto_enter = false;
        char cuid[32] = {0};
        uint16_t dev_pid = 0;
    };

    /* 队列槽位（单元素，避免 heap 抖动；新录音需等前一段识别完成） */
    static constexpr uint8_t kAsrQueueDepth = 1;

    bool capturing_ = false;
    bool suspended_ = false;
    uint32_t capture_start_ms_ = 0;
    int16_t *pcm_buf_ = nullptr;     // PSRAM
    size_t pcm_cap_samples_ = 0;
    size_t pcm_len_samples_ = 0;
    uint8_t captured_work_mode_ = 0;

    /* C3：后台 ASR 任务相关 */
    void *asr_task_handle_ = nullptr; // TaskHandle_t 转发避免公开 freertos 头
    volatile bool asr_job_pending_ = false;
    AsrJob asr_queue_[kAsrQueueDepth];
    uint8_t asr_queue_head_ = 0;
    uint8_t asr_queue_tail_ = 0;
};

}  // namespace ekeys

#endif  // EKEYS_VOICE_VOICE_RECOGNIZER_H
