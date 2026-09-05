/*
 * cmd_config.cpp
 *
 * CMD_CONFIG_GET / CMD_CONFIG_SET handler（FEATURE_DOC §5.3/§6）。
 *
 * GET：Configuration::snapshot() 序列化全部 §6 字段 + profile 元数据。
 * SET：parseConfigSetCommand 原子写入，随后：
 *   1) 回 success 响应
 *   2) 执行副作用（work_mode 重建键盘 / profile 重载键映射）
 *   3) 向 DisplayTask 投递 SETTING_UPDATE（亮度即时生效）
 *   4) 主动上报新快照（seq=0），桌面 App 免二次 GET
 */

#include "cmd_config.h"

#include <ArduinoJson.h>

#include "app/AppContext.h"
#include "audio/Speaker.h"
#include "config/Configuration.h"
#include "config/parseConfigSetCommand.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include "network/WiFiManager.h"
#include "services/ConfigStore.h"
#include "tasks/DisplayTask.h"
#include "../CommandRegistry.h"
#include "../SerialProtocol.h"

namespace ekeys::protocol::commands
{

    namespace
    {

        /* 上报当前配置快照（cmd=0x87） */
        void sendConfigSnapshot(int seq)
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            const uint8_t profile = snap.active_keymap_profile;

            JsonDocument doc;
            doc["cmd"] = CMD_CONFIG_GET | 0x80;
            doc["seq"] = seq;
            doc["status"] = 0;
            JsonObject cfg = doc["data"].to<JsonObject>();

            cfg["wifi_switch"] = snap.wifi_switch;
            cfg["connect_host"] = snap.connect_host;
            cfg["wifi_ssid"] = snap.wifi_ssid;
            cfg["wifi_password"] = snap.wifi_password;
            cfg["work_mode"] = snap.work_mode;
            cfg["rgb_mode"] = snap.rgb_mode;
            cfg["rgb_single_colar"] = snap.rgb_single_colar;
            cfg["rgb_click_mode"] = snap.rgb_click_mode;
            cfg["rgb_brightness"] = snap.rgb_brightness;
            cfg["tft_theme"] = snap.tft_theme;
            cfg["tft_brightness"] = snap.tft_brightness;
            cfg["device_volume"] = snap.device_volume;
            cfg["audio_enable"] = snap.audio_enable;
            cfg["power_mode"] = snap.power_mode;
            cfg["voice_enable"] = snap.voice_enable;
            cfg["voice_trigger_key"] = snap.voice_trigger_key;
            cfg["voice_max_record_ms"] = snap.voice_max_record_ms;
            cfg["voice_auto_enter"] = snap.voice_auto_enter;
            cfg["voice_dev_pid"] = snap.voice_dev_pid;
            cfg["voice_cuid"] = snap.voice_cuid;
            cfg["voice_baidu_api_key"] = snap.voice_baidu_api_key;
            cfg["voice_baidu_secret_key"] = snap.voice_baidu_secret_key;
            cfg["pc_status_mask"] = snap.pc_status_mask;
            cfg["active_keymap_profile"] = profile;
            cfg["active_profile_name"] =
                Configuration::instance().getProfileDisplayName(profile);
            cfg["active_profile_has_custom_icon"] =
                ConfigStore::exists(Configuration::instance().getProfileIconPath(profile));

            SerialProtocol::instance().sendDocument(doc);
        }

        /* 向 DisplayTask 投递 SETTING_UPDATE（全量快照，驱动 UI + 背光） */
        void postSettingUpdate()
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);

            DisplayMessage msg;
            msg.type = DisplayMessageType::SettingUpdate;
            fillSettingPayload(snap, msg.setting);
            DisplayTask::instance().post(msg, 0);
        }

        int handleConfigGet(int cmd, int seq, JsonObject /*data*/)
        {
            sendConfigSnapshot(seq);
            (void)cmd;
            return 0;
        }

        int handleConfigSet(int cmd, int seq, JsonObject data)
        {
            JsonObject cfg = data["config"].as<JsonObject>();
            ConfigSetResult result;
            if (!parseConfigSetCommand(cfg, result))
            {
                SerialProtocol::instance().sendErrorResponse(cmd, seq,
                                                             "missing 'config'");
                return -1;
            }

            /* 成功响应 */
            {
                JsonDocument resp;
                resp["cmd"] = cmd | 0x80;
                resp["seq"] = seq;
                resp["status"] = 0;
                SerialProtocol::instance().sendDocument(resp);
            }

            /*
             * C6 修复：副作用统一到 AppContext::applyUiSideEffects，
             * 与 MainTask::applyUiSettingsSnapshot 走同一路径，避免两份代码漂移。
             */
            DeviceSettings prev{};
            DeviceSettings curr{};
            Configuration::instance().snapshot(prev);
            curr = prev;
            /* 应用本次变更到 curr（仅内存，不写盘：持久化已由 parseConfigSetCommand 完成） */
            if (result.work_mode_changed)
            {
                curr.work_mode = result.work_mode;
            }
            if (result.profile_changed)
            {
                /* active_keymap_profile 由 parseConfigSetCommand 单独写入；
                 * 此处读取最新值用于 diff。 */
                Configuration::instance().snapshot(curr);
            }
            AppContext::instance().applyUiSideEffects(prev, curr);
            if (result.wifi_changed)
            {
                LOG_INFO("CFG_CMD", "wifi setting changed, reconnect scheduled");
            }

            if (result.any_changed)
            {
                DeviceSettings snap;
                Configuration::instance().snapshot(snap);
                /* 音量即时生效（cmd handler 与 Speaker::loop 同在 MainTask 上下文） */
                Speaker::instance().applyDeviceVolume(snap.device_volume);
                postSettingUpdate();
                sendConfigSnapshot(0);
            }
            return 0;
        }

    } // namespace

    void registerConfigHandlers()
    {
        CommandRegistry::instance().registerHandler(
            CMD_CONFIG_GET, handleConfigGet);
        CommandRegistry::instance().registerHandler(
            CMD_CONFIG_SET, handleConfigSet);
        LOG_INFO("CFG_CMD", "handlers registered (GET=0x%02X SET=0x%02X)",
                 CMD_CONFIG_GET, CMD_CONFIG_SET);
    }

    void unregisterConfigHandlers()
    {
        CommandRegistry::instance().unregisterHandler(CMD_CONFIG_GET);
        CommandRegistry::instance().unregisterHandler(CMD_CONFIG_SET);
    }

} // namespace ekeys::protocol::commands
