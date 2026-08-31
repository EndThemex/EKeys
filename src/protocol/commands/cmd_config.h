/*
 * cmd_config.h
 *
 * CMD_CONFIG_GET(0x07) / CMD_CONFIG_SET(0x08) handler 注册
 * （FEATURE_DOC §5.3/§6；阶段 04 任务 4.4）。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_CONFIG_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_CONFIG_H

namespace ekeys::protocol::commands {

void registerConfigHandlers();
void unregisterConfigHandlers();

}  // namespace ekeys::protocol::commands

#endif  // EKEYS_PROTOCOL_COMMANDS_CMD_CONFIG_H
