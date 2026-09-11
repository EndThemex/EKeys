/*
 * cmd_keymap.h
 *
 * CMD_KEYMAP_GET（0x05，App→主控）：
 *   返回 11 键映射（data.keymap 数组）。可选 data.profile（0~7）指定
 *   方案（缺省 = 当前激活）；响应含 profile 字段标识实际返回的方案。
 *   App 连接时先收 0x10 的名称+图标列表，进入键盘设置页再按方案取全量。
 * CMD_KEYMAP_SET（0x06，App→主控）：
 *   data.keymap[{physical, normal, macro, function}] 逐键写入
 *   当前激活 profile 的 keymap{N}.ini，成功后 applyKeymap 直刷运行时。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_KEYMAP_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_KEYMAP_H

namespace ekeys::protocol::commands
{

    void registerKeymapHandlers();
    void unregisterKeymapHandlers();

} // namespace ekeys::protocol::commands

#endif // EKEYS_PROTOCOL_COMMANDS_CMD_KEYMAP_H
