/*
 * cmd_pc_status.cpp
 *
 * 报文（参考工程 onPcStatusCommand）：
 *   请求：{"cmd":0x0d,"seq":N,"data":{"pc_status":{
 *     type?("config"), mask?, network_connected?,
 *     cpu_usage_percent?, memory_usage_percent?, cpu_temp_c?,
 *     disk_io_percent?, network_up_kbps?, network_down_kbps? }}}
 *   响应：{"cmd":0x8d,"seq":N,"status":0}
 *
 *   不支持的字段（推送会被静默丢弃）：
 *     caps_lock / num_lock / scroll_lock —— 二级屏布局未单独渲染，
 *       早期实现解析后未消费，2026-09 删除解析以保持实现与契约一致。
 *     on_ac_power / battery_percent —— 电源信息本协议未实现；
 *       电量走 CMD_BATTERY_STATUS。
 */

#include "cmd_pc_status.h"

#include <ArduinoJson.h>
#include <string.h>

#include "../../logging/LogManager.h"
#include "../../message_types.h"
#include "../SerialProtocol.h"
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
                    /* F6 修复：用 uint32 重载，高位不再被截断 */
                    Configuration::instance().saveSetting(
                        "pc_status_mask", mask);
                }
                SerialProtocol::instance().sendSuccessResponse(cmd, seq,
                                                               JsonObject());
                return 0;
            }

            DisplayMessage msg;
            msg.type = DisplayMessageType::PcStatus;
            PcStatusInfo &p = msg.pc_status;
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
