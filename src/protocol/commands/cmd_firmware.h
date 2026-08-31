/*
 * cmd_firmware.h
 *
 * CMD_CONF_VERSION_GET（0x01，App→主控）：返回配置结构版本。
 * CMD_FIRMWARE_INFO（0x0b，App→主控）：返回固件版本 / 构建时间
 *   （0x02 CMD_CONF_VERSION_SET 留待阶段 07 任务 7.4）。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_FIRMWARE_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_FIRMWARE_H

namespace ekeys::protocol::commands
{

    void registerFirmwareHandlers();
    void unregisterFirmwareHandlers();

} // namespace ekeys::protocol::commands

#endif // EKEYS_PROTOCOL_COMMANDS_CMD_FIRMWARE_H
