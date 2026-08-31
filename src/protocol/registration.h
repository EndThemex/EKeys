/*
 * registration.h
 *
 * 协议命令统一注册入口（FEATURE_DOC §5.4；阶段 04 任务 4.6）。
 * 在 AppContext::init() 中 MainTask::begin() 之后调用。
 */

#ifndef EKEYS_PROTOCOL_REGISTRATION_H
#define EKEYS_PROTOCOL_REGISTRATION_H

namespace ekeys::protocol::registration {

void registerAllCommandHandlers();

}  // namespace ekeys::protocol::registration

#endif  // EKEYS_PROTOCOL_REGISTRATION_H
