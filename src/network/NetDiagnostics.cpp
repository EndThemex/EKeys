/*
 * NetDiagnostics.cpp
 *
 * 见 NetDiagnostics.h。
 */

#include "NetDiagnostics.h"

#include <stdio.h>

#include <WiFi.h>

#include "network/TcpChannel.h"
#include "network/WiFiManager.h"

namespace ekeys
{

    void NetDiagnostics::fillNetworkFields(HaStatusInfo &out)
    {
        const WiFiManager &wifi = WiFiManager::instance();

        out.wifi_enabled = wifi.isEnabled();
        out.wifi_connected = wifi.isConnected();
        out.wifi_rssi = wifi.rssi();
        out.tcp_connected = TcpChannel::instance().isConnected();

        if (out.wifi_connected)
        {
            snprintf(out.ip_address, sizeof(out.ip_address), "%s",
                     WiFi.localIP().toString().c_str());
        }
        else
        {
            out.ip_address[0] = '\0';
        }

        if (out.tcp_connected)
        {
            TcpChannel::instance().remoteEndpoint(out.server_endpoint,
                                                  sizeof(out.server_endpoint));
        }
        else
        {
            out.server_endpoint[0] = '\0';
        }
    }

} // namespace ekeys
