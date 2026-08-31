/*
 * MainTask.h
 *
 * 阶段 05：
 *   - 5ms tick：MatrixScanner::scan() → KeyResolver → IKeyboard
 *   - EC11 旋钮 → DisplayMessageType::ActionInput（左 / 右 / 进入 / 返回）
 *   - 消费设置屏反向同步请求（ui_settings_request_apply/save）
 *     → 写回 DeviceSettings + 持久化 + 副作用（docs/05 §5.8）
 *   - 键映射加载后投递 KEYMAP_PROFILE_UPDATE（11 键标签）
 *   - 1s  tick：TIME_UPDATE（millis() 推算 HH:MM:SS；阶段 06 换 NTP）
 */

#ifndef EKEYS_TASKS_MAIN_TASK_H
#define EKEYS_TASKS_MAIN_TASK_H

#include <stdint.h>

#include "input/MatrixScanner.h"
#include "input/RotaryEncoder.h"
#include "keymap/KeyResolver.h"
#include "ui/ui_settings_types.h"

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
        void reloadKeymap();

        /*
         * 由 Arduino loop() 调用，约 5ms 一次。
         */
        void loop();

    private:
        void tick();

        /* 向 DisplayTask 投递（display_queue_ 为空时忽略） */
        void postMessage(const struct DisplayMessage &msg);

        /* 旋钮动作 → ActionInput */
        void sendDisplayAction(uint8_t action);

        /* 当前键映射 + Profile → KEYMAP_PROFILE_UPDATE（11 键标签） */
        void sendKeymapProfileUi();

        /* 设置屏反向同步（FEATURE_DOC §8.4） */
        void applyUiSettingsSnapshot(const ui_settings_snapshot_t &requested,
                                     bool persist);

        MatrixScanner scanner_;
        KeyResolver resolver_;
        RotaryEncoder encoder_;
        IKeyboard *keyboard_; // 不持有所有权
        void *display_queue_; // FreeRTOS QueueHandle_t（避免强引用）
        uint32_t last_tick_ms_;
        uint32_t last_time_post_ms_;
        bool keymap_ui_pending_{false};
    };

} // namespace ekeys

#endif // EKEYS_TASKS_MAIN_TASK_H
