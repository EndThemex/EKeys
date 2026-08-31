/*
 * CommandRegistry.h
 *
 * 命令注册表（FEATURE_DOC §5.4、ARCHITECTURE §3.6；阶段 04 任务 4.2）。
 *
 * - std::array<Entry, 64> 零堆分配 handler 表
 * - portMUX 临界区保护注册 / 分发（参考 FunModularKeyboard 实现）
 * - 未注册命令 dispatch 返回 -1 并 LOG_WARNING，不崩溃
 *
 * handler 签名：int(int cmd, int seq, JsonObject data)
 *   返回 0=成功，非 0=失败。
 */

#ifndef EKEYS_PROTOCOL_COMMAND_REGISTRY_H
#define EKEYS_PROTOCOL_COMMAND_REGISTRY_H

#include <ArduinoJson.h>
#include <array>
#include <functional>
#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace ekeys {

using CommandHandler = std::function<int(int cmd, int seq, JsonObject data)>;

class CommandRegistry {
public:
    static constexpr size_t kMaxCommands = 64;

    static CommandRegistry &instance();

    CommandRegistry(const CommandRegistry &) = delete;
    CommandRegistry &operator=(const CommandRegistry &) = delete;

    /* 注册 handler；重复注册同一 cmd 覆盖旧值；表满时拒绝并 LOG_ERROR */
    void registerHandler(int cmd, CommandHandler handler);

    /* 移除 handler */
    void unregisterHandler(int cmd);

    /* 分发命令；无 handler 返回 -1（同时 LOG_WARNING） */
    int dispatch(int cmd, int seq, JsonObject data);

    bool hasHandler(int cmd) const;
    size_t handlerCount() const;

private:
    CommandRegistry() = default;
    ~CommandRegistry() = default;

    struct Entry {
        int cmd{-1};
        CommandHandler handler;
    };

    int findIndex_locked(int cmd) const;

    std::array<Entry, kMaxCommands> table_;
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

}  // namespace ekeys

#endif  // EKEYS_PROTOCOL_COMMAND_REGISTRY_H
