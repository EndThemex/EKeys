/*
 * cmd_profile.cpp
 *
 * 报文（参考工程 onProfileIconSetCommand / sendCurrentProfileState）：
 *   0x11 请求：data.profile_icon{profile?, clear?, png_base64?}
 *   0x11 响应：data{profile, profile_number, has_custom_icon, profile_name}
 *   0x10 推送：{"cmd":0x10,"seq":N,"profile_state":{
 *     active_profile, profile_number, profile_name,
 *     has_custom_icon, icon_path? }}
 */

#include "cmd_profile.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <string.h>
#include <mbedtls/base64.h>

#include "../../logging/LogManager.h"
#include "../SerialProtocol.h"
#include "../CommandRegistry.h"
#include "../../config/Configuration.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        bool removeProfileIcon(uint8_t profile)
        {
            const char *path =
                Configuration::instance().getProfileIconPath(profile);
            if (!SPIFFS.exists(path))
            {
                return true; // 幂等：本就不存在
            }
            return SPIFFS.remove(path);
        }

        bool saveProfileIconFromBase64(uint8_t profile, const char *png_base64)
        {
            if (png_base64 == nullptr || png_base64[0] == '\0')
            {
                return false;
            }

            /* 解码（PSRAM 不足时退回堆） */
            size_t out_len = 0;
            const size_t src_len = strlen(png_base64);
            if (mbedtls_base64_decode(nullptr, 0, &out_len,
                                      reinterpret_cast<const unsigned char *>(png_base64),
                                      src_len) != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL)
            {
                return false;
            }
            uint8_t *png = static_cast<uint8_t *>(ps_malloc(out_len));
            if (png == nullptr)
            {
                png = static_cast<uint8_t *>(malloc(out_len));
            }
            if (png == nullptr)
            {
                return false;
            }
            size_t decoded = 0;
            const int rc = mbedtls_base64_decode(
                png, out_len, &decoded,
                reinterpret_cast<const unsigned char *>(png_base64), src_len);
            if (rc != 0 || decoded == 0)
            {
                free(png);
                return false;
            }

            const char *path =
                Configuration::instance().getProfileIconPath(profile);
            File f = SPIFFS.open(path, FILE_WRITE);
            if (!f)
            {
                free(png);
                return false;
            }
            const size_t written = f.write(png, decoded);
            f.close();
            free(png);
            LOG_INFO("PROFILE", "icon saved to %s (%u bytes)",
                     path, static_cast<unsigned>(written));
            return written == decoded;
        }

        bool profileIconExists(uint8_t profile)
        {
            return SPIFFS.exists(
                Configuration::instance().getProfileIconPath(profile));
        }

        int handleProfileState(int cmd, int seq, JsonObject /*data*/)
        {
            (void)cmd;
            sendProfileState(seq);
            return 0;
        }

        int handleProfileIconSet(int cmd, int seq, JsonObject data)
        {
            JsonObject icon = data["profile_icon"].as<JsonObject>();
            if (icon.isNull())
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "missing 'profile_icon'");
                return -1;
            }

            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            const uint8_t profile = icon["profile"] | snap.active_keymap_profile;
            if (profile >= Configuration::CONFIG_PROFILE_COUNT)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "profile out of range");
                return -1;
            }

            bool success = false;
            const bool clear_icon = icon["clear"] | false;
            const char *error = "";
            if (clear_icon)
            {
                success = removeProfileIcon(profile);
                error = "remove icon failed";
            }
            else if (icon["png_base64"].is<const char *>())
            {
                success = saveProfileIconFromBase64(
                    profile, icon["png_base64"].as<const char *>());
                error = "save icon failed";
            }
            else
            {
                error = "png_base64 missing";
            }

            if (!success)
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq, error);
                return -1;
            }

            {
                JsonDocument resp;
                resp["cmd"] = cmd | 0x80;
                resp["seq"] = seq;
                resp["status"] = 0;
                JsonObject out = resp["data"].to<JsonObject>();
                out["profile"] = profile;
                out["profile_number"] = profile + 1;
                out["has_custom_icon"] = profileIconExists(profile);
                out["profile_name"] =
                    Configuration::instance().getProfileDisplayName(profile);
                SerialProtocol::instance().sendDocument(resp);
            }

            /* 图标变化后同步 profile 状态（seq=0 推送） */
            sendProfileState(0);
            return 0;
        }

    } // namespace

    void sendProfileState(int seq)
    {
        DeviceSettings snap;
        Configuration::instance().snapshot(snap);
        const uint8_t active = snap.active_keymap_profile;
        const bool has_icon =
            SPIFFS.exists(Configuration::instance().getProfileIconPath(active));

        JsonDocument doc;
        doc["cmd"] = CMD_PROFILE_STATE;
        doc["seq"] = seq;
        JsonObject state = doc["profile_state"].to<JsonObject>();
        state["active_profile"] = active;
        state["profile_number"] = active + 1;
        state["profile_name"] =
            Configuration::instance().getProfileDisplayName(active);
        state["has_custom_icon"] = has_icon;
        if (has_icon)
        {
            state["icon_path"] =
                Configuration::instance().getProfileIconPath(active);
        }
        SerialProtocol::instance().sendDocument(doc);
    }

    void registerProfileHandlers()
    {
        CommandRegistry::instance().registerHandler(
            CMD_PROFILE_STATE, handleProfileState);
        CommandRegistry::instance().registerHandler(
            CMD_PROFILE_ICON_SET, handleProfileIconSet);
        LOG_INFO("CMD", "cmd_profile registered (0x%02X/0x%02X)",
                 CMD_PROFILE_STATE, CMD_PROFILE_ICON_SET);
    }

    void unregisterProfileHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_PROFILE_STATE);
        CommandRegistry::instance().unregisterHandler(CMD_PROFILE_ICON_SET);
    }

} // namespace ekeys::protocol::commands
