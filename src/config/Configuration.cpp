/*
 * Configuration.cpp
 *
 * /config.ini 读写与 Profile 元数据。见 Configuration.h。
 */

#include "Configuration.h"

#include <stdio.h>
#include <string.h>

#include <SimpleIni.h>
#include <lvgl.h> // LV_SYMBOL_*

#include "logging/LogManager.h"
#include "services/ConfigStore.h"
#include "services/KeymapRepository.h"

namespace ekeys
{

    namespace
    {

        constexpr const char *kGlobalConfigPath = "/config.ini";
        constexpr const char *kKeymapPathFmt = "/keymap%u.ini"; // /keymap1.ini ~ /keymap8.ini
        constexpr const char *kIconPathFmt = "/icon%u.png";     // Profile 图标（阶段 06 接入）

        /*
         * 设置项 → INI 小节映射（FEATURE_DOC §6）。
         * 返回 nullptr 表示未知字段。
         */
        const char *sectionOfKey(const char *key)
        {
            if (strcmp(key, "active_keymap_profile") == 0 ||
                strcmp(key, "work_mode") == 0 ||
                strcmp(key, "pc_status_mask") == 0)
            {
                return "system";
            }
            if (strncmp(key, "wifi_", 5) == 0 || strcmp(key, "connect_host") == 0)
            {
                return "wifi";
            }
            if (strncmp(key, "rgb_", 4) == 0)
            {
                return "rgb";
            }
            if (strncmp(key, "tft_", 4) == 0)
            {
                return "display";
            }
            if (strcmp(key, "device_volume") == 0 ||
                strcmp(key, "audio_enable") == 0 ||
                strcmp(key, "power_mode") == 0)
            {
                return "audio";
            }
            if (strcmp(key, "config_version") == 0 ||
                strcmp(key, "device_name") == 0 ||
                strcmp(key, "serial_number") == 0)
            {
                return "system";
            }
            if (strncmp(key, "voice_", 6) == 0)
            {
                return "voice";
            }
            return nullptr;
        }

    } // namespace

    Configuration &Configuration::instance()
    {
        static Configuration inst;
        return inst;
    }

    Configuration::Configuration()
        : mutex_(nullptr), repo_(nullptr)
    {
        settings_ = DeviceSettings{}; // 全 0 默认值（FEATURE_DOC §6 占位）

        for (uint8_t i = 0; i < CONFIG_PROFILE_COUNT; ++i)
        {
            snprintf(config_paths_[i], sizeof(config_paths_[i]), kKeymapPathFmt,
                     static_cast<unsigned>(i) + 1U);
            snprintf(icon_paths_[i], sizeof(icon_paths_[i]), kIconPathFmt,
                     static_cast<unsigned>(i) + 1U);
        }

        mutex_ = xSemaphoreCreateMutex();
    }

    Configuration::~Configuration()
    {
        if (mutex_ != nullptr)
        {
            vSemaphoreDelete(mutex_);
        }
    }

    void Configuration::lock()
    {
        if (mutex_ != nullptr)
        {
            xSemaphoreTake(mutex_, portMAX_DELAY);
        }
    }

    void Configuration::unlock()
    {
        if (mutex_ != nullptr)
        {
            xSemaphoreGive(mutex_);
        }
    }

    void Configuration::load()
    {
        lock();
        loadGlobalSettings_locked();
        unlock();
    }

