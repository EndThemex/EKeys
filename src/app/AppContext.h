/*
 * AppContext.h
 *
 * 全局单例（ARCHITECTURE §3.1）：持有 MainTask / DisplayTask / IKeyboard
 * / Configuration / KeymapRepository 等子系统指针。
 */

#ifndef EKEYS_APP_APP_CONTEXT_H
#define EKEYS_APP_APP_CONTEXT_H

#include <memory>

#include "output/IKeyboard.h"
#include "tasks/MainTask.h"

namespace ekeys
{

    class Configuration;
    class KeymapRepository;
    struct DeviceSettings;

    class AppContext
    {
    public:
        static AppContext &instance();

        void init();
        void shutdown();

        MainTask &mainTask() { return main_task_; }
        IKeyboard *keyboard() { return keyboard_.get(); }
        Configuration *configuration() { return configuration_; }
        KeymapRepository *keymapRepository() { return keymap_repo_.get(); }

        void setKeyboard(std::unique_ptr<IKeyboard> kb)
        {
            keyboard_ = std::move(kb);
        }

        /*
         * work_mode 变更时重建键盘实例（阶段 04 任务 4.8；
         * KeyboardFactory::recreate() 语义：create + setKeyboard）。
         */
        void applyWorkMode(uint8_t mode);

        /*
         * C6 修复：把"配置变更的副作用"统一到一处。
         * 根据 prev / curr 两份快照 diff 出 work_mode / active_keymap_profile
         * 变化，分别触发键盘重建 / 键映射重载，并统一调度 WiFi。
         * 调用方：cmd_config::handleConfigSet 与 MainTask::applyUiSettingsSnapshot。
         */
        void applyUiSideEffects(const DeviceSettings &prev, const DeviceSettings &curr);

    private:
        AppContext() = default;

        MainTask main_task_;
        std::unique_ptr<IKeyboard> keyboard_;
        Configuration *configuration_ = nullptr; // 指向单例，不持有所有权
        std::unique_ptr<KeymapRepository> keymap_repo_;
    };

} // namespace ekeys

#endif // EKEYS_APP_APP_CONTEXT_H
