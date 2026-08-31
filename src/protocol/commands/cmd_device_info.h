/*
 * cmd_device_info.h
 *
 * CMD_DEVICE_INFO_GET（0x03，App→主控）：返回设备名 / 设备 ID / 固件版本
 *   （0x04 CMD_DEVICE_INFO_SET 留待阶段 07 任务 7.5）。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_DEVICE_INFO_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_DEVICE_INFO_H

namespace ekeys::protocol::commands
{

    void registerDeviceInfoHandlers();
    void unregisterDeviceInfoHandlers();

} // namespace ekeys::protocol::commands

#endif // EKEYS_PROTOCOL_COMMANDS_CMD_DEVICE_INFO_H
