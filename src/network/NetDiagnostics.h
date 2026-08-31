/*
 * NetDiagnostics.h
 *
 * 网络诊断信息收集（阶段 06 任务 6.4）：
 * 把 WiFi / TCP 状态聚合到 HaStatusInfo（HA 屏 + 0x12 推送数据源）。
 */

#ifndef EKEYS_NETWORK_NET_DIAGNOSTICS_H
#define EKEYS_NETWORK_NET_DIAGNOSTICS_H

#include <stddef.h>

#include "message_types.h"

namespace ekeys {

class NetDiagnostics {
public:
    NetDiagnostics() = delete;

    /*
     * 只填充网络相关字段（wifi_* / tcp_connected / ip_address /
     * server_endpoint）；work_mode / voice / module 由调用方补齐。
     */
    static void fillNetworkFields(HaStatusInfo &out);
};

}  // namespace ekeys

#endif  // EKEYS_NETWORK_NET_DIAGNOSTICS_H
