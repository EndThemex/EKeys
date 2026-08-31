/*
 * cmd_music.h
 *
 * CMD_MUSIC_STATUS（0x0e，App→主控）：
 *   data.music_status 更新音乐播放器状态并推送音乐屏。
 * CMD_MUSIC_CONTROL（0x0f，主控→App）：
 *   经 SerialProtocol::sendMusicControl() 主动发送（UI 按钮触发，联调接入）。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_MUSIC_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_MUSIC_H

namespace ekeys::protocol::commands
{

  void registerMusicHandlers();
  void unregisterMusicHandlers();

} // namespace ekeys::protocol::commands

#endif // EKEYS_PROTOCOL_COMMANDS_CMD_MUSIC_H
