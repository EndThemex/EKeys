/*
 * cmd_time.cpp
 *
 * 见 cmd_time.h。
 */

#include "cmd_time.h"

#include <ArduinoJson.h>

#include "../../logging/LogManager.h"
#include "../../network/NtpSync.h"
#include "../CommandRegistry.h"
#include "../SerialProtocol.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        int handleTimeSet(int cmd, int seq, JsonObject data)
        {
            if (data["epoch"].isNull())
            {
                SerialProtocol::instance().sendErrorResponse(
                    cmd, seq, "missing 'epoch'");
                return -1;
            }

            const int64_t epoch = data["epoch"].as<int64_t>();
            const char *tz = data["tz"] | "";

            NtpSync::instance().setEpoch(epoch, tz);

            SerialProtocol::instance().sendSuccessResponse(cmd, seq,
                                                           JsonObject());
            return 0;
        }

    } // namespace

    void registerTimeHandlers()
    {
        CommandRegistry::instance().registerHandler(CMD_TIME_SET,
                                                    handleTimeSet);
        LOG_INFO("CMD", "cmd_time registered (0x%02X)", CMD_TIME_SET);
    }

    void unregisterTimeHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_TIME_SET);
    }

} // namespace ekeys::protocol::commands
