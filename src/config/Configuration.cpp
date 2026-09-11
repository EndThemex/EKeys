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
                strcmp(key, "pc_status_mask") == 0 ||
                strcmp(key, "fun_key1") == 0 ||
                strcmp(key, "fun_key2") == 0)
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
                strcmp(key, "serial_number") == 0 ||
                strcmp(key, "ui_lang") == 0)
            {
                return "system";
            }
            if (strncmp(key, "voice_", 6) == 0)
            {
                return "voice";
            }
            /* Profile 名称（APP 下发）：profile_name_0 ~ profile_name_7 */
            if (strncmp(key, "profile_name_", 13) == 0)
            {
                return "profile";
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
            profile_names_[i][0] = '\0';
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
        /*
         * 首启无 config.ini / 文件损坏时不再提前 return：
         * 用空 ini 继续走逐段读取，让 GetLongValue 的默认值生效
         * （如 tft_brightness=80）。否则 settings_ 停留构造时的全 0，
         * 背光会被 Backlight 钳到最小 5%。
         */
        CSimpleIniA ini(true, false, false);
        if (!ConfigStore::exists(kGlobalConfigPath))
        {
            LOG_INFO("CONFIG", "Using default config");
        }
        else if (!ConfigStore::loadGlobal(kGlobalConfigPath, ini))
        {
            LOG_WARNING("CONFIG", "config.ini unreadable, using defaults");
        }

        /* 逐段读取已知字段（FEATURE_DOC §6） */
        settings_.active_keymap_profile =
            static_cast<uint8_t>(ini.GetLongValue("system", "active_keymap_profile", 0));
        settings_.work_mode =
            static_cast<uint8_t>(ini.GetLongValue("system", "work_mode", 0));
        settings_.config_version =
            static_cast<uint32_t>(ini.GetLongValue("system", "config_version", 0));
        /* F6 修复：pc_status_mask 作为 uint32 加载，long 范围足够 */
        settings_.pc_status_mask =
            static_cast<uint32_t>(ini.GetLongValue("system", "pc_status_mask", 0));
        /*
         * FUN 组合键（0=未配置）：0x06 写入经 saveSettings 持久化到
         * system 节，这里必须读回，否则重启后 fun_key 静默归 0，
         * FUN 预扫描永不命中（组合键失效 + 键映射屏 FUN 预览不切换）。
         */
        settings_.fun_key1 =
            static_cast<uint8_t>(ini.GetLongValue("system", "fun_key1", 0));
        settings_.fun_key2 =
            static_cast<uint8_t>(ini.GetLongValue("system", "fun_key2", 0));

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
        settings_.rgb_single_color =
            static_cast<uint8_t>(ini.GetLongValue("rgb", "rgb_single_color", 0));
        settings_.rgb_click_mode =
            static_cast<uint8_t>(ini.GetLongValue("rgb", "rgb_click_mode", 0));
        settings_.rgb_brightness =
            static_cast<uint8_t>(ini.GetLongValue("rgb", "rgb_brightness", 0));

        settings_.tft_theme =
            static_cast<uint8_t>(ini.GetLongValue("display", "tft_theme", 0));
        settings_.tft_brightness =
            static_cast<uint8_t>(ini.GetLongValue("display", "tft_brightness", 80));

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
        strncpy(settings_.voice_cuid, ini.GetValue("voice", "voice_cuid", ""),
                sizeof(settings_.voice_cuid) - 1);
        settings_.voice_cuid[sizeof(settings_.voice_cuid) - 1] = '\0';
        strncpy(settings_.voice_tencent_secret_id,
                ini.GetValue("voice", "voice_tencent_secret_id", ""),
                sizeof(settings_.voice_tencent_secret_id) - 1);
        settings_.voice_tencent_secret_id[sizeof(settings_.voice_tencent_secret_id) - 1] = '\0';
        strncpy(settings_.voice_tencent_secret_key,
                ini.GetValue("voice", "voice_tencent_secret_key", ""),
                sizeof(settings_.voice_tencent_secret_key) - 1);
        settings_.voice_tencent_secret_key[sizeof(settings_.voice_tencent_secret_key) - 1] = '\0';

        strncpy(settings_.device_name, ini.GetValue("system", "device_name", ""),
                sizeof(settings_.device_name) - 1);
        settings_.device_name[sizeof(settings_.device_name) - 1] = '\0';
        strncpy(settings_.serial_number, ini.GetValue("system", "serial_number", ""),
                sizeof(settings_.serial_number) - 1);
        settings_.serial_number[sizeof(settings_.serial_number) - 1] = '\0';

        /* 主页日期/星期语言：0=中文（默认），1=英文 */
        settings_.ui_lang =
            static_cast<uint8_t>(ini.GetLongValue("system", "ui_lang", 0));
        if (settings_.ui_lang > 1)
        {
            settings_.ui_lang = 0;
        }

        /* Profile 名称（APP 下发，UTF-8，空=未设置回退内置符号名） */
        for (uint8_t i = 0; i < CONFIG_PROFILE_COUNT; ++i)
        {
            char key[20];
            snprintf(key, sizeof(key), "profile_name_%u",
                     static_cast<unsigned>(i));
            const char *name = ini.GetValue("profile", key, "");
            strncpy(profile_names_[i], name, sizeof(profile_names_[i]) - 1);
            profile_names_[i][sizeof(profile_names_[i]) - 1] = '\0';
        }

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

    /* F6 修复：uint32 持久化，避免高位被 int 强转破坏 */
    bool Configuration::saveSetting(const char *key, uint32_t value)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%lu",
                 static_cast<unsigned long>(value));
        return saveSetting(key, buf);
    }

    bool Configuration::saveSettings(
        const std::initializer_list<std::pair<const char *, int>> &kvs)
    {
        return saveSettings(kvs.begin(), kvs.size());
    }

    bool Configuration::saveSettings(const std::pair<const char *, int> *kvs,
                                     size_t count)
    {
        if (count == 0)
        {
            return true;
        }

        /* 先校验全部键，避免写了一半才发现非法 */
        for (size_t i = 0; i < count; ++i)
        {
            if (sectionOfKey(kvs[i].first) == nullptr)
            {
                LOG_WARNING("CONFIG", "unknown setting key: %s", kvs[i].first);
                return false;
            }
        }

        lock();
        CSimpleIniA ini(true, false, false);
        ConfigStore::loadGlobal(kGlobalConfigPath, ini); // 不存在则从空文件开始
        for (size_t i = 0; i < count; ++i)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", kvs[i].second);
            ini.SetValue(sectionOfKey(kvs[i].first), kvs[i].first, buf);
        }
        bool ok = ConfigStore::saveGlobal(kGlobalConfigPath, ini);
        unlock();
        return ok;
    }

    bool Configuration::saveSettings(
        const std::pair<const char *, const char *> *kvs, size_t count)
    {
        if (count == 0)
        {
            return true;
        }

        for (size_t i = 0; i < count; ++i)
        {
            if (sectionOfKey(kvs[i].first) == nullptr)
            {
                LOG_WARNING("CONFIG", "unknown setting key: %s", kvs[i].first);
                return false;
            }
        }

        lock();
        CSimpleIniA ini(true, false, false);
        ConfigStore::loadGlobal(kGlobalConfigPath, ini);
        for (size_t i = 0; i < count; ++i)
        {
            ini.SetValue(sectionOfKey(kvs[i].first), kvs[i].first,
                         kvs[i].second);
        }
        bool ok = ConfigStore::saveGlobal(kGlobalConfigPath, ini);
        unlock();
        return ok;
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

    bool Configuration::loadProfileKeyMapping(uint8_t idx, KeymapArray &out)
    {
        if (repo_ == nullptr)
        {
            LOG_ERROR("CONFIG", "KeymapRepository not injected");
            return false;
        }
        if (idx >= CONFIG_PROFILE_COUNT)
        {
            LOG_WARNING("CONFIG", "profile index %u out of range", idx);
            return false;
        }
        return repo_->loadProfile(getProfileConfigPath(idx), out);
    }

    bool Configuration::saveKeyMappings(const KeymapArray &mappings,
                                        uint16_t keyMask)
    {
        if (repo_ == nullptr)
        {
            LOG_ERROR("CONFIG", "KeymapRepository not injected");
            return false;
        }
        return repo_->saveKeys(getProfileConfigPath(activeProfile()),
                               mappings, keyMask);
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
        if (idx >= CONFIG_PROFILE_COUNT)
        {
            idx = 0;
        }
        /* APP 下发名称优先（UTF-8 中文，设备 UI 用 CKJGT 字体渲染） */
        if (profile_names_[idx][0] != '\0')
        {
            return profile_names_[idx];
        }
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
        return kNames[idx];
    }

    bool Configuration::isProfileNameCustom(uint8_t idx) const
    {
        if (idx >= CONFIG_PROFILE_COUNT)
        {
            return false;
        }
        /*
         * 读写均为 MainTask 单线程（协议命令处理与 UI 消息构造同任务），
         * 32 字节数组读取无需持锁；写侧 setProfileName 亦在 MainTask。
         */
        return profile_names_[idx][0] != '\0';
    }

    bool Configuration::setProfileName(uint8_t idx, const char *name)
    {
        if (idx >= CONFIG_PROFILE_COUNT)
        {
            LOG_WARNING("CONFIG", "profile name idx %u out of range", idx);
            return false;
        }
        if (name == nullptr)
        {
            name = "";
        }

        /*
         * UTF-8 安全截断：只保留完整字符（首字节 10xxxxxx 为 continuation，
         * 截断点回退到字符起始字节），避免把多字节中文截成非法序列。
         */
        char trimmed[kProfileNameMaxLen];
        size_t len = strlen(name);
        if (len >= sizeof(trimmed))
        {
            len = sizeof(trimmed) - 1;
            while (len > 0 && (static_cast<unsigned char>(name[len]) & 0xC0) == 0x80)
            {
                --len; /* 回退到字符首字节 */
            }
        }
        memcpy(trimmed, name, len);
        trimmed[len] = '\0';
        if (len != strlen(name))
        {
            LOG_WARNING("CONFIG", "profile name too long, truncated to %u bytes",
                        static_cast<unsigned>(len));
        }

        lock();
        strncpy(profile_names_[idx], trimmed, sizeof(profile_names_[idx]) - 1);
        profile_names_[idx][sizeof(profile_names_[idx]) - 1] = '\0';
        unlock();

        /* 持久化（空串=清除，回退内置符号名）；saveSetting 内部自行加锁 */
        char key[20];
        snprintf(key, sizeof(key), "profile_name_%u", static_cast<unsigned>(idx));
        if (!saveSetting(key, trimmed))
        {
            LOG_ERROR("CONFIG", "persist %s failed", key);
            return false;
        }
        LOG_INFO("CONFIG", "profile_name_%u set to \"%s\"",
                 static_cast<unsigned>(idx), trimmed);
        return true;
    }

} // namespace ekeys
