/*
 * CommandRegistry.cpp
 *
 * 见 CommandRegistry.h。
 */

#include "CommandRegistry.h"

#include "logging/LogManager.h"

namespace ekeys {

CommandRegistry &CommandRegistry::instance()
{
    static CommandRegistry reg;
    return reg;
}

int CommandRegistry::findIndex_locked(int cmd) const
{
    for (size_t i = 0; i < table_.size(); ++i) {
        if (table_[i].cmd == cmd) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CommandRegistry::registerHandler(int cmd, CommandHandler handler)
{
    if (!handler) {
        LOG_WARNING("REG", "skip null handler for cmd=0x%02X", cmd);
        return;
    }

    taskENTER_CRITICAL(&mux_);
    int idx = findIndex_locked(cmd);
    if (idx >= 0) {
        table_[idx].handler = std::move(handler);  // 覆盖旧 handler
        taskEXIT_CRITICAL(&mux_);
        return;
    }
    for (size_t i = 0; i < table_.size(); ++i) {
        if (table_[i].cmd == -1) {
            table_[i].cmd = cmd;
            table_[i].handler = std::move(handler);
            taskEXIT_CRITICAL(&mux_);
            return;
        }
    }
    taskEXIT_CRITICAL(&mux_);
    LOG_ERROR("REG", "registry full, cmd=0x%02X rejected", cmd);
}

void CommandRegistry::unregisterHandler(int cmd)
{
    taskENTER_CRITICAL(&mux_);
    int idx = findIndex_locked(cmd);
    if (idx >= 0) {
        table_[idx].cmd = -1;
        table_[idx].handler = nullptr;
    }
    taskEXIT_CRITICAL(&mux_);
}

int CommandRegistry::dispatch(int cmd, int seq, JsonObject data)
{
    CommandHandler handler;
    taskENTER_CRITICAL(&mux_);
    int idx = findIndex_locked(cmd);
    if (idx >= 0) {
        handler = table_[idx].handler;
    }
    taskEXIT_CRITICAL(&mux_);

    if (!handler) {
        LOG_WARNING("REG", "no handler for cmd=0x%02X (seq=%d)", cmd, seq);
        return -1;
    }
    return handler(cmd, seq, data);
}

bool CommandRegistry::hasHandler(int cmd) const
{
    taskENTER_CRITICAL(&mux_);
    int idx = findIndex_locked(cmd);
    bool found = (idx >= 0 && static_cast<bool>(table_[idx].handler));
    taskEXIT_CRITICAL(&mux_);
    return found;
}

size_t CommandRegistry::handlerCount() const
{
    size_t count = 0;
    taskENTER_CRITICAL(&mux_);
    for (const auto &entry : table_) {
        if (entry.cmd != -1 && entry.handler) {
            ++count;
        }
    }
    taskEXIT_CRITICAL(&mux_);
    return count;
}

}  // namespace ekeys
