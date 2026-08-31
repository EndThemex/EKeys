/*
 * cmd_device_info.cpp
 *
 * 报文（参考工程 onDeviceInfoGetCommand / SerialProtocol::sendDeviceInfo）：
 *   0x03 响应：{"cmd":0x83,"seq":N,"status":0,"device_info":{
 *     "device_name":"EKeys","device_id":"<efuse MAC 12 位十六进制>",
 *     "firmware_version":"0.6.0"}}
 */

#include "cmd_device_info.h"

#include <Arduino.h>
#include <ArduinoJson.h>

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

            JsonDocument doc;
            doc["cmd"] = CMD_DEVICE_INFO_GET | 0x80;
            doc["seq"] = seq;
            doc["status"] = 0;
            JsonObject info = doc["device_info"].to<JsonObject>();
            info["device_name"] = kDeviceName;
            info["device_id"] = device_id;
            info["firmware_version"] = kFirmwareVersion;
            SerialProtocol::instance().sendDocument(doc);
            return 0;
        }

    } // namespace

    void registerDeviceInfoHandlers()
    {
        CommandRegistry::instance().registerHandler(CMD_DEVICE_INFO_GET,
                                                    handleDeviceInfoGet);
        LOG_INFO("CMD", "cmd_device_info registered (0x%02X)",
                 CMD_DEVICE_INFO_GET);
    }

    void unregisterDeviceInfoHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_DEVICE_INFO_GET);
    }

} // namespace ekeys::protocol::commands