    void Configuration::loadGlobalSettings_locked()
    {
        if (!ConfigStore::exists(kGlobalConfigPath))
        {
            LOG_INFO("CONFIG", "Using default config");
            return;
        }

        CSimpleIniA ini(true, false, false);
        if (!ConfigStore::loadGlobal(kGlobalConfigPath, ini))
        {
            LOG_WARNING("CONFIG", "config.ini unreadable, using defaults");
            return;
        }

        /* 逐段读取已知字段（FEATURE_DOC §6） */
        settings_.active_keymap_profile =
            static_cast<uint8_t>(ini.GetLongValue("system", "active_keymap_profile", 0));
        settings_.work_mode =
            static_cast<uint8_t>(ini.GetLongValue("system", "work_mode", 0));
        settings_.config_version =
            static_cast<uint32_t>(ini.GetLongValue("system", "config_version", 0));

        settings_.wifi_switch =
            static_cast<uint8_t>(ini.GetLongValue("wifi", "wifi_switch", 0));
        settings_.connect_host =
            static_cast<uint8_t>(ini.GetLongValue("wifi", "connect_host", 0));
        strncpy(settings_.wifi_ssid, ini.GetValue("wifi", "wifi_ssid", ""),
                sizeof(settings_.wifi_ssid) - 1);
        settings_.wifi_ssid[sizeof(settings_.wifi_ssid) - 1] = '\0';
        strncpy(settings_.wifi_password, ini.GetValue("wifi", "wifi_password", ""),
                sizeof(settings_.wifi_password) - 1);
        settings_.wifi_password[sizeof(settings_.wifi_password) - 1] = '\0';

        settings_.rgb_mode =
            static_cast<uint8_t>(ini.GetLongValue("rgb", "rgb_mode", 0));
        settings_.rgb_single_colar =
            static_cast<uint8_t>(ini.GetLongValue("rgb", "rgb_single_colar", 0));
        settings_.rgb_click_mode =
            static_cast<uint8_t>(ini.GetLongValue("rgb", "rgb_click_mode", 0));
        settings_.rgb_brightness =
            static_cast<uint8_t>(ini.GetLongValue("rgb", "rgb_brightness", 0));

        settings_.tft_theme =
            static_cast<uint8_t>(ini.GetLongValue("display", "tft_theme", 0));
        settings_.tft_brightness =
            static_cast<uint8_t>(ini.GetLongValue("display", "tft_brightness", 0));

        settings_.device_volume =
            static_cast<uint8_t>(ini.GetLongValue("audio", "device_volume", 0));
        settings_.audio_enable =
            static_cast<uint8_t>(ini.GetLongValue("audio", "audio_enable", 0));
        settings_.power_mode =
            static_cast<uint8_t>(ini.GetLongValue("audio", "power_mode", 0));

        /* A2 修复：补充 voice 段读取，否则重启后语音配置全部丢失 */
        settings_.voice_enable =
            static_cast<uint8_t>(ini.GetLongValue("voice", "voice_enable", 0));
        settings_.voice_trigger_key =
            static_cast<uint8_t>(ini.GetLongValue("voice", "voice_trigger_key", 0));
        settings_.voice_max_record_ms =
            static_cast<uint16_t>(ini.GetLongValue("voice", "voice_max_record_ms", 0));
        settings_.voice_auto_enter =
            static_cast<uint8_t>(ini.GetLongValue("voice", "voice_auto_enter", 0));
        settings_.voice_dev_pid =
            static_cast<uint16_t>(ini.GetLongValue("voice", "voice_dev_pid", 0));
        strncpy(settings_.voice_cuid, ini.GetValue("voice", "voice_cuid", ""),
                sizeof(settings_.voice_cuid) - 1);
        settings_.voice_cuid[sizeof(settings_.voice_cuid) - 1] = '\0';
        strncpy(settings_.voice_baidu_api_key,
                ini.GetValue("voice", "voice_baidu_api_key", ""),
                sizeof(settings_.voice_baidu_api_key) - 1);
        settings_.voice_baidu_api_key[sizeof(settings_.voice_baidu_api_key) - 1] = '\0';
        strncpy(settings_.voice_baidu_secret_key,
                ini.GetValue("voice", "voice_baidu_secret_key", ""),
                sizeof(settings_.voice_baidu_secret_key) - 1);
        settings_.voice_baidu_secret_key[sizeof(settings_.voice_baidu_secret_key) - 1] = '\0';

        strncpy(settings_.device_name, ini.GetValue("system", "device_name", ""),
                sizeof(settings_.device_name) - 1);
        settings_.device_name[sizeof(settings_.device_name) - 1] = '\0';
        strncpy(settings_.serial_number, ini.GetValue("system", "serial_number", ""),
                sizeof(settings_.serial_number) - 1);
        settings_.serial_number[sizeof(settings_.serial_number) - 1] = '\0';

        LOG_INFO("CONFIG", "config.ini loaded (active profile=%u)",
                 static_cast<unsigned>(settings_.active_keymap_profile));
    }

    bool Configuration::saveSetting(const char *key, const char *value)
    {
        const char *section = sectionOfKey(key);
        if (section == nullptr)
        {
            LOG_WARNING("CONFIG", "unknown setting key: %s", key);
            return false;
        }

        lock();
        CSimpleIniA ini(true, false, false);
        ConfigStore::loadGlobal(kGlobalConfigPath, ini); // 不存在则从空文件开始
        ini.SetValue(section, key, value);
        bool ok = ConfigStore::saveGlobal(kGlobalConfigPath, ini);
        unlock();
        return ok;
    }

    bool Configuration::saveSetting(const char *key, int value)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        return saveSetting(key, buf);
    }

    void Configuration::snapshot(DeviceSettings &out)
    {
        lock();
        out = settings_;
        unlock();
    }

    bool Configuration::mutateSettings(const SettingsMutator &mutator)
    {
        if (!mutator)
        {
            return false;
        }
        lock();
        mutator(settings_);
        unlock();
        return true;
    }

    bool Configuration::loadActiveProfileKeyMapping(KeymapArray &out)
    {
        if (repo_ == nullptr)
        {
            LOG_ERROR("CONFIG", "KeymapRepository not injected");
            return false;
        }
        return repo_->loadProfile(getProfileConfigPath(activeProfile()), out);
    }

    bool Configuration::saveKeyMapping(uint8_t keyId, const KeyMapping &mapping)
    {
        if (repo_ == nullptr)
        {
            LOG_ERROR("CONFIG", "KeymapRepository not injected");
            return false;
        }
        return repo_->saveKey(getProfileConfigPath(activeProfile()), keyId, mapping);
    }

    bool Configuration::switchActiveProfile(uint8_t idx)
    {
        if (idx >= CONFIG_PROFILE_COUNT)
        {
            LOG_WARNING("CONFIG", "profile index %u out of range", idx);
            return false;
        }
        settings_.active_keymap_profile = idx;
        return saveSetting("active_keymap_profile", idx);
    }

    const char *Configuration::getProfileConfigPath(uint8_t idx) const
    {
        return config_paths_[idx < CONFIG_PROFILE_COUNT ? idx : 0];
    }

    const char *Configuration::getProfileIconPath(uint8_t idx) const
    {
        return icon_paths_[idx < CONFIG_PROFILE_COUNT ? idx : 0];
    }

    const char *Configuration::getProfileDisplayName(uint8_t idx) const
    {
        /* 内置 8 个 LVGL 符号（FEATURE_DOC §3.3） */
        static const char *kNames[CONFIG_PROFILE_COUNT] = {
            LV_SYMBOL_WIFI,
            LV_SYMBOL_AUDIO,
            LV_SYMBOL_VIDEO,
            LV_SYMBOL_BELL,
            LV_SYMBOL_HOME,
            LV_SYMBOL_SETTINGS,
            LV_SYMBOL_KEYBOARD,
            LV_SYMBOL_BARS,
        };
        return kNames[idx < CONFIG_PROFILE_COUNT ? idx : 0];
    }

} // namespace ekeys
