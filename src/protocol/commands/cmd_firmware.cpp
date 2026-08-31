/*
 * cmd_firmware.cpp
 *
 * 报文（参考工程 onFirmwareInfoGetCommand / SerialProtocol::sendFirmwareInfo）：
 *   0x01 响应：{"cmd":0x81,"seq":N,"status":0,"data":{"version":1}}
 *   0x0b 响应：{"cmd":0x8b,"seq":N,"status":0,"firmware":{
 *     "version":"0.6.0","device":"EKeys",
 *     "build_date":"...","build_time":"..."}}
 */

#include "cmd_firmware.h"

#include <ArduinoJson.h>

#include "../../logging/LogManager.h"
#include "../CommandRegistry.h"
#include "../DeviceIdentity.h"
#include "../SerialProtocol.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        int handleConfVersionGet(int cmd, int seq, JsonObject /*data*/)
        {
            (void)cmd;
            JsonDocument doc;
            doc["cmd"] = CMD_CONF_VERSION_GET | 0x80;
            doc["seq"] = seq;
            doc["status"] = 0;
            JsonObject out = doc["data"].to<JsonObject>();
            out["version"] = kConfigVersion;
            SerialProtocol::instance().sendDocument(doc);
            return 0;
        }

        int handleFirmwareInfo(int cmd, int seq, JsonObject /*data*/)
        {
            (void)cmd;
            JsonDocument doc;
            doc["cmd"] = CMD_FIRMWARE_INFO | 0x80;
            doc["seq"] = seq;
            doc["status"] = 0;
            JsonObject fw = doc["firmware"].to<JsonObject>();
            fw["version"] = kFirmwareVersion;
            fw["device"] = kDeviceName;
            fw["build_date"] = __DATE__;
            fw["build_time"] = __TIME__;
            SerialProtocol::instance().sendDocument(doc);
            return 0;
        }

    } // namespace

    void registerFirmwareHandlers()
    {
        CommandRegistry::instance().registerHandler(CMD_CONF_VERSION_GET,
                                                    handleConfVersionGet);
        CommandRegistry::instance().registerHandler(CMD_FIRMWARE_INFO,
                                                    handleFirmwareInfo);
        LOG_INFO("CMD", "cmd_firmware registered (0x%02X/0x%02X)",
                 CMD_CONF_VERSION_GET, CMD_FIRMWARE_INFO);
    }

    void unregisterFirmwareHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_CONF_VERSION_GET);
        CommandRegistry::instance().unregisterHandler(CMD_FIRMWARE_INFO);
    }

} // namespace ekeys::protocol::commands
