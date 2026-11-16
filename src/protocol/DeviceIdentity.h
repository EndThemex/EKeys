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

    /*
     * 通信协议版本（MAJOR.MINOR）：
     *   MAJOR 不同 = 不兼容（命令/字段语义/帧格式发生破坏性变更）
     *   MINOR 不同 = 向后兼容（仅新增可选字段、命令或枚举值）
     * Web / App 端以 device_info.protocol_version 做 MAJOR 比对。
     */
    constexpr int kProtocolVersion = 1;
    constexpr int kProtocolMinor = 0;

    /* 配置结构版本（0x01 CMD_CONF_VERSION_GET，与协议版本独立） */
    constexpr int kConfigVersion = 1;

    /* 固件版本（0x0b / 0x03 响应共用） */
    constexpr const char *kFirmwareVersion = "0.6.0";

    /* 设备名（0x03 device_info） */
    constexpr const char *kDeviceName = "EKeys";

} // namespace ekeys::protocol

#endif // EKEYS_PROTOCOL_DEVICE_IDENTITY_H
