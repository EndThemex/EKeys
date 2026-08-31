/*
 * DisplayTask.cpp
 *
 * 阶段 05：SquareLine 生成的 11 屏 UI 接管显示。
 *
 *   - run() 入口调用 ui_init()（所有屏幕一次性创建）；
 *   - 启动时从 Configuration 拉取快照刷新状态条 / 主屏；
 *   - 队列消息分发：SettingUpdate / TimeUpdate / ActionInput /
 *     KeymapProfile / ModuleStatus / AsrRecording / PcStatus /
 *     HaStatus / MusicPlayer / Navigate。
 *
 * 数据源尚未接入的消息（Pc/Ha/Music 等）处理路径已就绪，
 * 阶段 06/07 由协议层与网络模块投递。
 */

#include "DisplayTask.h"

#include <Arduino.h>
#include <stdio.h>

#include "config/Configuration.h"
#include "display/Backlight.h"
#include "display/LvglPort.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include "rgb/RGBLightControl.h"
#include "ui/ui.h"
#include "ui/ui_HaScreenSecondary.h"
#include "ui/ui_KeyMappedSecondary.h"
#include "ui/ui_MainScreen.h"
#include "ui/ui_MusicScreenSecondary.h"
#include "ui/ui_PcStatusScreen.h"
#include "ui/ui_SettingScreenSecondary.h"
#include "ui/ui_StatusBar.h"

namespace ekeys
{

    namespace
    {

        constexpr uint32_t kDisplayTaskStackDepth = 8192;
        constexpr uint8_t kDisplayTaskPriority = 1;
        constexpr UBaseType_t kDisplayMessageQueueLen = 10;
        constexpr TickType_t kDisplayMessageBlockTicks = pdMS_TO_TICKS(50);

        /*
         * DeviceSettings.work_mode（0=USB 1=BLE 2=2.4G）与
         * ui_StatusBar 的 WORKMODE 枚举同序。
         */
        const char *workModeText(uint8_t work_mode)
        {
            switch (work_mode)
            {
            case 1:
                return "BLT MODE";
            case 2:
                return "2.4 MODE";
            default:
                return "WIR MODE";
            }
        }

    } // namespace

    DisplayTask::DisplayTask()
        : queue_(nullptr)
    {
    }

    DisplayTask &DisplayTask::instance()
    {
        static DisplayTask inst;
        return inst;
    }

    void DisplayTask::begin()
    {
        if (queue_ != nullptr)
        {
            return;
        }
        queue_ = xQueueCreate(kDisplayMessageQueueLen, sizeof(DisplayMessage));
        if (queue_ == nullptr)
        {
            LOG_ERROR("DISP", "xQueueCreate failed");
            return;
        }

        BaseType_t ok = xTaskCreatePinnedToCore(
            &DisplayTask::taskEntry,
            "DisplayTask",
            kDisplayTaskStackDepth,
            this,
            kDisplayTaskPriority,
            nullptr,
            0); /* Core 0 */

        if (ok != pdPASS)
        {
            LOG_ERROR("DISP", "xTaskCreatePinnedToCore failed");
        }
        else
        {
            LOG_INFO("DISP", "DisplayTask started on core 0");
        }
    }

    BaseType_t DisplayTask::post(const DisplayMessage &msg, TickType_t wait_ticks)
    {
        if (queue_ == nullptr)
        {
            return pdFALSE;
        }
        return xQueueSend(queue_, &msg, wait_ticks);
    }

    void DisplayTask::navigateTo(ui_screen_tag_t tag)
    {
        DisplayMessage msg;
        msg.type = DisplayMessageType::Navigate;
        msg.navigate_target = static_cast<uint8_t>(tag);
        post(msg, 0);
    }

    void DisplayTask::taskEntry(void *arg)
    {
        auto *self = static_cast<DisplayTask *>(arg);
        self->run();
        vTaskDelete(nullptr);
    }

