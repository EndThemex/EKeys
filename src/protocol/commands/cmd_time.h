/*
 * cmd_time.h
 *
 * CMD_TIME_SET（0x13，App→主控）：写入系统时间。
 *   data: {"epoch": <seconds since 1970-01-01 UTC>, "tz": "CST-8"}
 *   响应: {"cmd":0x93,"seq":N,"status":0}
 *
 * 写入走 NtpSync::setEpoch()，立即生效并标记 synced；
 * MainTask::loop() 的 1s tick 会读出并刷新主屏时间/日期。
 */

#ifndef EKEYS_PROTOCOL_COMMANDS_CMD_TIME_H
#define EKEYS_PROTOCOL_COMMANDS_CMD_TIME_H

namespace ekeys::protocol::commands {

void registerTimeHandlers();
void unregisterTimeHandlers();

}  // namespace ekeys::protocol::commands

#endif  // EKEYS_PROTOCOL_COMMANDS_CMD_TIME_H
