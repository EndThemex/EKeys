/*
 * cmd_profile.h
 *
 * CMD_PROFILE_STATE（0x10，双向）：
 *   请求（GET）/ 连接推送 → profile 状态：
 *   - 顶层 profile_state：当前激活 profile（向后兼容保留）。
 *   - profiles 数组：全部 8 个 profile 的名称 + 图标元数据
 *     （profile / profile_number / profile_name / is_custom_name /
 *     has_custom_icon / icon_path?），不含键映射内容 —— App 连接后
 *     先拿列表，进入键盘设置页时再用 0x05（data.profile）按需取具体配置。
 * CMD_PROFILE_ICON_SET（0x11，App→主控）：
 *   data.profile_icon{profile, clear, png_base64} 写 / 删 SPIFFS 图标。
 * CMD_PROFILE_NAME_SET（0x15，App→主控）：
 *   data{profile?, name} 设置 / 清除（name=""）profile 名称（UTF-8 中文），
 *   持久化到 config.ini [profile]，设备 UI 直接显示该名称。
 *   处理顺序同 0x06：先内存更新并回 ACK，再落盘（SPIFFS 原子写 ~1s，
 *   放 ACK 前会拖慢响应导致 App 超时误判）。
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
