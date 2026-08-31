/*
 * DeviceIdentity.h
 *
 * 设备标识 / 固件版本常量（cmd_firmware / cmd_device_info 共用）。
 * 升级版本号只需改这里。
 */

#ifndef EKEYS_PROTOCOL_DEVICE_IDENTITY_H
#define EKEYS_PROTOCOL_DEVICE_IDENTITY_H

namespace ekeys::protocol
{

    /* 配置结构版本（0x01 CMD_CONF_VERSION_GET） */
    constexpr int kConfigVersion = 1;

    /* 固件版本（0x0b / 0x03 响应共用） */
    constexpr const char *kFirmwareVersion = "0.6.0";

    /* 设备名（0x03 device_info） */
    constexpr const char *kDeviceName = "EKeys";

} // namespace ekeys::protocol

#endif // EKEYS_PROTOCOL_DEVICE_IDENTITY_H
