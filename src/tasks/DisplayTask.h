/*
 * DisplayTask.h
 *
 * FreeRTOS 任务（Core 0）：
 *
 *   1. 周期 LVGL tick（LvglPort::tick(delta_ms)）；
 *   2. 阻塞读取 DisplayMessage 队列（MainTask → DisplayTask）；
 *   3. 把消息分发到对应 ui 组件（现阶段仅 ui_minimal）。
 *
 * 栈 8192，优先级 1（低于默认 5 即可避免抢占 USB ISR）。
 */

#ifndef EKEYS_TASKS_DISPLAY_TASK_H
#define EKEYS_TASKS_DISPLAY_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

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

        QueueHandle_t queue_;
    };

} // namespace ekeys

#endif // EKEYS_TASKS_DISPLAY_TASK_H
