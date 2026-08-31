/*
 * cmd_firmware.cpp
 *
 * 报文（参考工程 onFirmwareInfoGetCommand / SerialProtocol::sendFirmwareInfo）：
 *   0x01 响应：{"cmd":0x81,"seq":N,"status":0,"data":{"version":1}}
 *   0x02 写配置版本：请求 {"cmd":2,"seq":N,"data":{"version":M}} →
 *        响应 0x82 status=0；version 写入 DeviceSettings.config_version
 *        并持久化到 config.ini [system] config_version
 *   0x0b 响应：{"cmd":0x8b,"seq":N,"status":0,"firmware":{...}}
 *   0x0b OTA 触发（阶段 07 7.3）：请求携带 data.url + data.checksum（MD5 hex）
 *        → 回 0x8b status=0 后执行 OTA；校验失败不覆盖固件，成功自动重启
 */

#include "cmd_firmware.h"

#include <ArduinoJson.h>
#include <string.h>

#include "../../config/Configuration.h"
#include "../../logging/LogManager.h"
#include "../../upgrade/Upgrade.h"
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

        int handleConfVersionSet(int cmd, int seq, JsonObject data)
        {
            if (data["version"].isNull())
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "missing 'version'");
                return -1;
            }
            const long version = data["version"] | -1L;
            if (version < 0 || version > 0xFFFFL)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "invalid 'version'");
                return -1;
            }

            /* 内存 + 持久化（mutate 加锁，saveSetting 内部自行加锁） */
            Configuration::instance().mutateSettings(
                [&](DeviceSettings &s)
                { s.config_version = static_cast<uint32_t>(version); });
            Configuration::instance().saveSetting("config_version",
                                                  static_cast<int>(version));

            JsonDocument doc;
            doc["cmd"] = CMD_CONF_VERSION_SET | 0x80;
            doc["seq"] = seq;
            doc["status"] = 0;
            JsonObject out = doc["data"].to<JsonObject>();
            out["version"] = static_cast<int>(version);
            SerialProtocol::instance().sendDocument(doc);
            LOG_INFO("CMD", "config_version set to %d", static_cast<int>(version));
            return 0;
        }

        int handleFirmwareInfo(int cmd, int seq, JsonObject data)
        {
            /*
             * 阶段 07 7.3：0x0b 双职责——
             *   data.url 非空 → OTA 触发（需 data.checksum = 固件 MD5）；
             *   否则 → 固件信息查询。
             */
            const char *url = data["url"] | "";
            if (url[0] != '\0')
            {
                const char *checksum = data["checksum"] | "";
                if (strlen(checksum) != 32)
                {
                    SerialProtocol::instance().sendErrorResponse(
                        cmd, seq, "missing 32-hex 'checksum' (md5)");
                    return -1;
                }

                Upgrade::instance().requestStart(url, checksum);
                SerialProtocol::instance().sendSuccessResponse(cmd, seq,
                                                               JsonObject());
                LOG_INFO("CMD", "ota requested, starting...");
                delay(100);                       // 等响应经 CDC flush
                Upgrade::instance().performOta(); // 阻塞；成功自动重启
                return 0;
            }

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
        CommandRegistry::instance().registerHandler(CMD_CONF_VERSION_SET,
                                                    handleConfVersionSet);
        CommandRegistry::instance().registerHandler(CMD_FIRMWARE_INFO,
                                                    handleFirmwareInfo);
        LOG_INFO("CMD", "cmd_firmware registered (0x%02X/0x%02X/0x%02X)",
                 CMD_CONF_VERSION_GET, CMD_CONF_VERSION_SET, CMD_FIRMWARE_INFO);
    }

    void unregisterFirmwareHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_CONF_VERSION_GET);
        CommandRegistry::instance().unregisterHandler(CMD_CONF_VERSION_SET);
        CommandRegistry::instance().unregisterHandler(CMD_FIRMWARE_INFO);
    }

} // namespace ekeys::protocol::commands
