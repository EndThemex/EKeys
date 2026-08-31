/*
 * parseConfigSetCommand.cpp
 *
 * 见 parseConfigSetCommand.h。
 */

#include "parseConfigSetCommand.h"

#include <stdio.h>
#include <string.h>

#include "logging/LogManager.h"

// 配置层接口与 DeviceSettings
#include "config/Configuration.h"

namespace ekeys {

namespace {

constexpr size_t kMaxIntChanges = 20;
constexpr size_t kMaxStrChanges = 5;
constexpr size_t kMaxMaskChanges = 2;

struct IntChange {
    const char *key;
    int value;
};
struct StrChange {
    const char *key;
    const char *value;
};
struct MaskChange {
    const char *key;
    uint32_t value;
};

}  // namespace

bool parseConfigSetCommand(JsonObject cfg, ConfigSetResult &result)
{
    if (cfg.isNull()) {
        LOG_ERROR("CFG_SET", "config object missing");
        return false;
    }

    IntChange int_changes[kMaxIntChanges];
    size_t n_int = 0;
    StrChange str_changes[kMaxStrChanges];
    size_t n_str = 0;
    MaskChange mask_changes[kMaxMaskChanges];
    size_t n_mask = 0;
    char mask_bufs[kMaxMaskChanges][12];
    bool want_profile = false;
    uint8_t new_profile = 0;

    Configuration &config = Configuration::instance();

    config.mutateSettings([&](DeviceSettings &s) {
        /* ---- WiFi / 主机连接 ---- */
        if (!cfg["wifi_switch"].isNull()) {
            int v = cfg["wifi_switch"].as<int>();
            v = (v != 0) ? 1 : 0;
            if (s.wifi_switch != v) {
                LOG_INFO("CFG_SET", "wifi_switch: %u -> %u",
                         s.wifi_switch, v);
                s.wifi_switch = static_cast<uint8_t>(v);
                result.any_changed = true;
                result.wifi_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"wifi_switch", v};
                }
            }
        }
        if (!cfg["connect_host"].isNull()) {
            int v = cfg["connect_host"].as<int>();
            v = (v != 0) ? 1 : 0;
            if (s.connect_host != v) {
                LOG_INFO("CFG_SET", "connect_host: %u -> %u",
                         s.connect_host, v);
                s.connect_host = static_cast<uint8_t>(v);
                result.any_changed = true;
                result.wifi_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"connect_host", v};
                }
            }
        }
        if (!cfg["wifi_ssid"].isNull()) {
            const char *v = cfg["wifi_ssid"].as<const char *>();
            if (v != nullptr && strcmp(s.wifi_ssid, v) != 0) {
                LOG_INFO("CFG_SET", "wifi_ssid: '%s' -> '%s'", s.wifi_ssid, v);
                strncpy(s.wifi_ssid, v, sizeof(s.wifi_ssid) - 1);
                s.wifi_ssid[sizeof(s.wifi_ssid) - 1] = '\0';
                result.any_changed = true;
                result.wifi_changed = true;
                if (n_str < kMaxStrChanges) {
                    str_changes[n_str++] = {"wifi_ssid", s.wifi_ssid};
                }
            }
        }
        if (!cfg["wifi_password"].isNull()) {
            const char *v = cfg["wifi_password"].as<const char *>();
            if (v != nullptr && strcmp(s.wifi_password, v) != 0) {
                LOG_INFO("CFG_SET", "wifi_password changed (len %u -> %u)",
                         strlen(s.wifi_password), strlen(v));
                strncpy(s.wifi_password, v, sizeof(s.wifi_password) - 1);
                s.wifi_password[sizeof(s.wifi_password) - 1] = '\0';
                result.any_changed = true;
                result.wifi_changed = true;
                if (n_str < kMaxStrChanges) {
                    str_changes[n_str++] = {"wifi_password", s.wifi_password};
                }
            }
        }

        /* ---- 工作模式 ---- */
        if (!cfg["work_mode"].isNull()) {
            int v = cfg["work_mode"].as<int>();
            if (v >= 0 && v <= 2) {
                if (s.work_mode != v) {
                    LOG_INFO("CFG_SET", "work_mode: %u -> %u", s.work_mode, v);
                    s.work_mode = static_cast<uint8_t>(v);
                    result.any_changed = true;
                    result.work_mode_changed = true;
                    result.work_mode = static_cast<uint8_t>(v);
                    if (n_int < kMaxIntChanges) {
                        int_changes[n_int++] = {"work_mode", v};
                    }
                }
            } else {
                LOG_WARNING("CFG_SET", "work_mode %d out of range, ignored", v);
            }
        }

        /* ---- RGB ---- */
        if (!cfg["rgb_mode"].isNull()) {
            int v = cfg["rgb_mode"].as<int>();
            if (s.rgb_mode != v) {
                LOG_INFO("CFG_SET", "rgb_mode: %u -> %d", s.rgb_mode, v);
                s.rgb_mode = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"rgb_mode", v};
                }
            }
        }
        if (!cfg["rgb_single_colar"].isNull()) {
            int v = cfg["rgb_single_colar"].as<int>();
            if (s.rgb_single_colar != v) {
                LOG_INFO("CFG_SET", "rgb_single_colar: %u -> %d",
                         s.rgb_single_colar, v);
                s.rgb_single_colar = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"rgb_single_colar", v};
                }
            }
        }
        if (!cfg["rgb_click_mode"].isNull()) {
            int v = cfg["rgb_click_mode"].as<int>();
            if (s.rgb_click_mode != v) {
                LOG_INFO("CFG_SET", "rgb_click_mode: %u -> %d",
                         s.rgb_click_mode, v);
                s.rgb_click_mode = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"rgb_click_mode", v};
                }
            }
        }
        if (!cfg["rgb_brightness"].isNull()) {
            int v = cfg["rgb_brightness"].as<int>();
            if (s.rgb_brightness != v) {
                LOG_INFO("CFG_SET", "rgb_brightness: %u -> %d",
                         s.rgb_brightness, v);
                s.rgb_brightness = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"rgb_brightness", v};
                }
            }
        }

        /* ---- 屏幕（亮度下限 5，FEATURE_DOC §6） ---- */
        if (!cfg["tft_theme"].isNull()) {
            int v = cfg["tft_theme"].as<int>();
            if (s.tft_theme != v) {
                LOG_INFO("CFG_SET", "tft_theme: %u -> %d", s.tft_theme, v);
                s.tft_theme = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"tft_theme", v};
                }
            }
        }
        if (!cfg["tft_brightness"].isNull()) {
            int v = cfg["tft_brightness"].as<int>();
            if (v < 5) {
                v = 5;
            }
            if (v > 100) {
                v = 100;
            }
            if (s.tft_brightness != v) {
                LOG_INFO("CFG_SET", "tft_brightness: %u -> %d",
                         s.tft_brightness, v);
                s.tft_brightness = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"tft_brightness", v};
                }
            }
        }

        /* ---- 音频 ---- */
        if (!cfg["device_volume"].isNull()) {
            int v = cfg["device_volume"].as<int>();
            if (s.device_volume != v) {
                LOG_INFO("CFG_SET", "device_volume: %u -> %d",
                         s.device_volume, v);
                s.device_volume = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"device_volume", v};
                }
            }
        }
        if (!cfg["audio_enable"].isNull()) {
            int v = cfg["audio_enable"].as<int>();
            if (s.audio_enable != v) {
                LOG_INFO("CFG_SET", "audio_enable: %u -> %d",
                         s.audio_enable, v);
                s.audio_enable = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"audio_enable", v};
                }
            }
        }
        if (!cfg["power_mode"].isNull()) {
            int v = cfg["power_mode"].as<int>();
            if (s.power_mode != v) {
                LOG_INFO("CFG_SET", "power_mode: %u -> %d", s.power_mode, v);
                s.power_mode = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"power_mode", v};
                }
            }
        }

        /* ---- 语音 ---- */
        if (!cfg["voice_enable"].isNull()) {
            int v = cfg["voice_enable"].as<int>();
            if (s.voice_enable != v) {
                LOG_INFO("CFG_SET", "voice_enable: %u -> %d",
                         s.voice_enable, v);
                s.voice_enable = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"voice_enable", v};
                }
            }
        }
        if (!cfg["voice_trigger_key"].isNull()) {
            int v = cfg["voice_trigger_key"].as<int>();
            if (s.voice_trigger_key != v) {
                LOG_INFO("CFG_SET", "voice_trigger_key: %u -> %d",
                         s.voice_trigger_key, v);
                s.voice_trigger_key = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"voice_trigger_key", v};
                }
            }
        }
        if (!cfg["voice_max_record_ms"].isNull()) {
            int v = cfg["voice_max_record_ms"].as<int>();
            if (s.voice_max_record_ms != v) {
                LOG_INFO("CFG_SET", "voice_max_record_ms: %u -> %d",
                         s.voice_max_record_ms, v);
                s.voice_max_record_ms = static_cast<uint16_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"voice_max_record_ms", v};
                }
            }
        }
        if (!cfg["voice_auto_enter"].isNull()) {
            int v = cfg["voice_auto_enter"].as<int>();
            if (s.voice_auto_enter != v) {
                LOG_INFO("CFG_SET", "voice_auto_enter: %u -> %d",
                         s.voice_auto_enter, v);
                s.voice_auto_enter = static_cast<uint8_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"voice_auto_enter", v};
                }
            }
        }
        if (!cfg["voice_dev_pid"].isNull()) {
            int v = cfg["voice_dev_pid"].as<int>();
            if (s.voice_dev_pid != v) {
                LOG_INFO("CFG_SET", "voice_dev_pid: %u -> %d",
                         s.voice_dev_pid, v);
                s.voice_dev_pid = static_cast<uint16_t>(v);
                result.any_changed = true;
                if (n_int < kMaxIntChanges) {
                    int_changes[n_int++] = {"voice_dev_pid", v};
                }
            }
        }
        if (!cfg["voice_cuid"].isNull()) {
            const char *v = cfg["voice_cuid"].as<const char *>();
            if (v != nullptr && strcmp(s.voice_cuid, v) != 0) {
                LOG_INFO("CFG_SET", "voice_cuid: '%s' -> '%s'",
                         s.voice_cuid, v);
                strncpy(s.voice_cuid, v, sizeof(s.voice_cuid) - 1);
                s.voice_cuid[sizeof(s.voice_cuid) - 1] = '\0';
                result.any_changed = true;
                if (n_str < kMaxStrChanges) {
                    str_changes[n_str++] = {"voice_cuid", s.voice_cuid};
                }
            }
        }
        if (!cfg["voice_baidu_api_key"].isNull()) {
            const char *v = cfg["voice_baidu_api_key"].as<const char *>();
            if (v != nullptr && strcmp(s.voice_baidu_api_key, v) != 0) {
                LOG_INFO("CFG_SET", "voice_baidu_api_key changed (len %u)",
                         strlen(v));
                strncpy(s.voice_baidu_api_key, v,
                        sizeof(s.voice_baidu_api_key) - 1);
                s.voice_baidu_api_key[sizeof(s.voice_baidu_api_key) - 1] = '\0';
                result.any_changed = true;
                if (n_str < kMaxStrChanges) {
                    str_changes[n_str++] = {"voice_baidu_api_key",
                                            s.voice_baidu_api_key};
                }
            }
        }
        if (!cfg["voice_baidu_secret_key"].isNull()) {
            const char *v = cfg["voice_baidu_secret_key"].as<const char *>();
            if (v != nullptr && strcmp(s.voice_baidu_secret_key, v) != 0) {
                LOG_INFO("CFG_SET", "voice_baidu_secret_key changed (len %u)",
                         strlen(v));
                strncpy(s.voice_baidu_secret_key, v,
                        sizeof(s.voice_baidu_secret_key) - 1);
                s.voice_baidu_secret_key[
                    sizeof(s.voice_baidu_secret_key) - 1] = '\0';
                result.any_changed = true;
                if (n_str < kMaxStrChanges) {
                    str_changes[n_str++] = {"voice_baidu_secret_key",
                                            s.voice_baidu_secret_key};
                }
            }
        }

        /* ---- PC 状态位掩码 ---- */
        if (!cfg["pc_status_mask"].isNull()) {
            uint32_t v = cfg["pc_status_mask"].as<uint32_t>();
            if (s.pc_status_mask != v) {
                LOG_INFO("CFG_SET", "pc_status_mask: 0x%08lX -> 0x%08lX",
                         static_cast<unsigned long>(s.pc_status_mask),
                         static_cast<unsigned long>(v));
                s.pc_status_mask = v;
                result.any_changed = true;
                if (n_mask < kMaxMaskChanges) {
                    mask_changes[n_mask++] = {"pc_status_mask", v};
                }
            }
        }

        /* ---- Profile 切换（经 switchActiveProfile 统一持久化） ---- */
        if (!cfg["active_keymap_profile"].isNull()) {
            int v = cfg["active_keymap_profile"].as<int>();
            if (v >= 0 && v < Configuration::CONFIG_PROFILE_COUNT) {
                if (s.active_keymap_profile != v) {
                    LOG_INFO("CFG_SET", "active_keymap_profile: %u -> %d",
                             s.active_keymap_profile, v);
                    want_profile = true;
                    new_profile = static_cast<uint8_t>(v);
                }
            } else {
                LOG_WARNING("CFG_SET",
                            "active_keymap_profile %d out of range, ignored",
                            v);
            }
        }
    });

    /* ---- 逐键持久化（saveSetting 内部自行加锁） ---- */
    for (size_t i = 0; i < n_int; ++i) {
        config.saveSetting(int_changes[i].key, int_changes[i].value);
    }
    for (size_t i = 0; i < n_str; ++i) {
        config.saveSetting(str_changes[i].key, str_changes[i].value);
    }
    for (size_t i = 0; i < n_mask; ++i) {
        snprintf(mask_bufs[i], sizeof(mask_bufs[i]), "%lu",
                 static_cast<unsigned long>(mask_changes[i].value));
        config.saveSetting(mask_changes[i].key, mask_bufs[i]);
    }
    if (want_profile) {
        config.switchActiveProfile(new_profile);
        result.profile_changed = true;
        result.any_changed = true;
    }

    return true;
}

}  // namespace ekeys
