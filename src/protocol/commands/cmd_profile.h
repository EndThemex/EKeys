/*
 * cmd_profile.h
 *
 * CMD_PROFILE_STATE（0x10，双向）：
 *   请求（GET）→ 主动推送当前 profile 状态（sendProfileState）。
 * CMD_PROFILE_ICON_SET（0x11，App→主控）：
 *   data.profile_icon{profile, clear, png_base64} 写 / 删 SPIFFS 图标。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_PROFILE_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_PROFILE_H

namespace ekeys::protocol::commands {

void registerProfileHandlers();
void unregisterProfileHandlers();

/* 主动推送：{"cmd":0x10,"seq":N,"profile_state":{...}}（seq=0 为推送） */
void sendProfileState(int seq);

}  // namespace ekeys::protocol::commands

#endif  // EKEYS_PROTOCOL_COMMANDS_CMD_PROFILE_H
