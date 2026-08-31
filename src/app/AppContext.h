/*
 * AppContext.h
 *
 * 全局单例（ARCHITECTURE §3.1）：持有 MainTask / DisplayTask / IKeyboard
 * 等子系统指针。阶段 01 仅持有几个必要的全局对象。
 */

#ifndef EKEYS_APP_APP_CONTEXT_H
#define EKEYS_APP_APP_CONTEXT_H

#include <memory>

#include "output/IKeyboard.h"
#include "tasks/MainTask.h"

namespace ekeys {

class AppContext {
public:
    static AppContext &instance();

    void init();
    void shutdown();

    MainTask       &mainTask()    { return main_task_; }
    IKeyboard      *keyboard()    { return keyboard_.get(); }
    void            setKeyboard(std::unique_ptr<IKeyboard> kb)
    {
        keyboard_ = std::move(kb);
    }

private:
    AppContext() = default;

    MainTask                  main_task_;
    std::unique_ptr<IKeyboard> keyboard_;
};

}  // namespace ekeys

#endif  // EKEYS_APP_APP_CONTEXT_H
