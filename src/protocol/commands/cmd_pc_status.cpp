/*
 * cmd_pc_status.cpp
 *
 * 报文（参考工程 onPcStatusCommand）：
 *   请求：{"cmd":0x0d,"seq":N,"data":{"pc_status":{
 *     type?("config"), mask?, caps_lock?, num_lock?, scroll_lock?,
 *     network_connected?, on_ac_power?, battery_percent?,
 *     cpu_usage_percent?, memory_usage_percent?, cpu_temp_c?,
 *     disk_io_percent?, network_up_kbps?, network_down_kbps? }}}
 *   响应：{"cmd":0x8d,"seq":N,"status":0}
 */

#include "cmd_pc_status.h"

#include <ArduinoJson.h>
#include <string.h>

#include "../../logging/LogManager.h"
#include "../../message_types.h"
#include "../../SerialProtocol.h"
#include "../../tasks/DisplayTask.h"
#include "../CommandRegistry.h"
#include "../../config/Configuration.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        int handlePcStatus(int cmd, int seq, JsonObject data)
        {
            JsonObject pc = data["pc_status"].as<JsonObject>();
            if (pc.isNull())
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "missing 'pc_status'");
                return -1;
            }

            const char *type = pc["type"] | "update";

            if (strcmp(type, "config") == 0)
            {
                if (!pc["mask"].isNull())
                {
                    const uint32_t mask = pc["mask"].as<uint32_t>();
                    Configuration::instance().mutateSettings(
                        [&mask](DeviceSettings &d)
                        { d.pc_status_mask = mask; });
                    Configuration::instance().saveSetting(
                        "pc_status_mask", static_cast<int>(mask));
                }
                SerialProtocol::instance().sendSuccessResponse(cmd, seq,
                                                               JsonObject());
                return 0;
            }

            DisplayMessage msg;
            msg.type = DisplayMessageType::PcStatus;
            PcStatusInfo &p = msg.pc_status;
            p.caps_lock = pc["caps_lock"] | false;
            p.num_lock = pc["num_lock"] | false;
            p.scroll_lock = pc["scroll_lock"] | false;
            p.network_connected = pc["network_connected"] | false;
            p.cpu_usage_percent = pc["cpu_usage_percent"] | -1.0f;
            p.memory_usage_percent = pc["memory_usage_percent"] | -1.0f;
            p.cpu_temp_c = pc["cpu_temp_c"] | -1.0f;
            p.disk_io_percent = pc["disk_io_percent"] | -1.0f;
            p.network_up_kbps = pc["network_up_kbps"] | -1.0f;
            p.network_down_kbps = pc["network_down_kbps"] | -1.0f;

            DisplayTask::instance().post(msg, 0);
            SerialProtocol::instance().sendSuccessResponse(cmd, seq,
                                                           JsonObject());
            return 0;
        }

    } // namespace

    void registerPcStatusHandlers()
    {
        CommandRegistry::instance().registerHandler(
            CMD_PC_STATUS, handlePcStatus);
        LOG_INFO("CMD", "cmd_pc_status registered (0x%02X)", CMD_PC_STATUS);
    }

    void unregisterPcStatusHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_PC_STATUS);
    }

} // namespace ekeys::protocol::commands
