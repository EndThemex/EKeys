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

    private:
        AppContext() = default;

        MainTask main_task_;
        std::unique_ptr<IKeyboard> keyboard_;
        Configuration *configuration_ = nullptr; // 指向单例，不持有所有权
        std::unique_ptr<KeymapRepository> keymap_repo_;
    };

} // namespace ekeys

#endif // EKEYS_APP_APP_CONTEXT_H
