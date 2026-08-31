/*
 * MainTask.h
 *
 * 阶段 02：在阶段 01 的 5ms tick 之上，每秒向 DisplayTask 队列投递
 * TIME_UPDATE（不依赖 NTP；阶段 06 之后切换为 NTP 时间）。
 */

#ifndef EKEYS_TASKS_MAIN_TASK_H
#define EKEYS_TASKS_MAIN_TASK_H

#include <stdint.h>

#include "input/MatrixScanner.h"
#include "keymap/KeyResolver.h"

namespace ekeys
{

    class IKeyboard;

    class MainTask
    {
    public:
        MainTask();

        void begin();
        void end();

        /*
         * 注入键盘后端。
         */
        void setKeyboard(IKeyboard *kb) { keyboard_ = kb; }

        /*
         * 注入 DisplayTask 队列。
         * AppContext::init() 中统一调用 DisplayTask::begin() 之后调用本接口。
         */
        void setDisplayQueue(void *queue_handle) { display_queue_ = queue_handle; }

        /*
         * active_keymap_profile 变更后重新加载键映射（阶段 04 任务 4.8）。
         */
        void reloadKeymap() { resolver_.begin(); }

        /*
         * 由 Arduino loop() 调用，约 5ms 一次。
         */
        void loop();

    private:
        void tick();

        MatrixScanner scanner_;
        KeyResolver resolver_;
        IKeyboard *keyboard_; // 不持有所有权
        void *display_queue_; // FreeRTOS QueueHandle_t（避免强引用）
        uint32_t last_tick_ms_;
        uint32_t last_time_post_ms_;
    };

} // namespace ekeys

#endif // EKEYS_TASKS_MAIN_TASK_H
