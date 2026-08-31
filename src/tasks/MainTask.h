/*
 * MainTask.h
 *
 * 阶段 01：单线程（不创建 FreeRTOS 任务），5ms 周期调用 loop()。
 * 阶段 02 后改为 Core 1 FreeRTOS 任务。
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
         * 注入键盘后端；阶段 02 后改为 AppContext 持有。
         */
        void setKeyboard(IKeyboard *kb) { keyboard_ = kb; }

        /*
         * 由 Arduino loop() 调用，约 5ms 一次。
         */
        void loop();

    private:
        void tick();

        MatrixScanner scanner_;
        KeyResolver resolver_;
        IKeyboard *keyboard_; // 不持有所有权
        uint32_t last_tick_ms_;
    };

} // namespace ekeys

#endif // EKEYS_TASKS_MAIN_TASK_H
