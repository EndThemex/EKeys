/*
 * cmd_pc_status.h
 *
 * CMD_PC_STATUS（0x0d，App→主控）：
 *   data.pc_status 更新 PC 状态并推送 PC 状态屏；
 *   type=config 时写 pc_status_mask。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_PC_STATUS_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_PC_STATUS_H

namespace ekeys::protocol::commands {

void registerPcStatusHandlers();
void unregisterPcStatusHandlers();

}  // namespace ekeys::protocol::commands

#endif  // EKEYS_PROTOCOL_COMMANDS_CMD_PC_STATUS_H
