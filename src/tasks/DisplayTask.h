/*
 * DisplayTask.h
 *
 * FreeRTOS 任务（Core 0）：
 *
 *   1. ui_init() 创建 11 屏（SquareLine 生成的 src/ui）；
 *   2. 周期 LVGL tick（LvglPort::tick(delta_ms)）；
 *   3. 阻塞读取 DisplayMessage 队列（MainTask → DisplayTask），
 *      分发到对应 ui 组件（状态条 / 各功能屏）。
 *
 * 栈 8192，优先级 1（低于默认 5 即可避免抢占 USB ISR）。
 */

#ifndef EKEYS_TASKS_DISPLAY_TASK_H
#define EKEYS_TASKS_DISPLAY_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "ui/ui_KeyMapped.h" // ui_screen_tag_t

namespace ekeys
{

    class DisplayTask
    {
    public:
        static DisplayTask &instance();

        /*
         * 在 setup() 中调用一次：创建队列 + 启动任务。
         */
        void begin();

        /*
         * 供 MainTask / ConfigStore 投递消息；不阻塞。
         * 返回 pdTRUE 表示成功加入队列，pdFALSE 表示队列满。
         */
        BaseType_t post(const struct DisplayMessage &msg, TickType_t wait_ticks = 0);

        /*
         * 屏幕路由（docs/05 §5.2）：请求切换到指定屏幕。
         * 内部转成 DisplayMessageType::Navigate 消息，本阶段为占位实现。
         */
        void navigateTo(ui_screen_tag_t tag);

        /*
         * 暴露底层 FreeRTOS 队列指针（opaque void*），便于 AppContext
         * 注入给 MainTask，避免 MainTask 强制包含 FreeRTOS 头。
         */
        void *queueHandle() const { return queue_; }

    private:
        DisplayTask();
        DisplayTask(const DisplayTask &) = delete;
        DisplayTask &operator=(const DisplayTask &) = delete;

        static void taskEntry(void *arg);
        void run();

        void applyMessage(const DisplayMessage &msg);
        void applySetting(const DisplayMessage &msg);
        void applyKeymapProfile(const DisplayMessage &msg);
        void applyPcStatus(const DisplayMessage &msg);
        void applyHaStatus(const DisplayMessage &msg);
        void applyMusicPlayer(const DisplayMessage &msg);
        void navigateNow(ui_screen_tag_t tag);

        /*
         * 频谱调度（阶段 07 任务 7.2）：
         * 音乐屏可见时接管 Mic → AudioAnalyzer → drawAudioBandsCool，
         * 离开时释放 Mic 并恢复语音识别。
         */
        void updateSpectrum();

        /*
         * 一级页 5s 无操作自动回主页：
         *   - isAutoReturnEnabled()  仅一级页（非 MAIN / 非 SECONDARY）启用
         *   - bumpActivity()         任意用户输入 / 主动导航都重置计时
         *   - checkAutoReturn()      run() 主循环每帧检查超时
         *
         * 二级页不启用：详情页往往需要较长时间查看，且 SETTING_SECONDARY 自身有
         * 1s apply debounce，让二级页独占用户注意。
         */
        bool isAutoReturnEnabled() const;
        bool isSecondaryScreen(ui_screen_tag_t tag) const;
        void bumpActivity();
        void checkAutoReturn();

        QueueHandle_t queue_;
        bool spectrum_active_ = false;
        /*
         * 上次用户活动时刻（FreeRTOS tick，单位 ms）。0 表示尚未开始计时，
         * 用于 run() 启动首帧 / navigateNow 到 MAIN 时禁用计时。
         */
        TickType_t last_activity_tick_ = 0;
    };

} // namespace ekeys

#endif // EKEYS_TASKS_DISPLAY_TASK_H
