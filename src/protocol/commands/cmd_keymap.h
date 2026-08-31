/*
 * cmd_keymap.h
 *
 * CMD_KEYMAP_GET（0x05，App→主控）：
 *   返回当前激活 Profile 的 11 键映射（data.keymap 数组）。
 * CMD_KEYMAP_SET（0x06，App→主控）：
 *   data.keymap[{physical, normal, macro, function}] 逐键写入
 *   keymap{N}.ini，成功后 reloadKeymap()。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_KEYMAP_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_KEYMAP_H

namespace ekeys::protocol::commands
{

    void registerKeymapHandlers();
    void unregisterKeymapHandlers();

} // namespace ekeys::protocol::commands

#endif // EKEYS_PROTOCOL_COMMANDS_CMD_KEYMAP_H