    void DisplayTask::run()
    {
        uint32_t last = millis();

        /* SquareLine UI：一次性创建 11 屏 */
        ui_init();

        /* 状态条初始状态（当前无 WiFi / 模块 / 电池数据源，先按离线占位） */
        status_bar_set_working_mode(WIRED_KEYBOARD_MODE);
        status_bar_set_recording_state(false);
        status_bar_set_volume(0);
        status_bar_set_battery_level(100);
        status_bar_set_wifi_strength(0);
        status_bar_set_module_status(UI_MODA, false);
        status_bar_set_module_status(UI_MODB, false);

        /* 启动快照：把当前配置刷到主屏 / 设置屏（背光 / 模式 / 音量） */
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            /* RGB 灯效初始化（RGBDriver::begin 在 DisplayTask 上下文统一驱动） */
            RGBLightControl::instance().applySettings(snap);
            DisplayMessage msg;
            msg.type = DisplayMessageType::SettingUpdate;
            fillSettingPayload(snap, msg.setting);
            applySetting(msg);
        }

        DisplayMessage msg;
        for (;;)
        {
            uint32_t now = millis();
            if (now != last)
            {
                const uint32_t delta = now - last;
                LvglPort::instance().tick(delta);
                /* RGB 动画 tick（内部 30ms 帧节流，docs/06 6.15） */
                RGBLightControl::instance().tick(delta);
                last = now;
            }

            if (xQueueReceive(queue_, &msg, kDisplayMessageBlockTicks) == pdTRUE)
            {
                applyMessage(msg);
            }
        }
    }

    void DisplayTask::applyMessage(const DisplayMessage &msg)
    {
        switch (msg.type)
        {
        case DisplayMessageType::TimeUpdate:
        {
            /* "HH:MM:SS" → 主屏 ui_LabelTime（HH:MM）+ ui_LabelSecond（SS） */
            const char *t = msg.time_text;
            if (t[0] != '\0')
            {
                static char hm[7];
                static char ss[3];
                hm[0] = t[0];
                hm[1] = t[1];
                hm[2] = t[3];
                hm[3] = t[4];
                hm[4] = t[6];
                hm[5] = t[7];
                hm[6] = '\0';
                ss[0] = t[6];
                ss[1] = t[7];
                ss[2] = '\0';
                lv_label_set_text(ui_LabelTime, hm);
                lv_label_set_text(ui_LabelSecond, ss);
            }
            break;
        }

        case DisplayMessageType::SettingUpdate:
            applySetting(msg);
            break;

        case DisplayMessageType::ActionInput:
        {
            /* 旋钮 / 设置键 → 当前活动屏的 LV_EVENT_KEY 处理器 */
            lv_obj_t *active_screen = lv_scr_act();
            if (active_screen != nullptr)
            {
                lv_event_send(active_screen, LV_EVENT_KEY,
                              (void *)(uintptr_t)msg.action);
            }
            break;
        }

        case DisplayMessageType::Navigate:
            navigateNow(static_cast<ui_screen_tag_t>(msg.navigate_target));
            break;

        case DisplayMessageType::ModuleStatus:
            status_bar_set_module_status(msg.module.mod_type,
                                         msg.module.status);
            break;

        case DisplayMessageType::AsrRecording:
            status_bar_set_recording_state(msg.asr_recording);
            break;

        case DisplayMessageType::KeymapProfile:
            applyKeymapProfile(msg);
            break;

        case DisplayMessageType::PcStatus:
            applyPcStatus(msg);
            break;

        case DisplayMessageType::HaStatus:
            applyHaStatus(msg);
            break;

        case DisplayMessageType::MusicPlayer:
            applyMusicPlayer(msg);
            break;

        default:
            break;
        }
    }

    void DisplayTask::applySetting(const DisplayMessage &msg)
    {
        const ui_settings_snapshot_t &s = msg.setting;

        /* 背光即时生效 */
        Backlight::instance().setDuty(
            static_cast<uint8_t>(s.tft_brightness));

        /* RGB 灯效即时生效（以配置层权威值为准，DisplayTask 上下文统一驱动） */
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            RGBLightControl::instance().applySettings(snap);
        }

        /* 状态条：工作模式 + 音量 */
        status_bar_set_working_mode(s.work_mode);
        status_bar_set_volume(s.device_volume);

        /* 主屏文字 */
        ui_MainScreen_set_work_mode((char *)workModeText(s.work_mode));
        char light[16] = {0};
        snprintf(light, sizeof(light), "RGB_LIGHT:%d%%", s.rgb_brightness);
        ui_MainScreen_set_rgb_light(light);
        snprintf(light, sizeof(light), "TFT_LIGHT:%d%%", s.tft_brightness);
        ui_MainScreen_set_tft_light(light);

        /* 设置屏反向显示当前快照 */
        ui_SettingScreenSecondary_set_snapshot(&s);
    }

    void DisplayTask::navigateNow(ui_screen_tag_t tag)
    {
        lv_obj_t *target = nullptr;
        switch (tag)
        {
        case UI_SCREEN_MAIN:
            target = ui_MainScreen;
            break;
        case UI_SCREEN_KEYMAPPED:
            target = ui_KeyMapped;
            break;
        case UI_SCREEN_KEYMAPPED_SECONDARY:
            target = ui_KeyMappedSecondary;
            break;
        case UI_SCREEN_MUSIC:
            target = ui_MusicScreen;
            break;
        case UI_SCREEN_MUSIC_SECONDARY:
            target = ui_MusicScreenSecondary;
            break;
        case UI_SCREEN_PC_STATUS:
            target = ui_PcStatusScreen;
            break;
        case UI_SCREEN_PC_STATUS_SECONDARY:
            target = ui_PcStatusScreenSecondary;
            break;
        case UI_SCREEN_HA:
            target = ui_HaScreen;
            break;
        case UI_SCREEN_HA_SECONDARY:
            target = ui_HaScreenSecondary;
            break;
        case UI_SCREEN_SETTING:
            target = ui_SettingScreen;
            break;
        case UI_SCREEN_SETTING_SECONDARY:
            target = ui_SettingScreenSecondary;
            break;
        default:
            return;
        }

        if (target != nullptr && target != lv_scr_act())
        {
            ui_set_active_screen_tag(tag);
            lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
        }
    }

    void DisplayTask::applyKeymapProfile(const DisplayMessage &msg)
    {
        const KeymapProfileInfo &p = msg.keymap_profile;

        /* 阶段 05 无 PNG 图标：统一走内置符号回退 */
        ui_KeyMapped_set_profile_icon_image_data(nullptr, 0, 0, 0,
                                                 p.profile_icon);
        char fileName[32] = {0};
        snprintf(fileName, sizeof(fileName), "config_profile_%u.ini",
                 static_cast<unsigned>(p.active_profile));
        ui_KeyMappedSecondary_set_profile(p.profile_icon, p.profile_name,
                                          fileName);
        ui_KeyMappedSecondary_set_profile_icon_image_data(nullptr, 0, 0, 0,
                                                          p.profile_icon);
        for (uint8_t i = 0; i < 11; ++i)
        {
            ui_KeyMappedSecondary_set_key_label(i, p.keymap_labels[i]);
        }
        if (ui_get_active_screen_tag() == UI_SCREEN_KEYMAPPED ||
            ui_get_active_screen_tag() == UI_SCREEN_KEYMAPPED_SECONDARY)
        {
            lv_refr_now(NULL);
        }
    }

    void DisplayTask::applyPcStatus(const DisplayMessage &msg)
    {
        const PcStatusInfo &pc = msg.pc_status;

        char locks[48];
        char network[48];
        char up[48];
        char down[48];
        char cpu[48];
        char cpuTemp[48];
        char mem[48];
        char diskIo[48];

        snprintf(locks, sizeof(locks), "Locks: C%s N%s S%s",
                 pc.caps_lock ? "ON" : "--",
                 pc.num_lock ? "ON" : "--",
                 pc.scroll_lock ? "ON" : "--");
        snprintf(network, sizeof(network), "Network: %s",
                 pc.network_connected ? "ONLINE" : "OFFLINE");
        if (pc.network_up_kbps >= 0.0f)
        {
            snprintf(up, sizeof(up), "Net Up: %.1f Kbps", pc.network_up_kbps);
        }
        else
        {
            snprintf(up, sizeof(up), "Net Up: -- Kbps");
        }
        if (pc.network_down_kbps >= 0.0f)
        {
            snprintf(down, sizeof(down), "Net Down: %.1f Kbps",
                     pc.network_down_kbps);
        }
        else
        {
            snprintf(down, sizeof(down), "Net Down: -- Kbps");
        }
        if (pc.cpu_usage_percent >= 0.0f)
        {
            snprintf(cpu, sizeof(cpu), "CPU: %.1f%%", pc.cpu_usage_percent);
        }
        else
        {
            snprintf(cpu, sizeof(cpu), "CPU: --");
        }
        if (pc.memory_usage_percent >= 0.0f)
        {
            snprintf(mem, sizeof(mem), "MEM: %.1f%%",
                     pc.memory_usage_percent);
        }
        else
        {
            snprintf(mem, sizeof(mem), "MEM: --");
        }
        if (pc.cpu_temp_c >= 0.0f)
        {
            snprintf(cpuTemp, sizeof(cpuTemp), "CPU Temp: %.1fC", pc.cpu_temp_c);
        }
        else
        {
            snprintf(cpuTemp, sizeof(cpuTemp), "CPU Temp: N/A");
        }
        if (pc.disk_io_percent >= 0.0f)
        {
            snprintf(diskIo, sizeof(diskIo), "Disk IO: %.1f%%",
                     pc.disk_io_percent);
        }
        else
        {
            snprintf(diskIo, sizeof(diskIo), "Disk IO: --");
        }

        ui_PcStatusScreen_set_host_status(locks);
        ui_PcStatusScreen_set_time_status(network);
        ui_PcStatusScreen_set_lock_status(up);
        ui_PcStatusScreen_set_network_status(down);
        ui_PcStatusScreen_set_power_status(cpu);
        ui_PcStatusScreen_set_cpu_temp_status(cpuTemp);
        ui_PcStatusScreen_set_perf_status(mem);
        ui_PcStatusScreen_set_temp_status(diskIo);
    }

    void DisplayTask::applyHaStatus(const DisplayMessage &msg)
    {
        const HaStatusInfo &ha = msg.ha_status;

        ui_MainScreen_set_host_connection(ha.tcp_connected);
        ui_HaScreenSecondary_set_wifi_status(ha.wifi_enabled,
                                             ha.wifi_connected,
                                             ha.wifi_rssi,
                                             ha.ip_address);
        ui_HaScreenSecondary_set_tcp_status(ha.tcp_connected,
                                            ha.server_endpoint);
        ui_HaScreenSecondary_set_mode_status(ha.work_mode);
        ui_HaScreenSecondary_set_voice_status(ha.voice_enabled,
                                              ha.voice_recording);
        ui_HaScreenSecondary_set_module_status(ha.module_a_connected,
                                               ha.module_b_connected);
    }

    void DisplayTask::applyMusicPlayer(const DisplayMessage &msg)
    {
        const MusicPlayerInfo &m = msg.music_player;
        ui_MusicScreenSecondary_set_player_state(
            m.title, m.artist, m.player_name,
            m.lyric_current, m.lyric_next,
            m.connected, m.is_playing, m.is_paused,
            m.can_prev, m.can_next,
            m.current_seconds, m.total_seconds);
    }

} // namespace ekeys
