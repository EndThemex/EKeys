/*
 * cmd_pc_status.h
 *
 * CMD_PC_STATUS（0x0d，App→主控）：
 *   data.pc_status 更新 PC 状态并推送 PC 状态屏；
 *   type=config 时写 pc_status_mask。
 *
 *   不支持 on_ac_power / battery_percent：电量走 BatteryStatus，
 *   电源信息本协议未实现，App 推送的字段会被静默丢弃。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_PC_STATUS_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_PC_STATUS_H

namespace ekeys::protocol::commands {

void registerPcStatusHandlers();
void unregisterPcStatusHandlers();

}  // namespace ekeys::protocol::commands

#endif  // EKEYS_PROTOCOL_COMMANDS_CMD_PC_STATUS_H
