/*
 * message_types.h
 *
 * MainTask → DisplayTask 消息定义（FEATURE_DOC §8.3）。
 *
 * 阶段 05：补齐 11 屏 UI 所需载荷（PC 状态 / HA 状态 / 音乐 / 录音 /
 * 模块状态 / 键映射 Profile / 旋钮动作）。各数据源随阶段 06/07 逐步接入，
 * 当前仅 cmd_config（SettingUpdate）、旋钮（ActionInput）与
 * 设置屏反向同步（SettingUpdate）会实际投递。
 *
 * 说明：载荷字段平铺在 DisplayMessage 上（不用 union）——
 * 各 Info 结构带默认值初始化器，放入 union 在 C++ 中非法；
 * 消息体积约 1.2KB，队列长度 10，内存开销可接受。
 *
 * 载荷为 POD（无 String），队列按值拷贝安全。
 */

#ifndef EKEYS_MESSAGE_TYPES_H
#define EKEYS_MESSAGE_TYPES_H

#include <stdio.h>

#include <stdint.h>

#include "config/DeviceSettings.h"
#include "ui/ui_settings_types.h"

namespace ekeys
{

    enum class DisplayMessageType : uint8_t
    {
        SettingUpdate = 0,
        TimeUpdate = 1,
        ActionInput = 2,
        KeyInput = 3,
        ModuleStatus = 4,
        AsrRecording = 5,
        PcStatus = 6,
        HaStatus = 7,
        MusicPlayer = 8,
        KeymapProfile = 9,
        Navigate = 10, /* 屏幕路由（docs/05 §5.2），载荷 navigate_target */
        BatteryStatus = 11, /* 电池电量（5s 节流，载荷 battery_percent） */
    };

    /* PC 状态（CMD_PC_STATUS，阶段 06 起由协议层填充） */
    struct PcStatusInfo
    {
        bool caps_lock{false};
        bool num_lock{false};
        bool scroll_lock{false};
        bool network_connected{false};
        float cpu_usage_percent{-1.0f};
        float memory_usage_percent{-1.0f};
        float cpu_temp_c{-1.0f};
        float disk_io_percent{-1.0f};
        float network_up_kbps{-1.0f};
        float network_down_kbps{-1.0f};
    };

    /* HA 状态聚合（阶段 06 TCP/WiFi 接入后填充） */
    struct HaStatusInfo
    {
        bool wifi_enabled{false};
        bool wifi_connected{false};
        int wifi_rssi{-100};
        bool tcp_connected{false};
        int work_mode{0};
        bool voice_enabled{false};
        bool voice_recording{false};
        bool module_a_connected{false};
        bool module_b_connected{false};
        char ip_address[24]{0};
        char server_endpoint[32]{0};
    };

    /* 音乐播放器状态（桌面 App 推送，阶段 06 起接入） */
    struct MusicPlayerInfo
    {
        bool connected{false};
        bool is_playing{false};
        bool is_paused{false};
        bool can_prev{false};
        bool can_next{false};
        uint16_t current_seconds{0};
        uint16_t total_seconds{0};
        char title[96]{0};
        char artist[64]{0};
        char player_name[32]{0};
        char lyric_current[160]{0};
        char lyric_next[160]{0};
    };

    struct ModuleStatusInfo
    {
        uint8_t mod_type{0}; /* UI_MODA / UI_MODB */
        bool status{false};
    };

    /*
     * 键映射 Profile 屏数据：
     *   keymap_labels[i] 对应应用键 i+1，格式 "K{i}:映射"（docs/05 §5.4）。
     */
    struct KeymapProfileInfo
    {
        uint8_t active_profile{0};
        char profile_name[24]{0};
        char profile_icon[8]{0};
        char keymap_labels[11][24]{};
    };

    struct DisplayMessage
    {
        DisplayMessageType type{DisplayMessageType::SettingUpdate};

        char time_text[16]{}; /* TimeUpdate："HH:MM:SS" + '\0' */
        uint8_t action{0};    /* ActionInput：LV_KEY_LEFT/RIGHT/ENTER/ESC */
        uint32_t key_value{0}; /* KeyInput：按键位掩码（RGB 高亮，阶段 06） */
        uint8_t navigate_target{0}; /* Navigate：ui_screen_tag_t */
        bool asr_recording{false};
        uint8_t battery_percent{0}; /* BatteryStatus：0~100 */

        ui_settings_snapshot_t setting{};
        PcStatusInfo pc_status{};
        HaStatusInfo ha_status{};
        MusicPlayerInfo music_player{};
        ModuleStatusInfo module{};
        KeymapProfileInfo keymap_profile{};
    };

    /*
     * DeviceSettings → ui_settings_snapshot_t（SettingUpdate 载荷填充）。
     * rgb_single_colar 为调色板索引（0~255），转成字符串供设置屏比较。
     */
    inline void fillSettingPayload(const DeviceSettings &s,
                                   ui_settings_snapshot_t &out)
    {
        out.work_mode = s.work_mode;
        out.rgb_mode = s.rgb_mode;
        out.rgb_click_mode = s.rgb_click_mode;
        out.rgb_brightness = s.rgb_brightness;
        out.tft_theme = s.tft_theme;
        out.tft_brightness = s.tft_brightness;
        out.device_volume = s.device_volume;
        out.power_mode = s.power_mode;
        out.audio_enable = s.audio_enable != 0;
        out.connect_host = s.connect_host != 0;
        out.voice_enable = s.voice_enable != 0;
        out.active_keymap_profile = s.active_keymap_profile;
        snprintf(out.rgb_single_color, sizeof(out.rgb_single_color),
                 "%u", static_cast<unsigned>(s.rgb_single_colar));
    }

} // namespace ekeys

#endif // EKEYS_MESSAGE_TYPES_H
