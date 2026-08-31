/*
 * cmd_device_info.cpp
 *
 * 报文（参考工程 onDeviceInfoGetCommand / SerialProtocol::sendDeviceInfo）：
 *   0x03 响应：{"cmd":0x83,"seq":N,"status":0,"device_info":{
 *     "device_name":"EKeys","device_id":"<efuse MAC 12 位十六进制>",
 *     "firmware_version":"0.6.0"}}
 *   0x04 写设备信息（阶段 07 7.5）：请求
 *     {"cmd":4,"seq":N,"data":{"device_name":"...","serial":"..."}}
 *     → 写入 DeviceSettings.device_name / serial_number 并持久化；
 *     0x03 之后优先返回持久化的 device_name（空则回默认 EKeys）。
 */

#include "cmd_device_info.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../../config/Configuration.h"
#include "../../logging/LogManager.h"
#include "../CommandRegistry.h"
#include "../DeviceIdentity.h"
#include "../SerialProtocol.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        /* efuse MAC → 12 位大写十六进制设备 ID */
        void fillDeviceId(char *out, size_t cap)
        {
            uint64_t mac = ESP.getEfuseMac();
            snprintf(out, cap, "%04X%08X",
                     static_cast<unsigned>((mac >> 32) & 0xFFFF),
                     static_cast<unsigned>(mac & 0xFFFFFFFF));
        }

        int handleDeviceInfoGet(int cmd, int seq, JsonObject /*data*/)
        {
            (void)cmd;
            char device_id[13];
            fillDeviceId(device_id, sizeof(device_id));

            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            const char *name = (snap.device_name[0] != '\0')
                                   ? snap.device_name
                                   : kDeviceName;

            JsonDocument doc;
            doc["cmd"] = CMD_DEVICE_INFO_GET | 0x80;
            doc["seq"] = seq;
            doc["status"] = 0;
            JsonObject info = doc["device_info"].to<JsonObject>();
            info["device_name"] = name;
            info["device_id"] = device_id;
            info["firmware_version"] = kFirmwareVersion;
            SerialProtocol::instance().sendDocument(doc);
            return 0;
        }

        int handleDeviceInfoSet(int cmd, int seq, JsonObject data)
        {
            const char *name = data["device_name"] | "";
            const char *serial = data["serial"] | "";
            if (name[0] == '\0' && serial[0] == '\0')
            {
                SerialProtocol::instance().sendErrorResponse(
                    cmd, seq, "need 'device_name' and/or 'serial'");
                return -1;
            }
            if (strlen(name) >= sizeof(DeviceSettings{}.device_name) ||
                strlen(serial) >= sizeof(DeviceSettings{}.serial_number))
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "field too long (<=32)");
                return -1;
            }

            /* 内存 + 持久化（mutate 加锁，saveSetting 内部自行加锁） */
            Configuration::instance().mutateSettings(
                [&](DeviceSettings &s)
                {
                    if (name[0] != '\0')
                    {
                        snprintf(s.device_name, sizeof(s.device_name), "%s", name);
                    }
                    if (serial[0] != '\0')
                    {
                        snprintf(s.serial_number, sizeof(s.serial_number), "%s",
                                 serial);
                    }
                });
            if (name[0] != '\0')
            {
                Configuration::instance().saveSetting("device_name", name);
            }
            if (serial[0] != '\0')
            {
                Configuration::instance().saveSetting("serial_number", serial);
            }

            DeviceSettings snap;
            Configuration::instance().snapshot(snap);

            JsonDocument doc;
            doc["cmd"] = CMD_DEVICE_INFO_SET | 0x80;
            doc["seq"] = seq;
            doc["status"] = 0;
            JsonObject info = doc["device_info"].to<JsonObject>();
            info["device_name"] = snap.device_name;
            info["serial"] = snap.serial_number;
            SerialProtocol::instance().sendDocument(doc);
            LOG_INFO("CMD", "device info set (name=%s serial=%s)",
                     snap.device_name, snap.serial_number);
            return 0;
        }

    } // namespace

    void registerDeviceInfoHandlers()
    {
        CommandRegistry::instance().registerHandler(CMD_DEVICE_INFO_GET,
                                                    handleDeviceInfoGet);
        CommandRegistry::instance().registerHandler(CMD_DEVICE_INFO_SET,
                                                    handleDeviceInfoSet);
        LOG_INFO("CMD", "cmd_device_info registered (0x%02X/0x%02X)",
                 CMD_DEVICE_INFO_GET, CMD_DEVICE_INFO_SET);
    }

    void unregisterDeviceInfoHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_DEVICE_INFO_GET);
        CommandRegistry::instance().unregisterHandler(CMD_DEVICE_INFO_SET);
    }

} // namespace ekeys::protocol::commands
