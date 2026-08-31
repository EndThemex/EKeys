/*
 * registration.cpp
 *
 * 见 registration.h。阶段 04 仅注册 cmd_config；
 * 阶段 05/06 扩展 cmd_keymap / cmd_profile / cmd_pc_status / cmd_music 等。
 */

#include "registration.h"

#include "../logging/LogManager.h"
#include "CommandRegistry.h"
#include "commands/cmd_config.h"

namespace ekeys::protocol::registration {

void registerAllCommandHandlers()
{
    commands::registerConfigHandlers();
    LOG_INFO("REG", "all command handlers registered (%u)",
             static_cast<unsigned>(CommandRegistry::instance().handlerCount()));
}

}  // namespace ekeys::protocol::registration
