/*
 * MainTask.h
 *
 * 阶段 05：
 *   - 5ms tick：MatrixScanner::scan() → KeyResolver → IKeyboard
 *   - EC11 旋钮 → DisplayMessageType::ActionInput（左 / 右 / 进入 / 返回）
 *   - 消费设置屏反向同步请求（ui_settings_request_apply/save）
 *     → 写回 DeviceSettings + 持久化 + 副作用（docs/05 §5.8）
 *   - 键映射加载后投递 KEYMAP_PROFILE_UPDATE（11 键标签）
 *   - 1s  tick：TIME_UPDATE（NTP 优先，未同步用 millis() 推算）
 *
 * 阶段 06：tick() 周期调度 WiFi / NTP / 发现 / TCP / 扬声器 / ASR。
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
         * 0x06 下发路径：用协议层解析好的内存映射直接刷新 KeyResolver
         * （免 SPIFFS 重读），并投递键映射屏标签更新。持久化由协议层
         * 在回 ACK 后自行完成；启动路径仍走 reloadKeymap()。
         */
        void applyKeymap(const std::array<KeyMapping, kMatrixKeyCount + 1> &map,
                         uint16_t keyMask);

        /*
         * 由 Arduino loop() 调用，约 5ms 一次。
         */
        void loop();

    private:
        /* 阶段 06 服务调度（WiFi/NTP/发现/TCP/扬声器/ASR + HA 状态节流） */
        void tick();

        /* 向 DisplayTask 投递（display_queue_ 为空时忽略） */
        void postMessage(const struct DisplayMessage &msg);

        /* 旋钮动作 → ActionInput */
        void sendDisplayAction(uint8_t action);

        /*
         * 当前键映射 + Profile → KEYMAP_PROFILE_UPDATE（11 键标签）。
         * fun_layer：0=单击视图（含组合摘要后缀），1/2=FUN 按住时只显示
         * 对应组合层摘要（键映射二级页 FUN 预览）。
         */
        void sendKeymapProfileUi(uint8_t fun_layer);

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
        uint32_t last_ha_status_ms_{0};
        uint32_t last_battery_status_ms_{0};
        bool keymap_ui_pending_{false};
        uint8_t fun_ui_layer_{0}; /* 上次推送给 UI 的 FUN 组合层（0/1/2） */
        /* ui_screen_tag_t 缓存（UI_SCREEN_UNKNOWN=0），仅 5ms tick 内读写，
         * 用于检测"进入键映射屏"边沿以补推当前 FUN 层标签 */
        uint8_t keymap_ui_screen_{0};
    };

} // namespace ekeys

#endif // EKEYS_TASKS_MAIN_TASK_H
