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
#include <SPIFFS.h>
#include <stdio.h>
#include <stdlib.h>

#include "config/Configuration.h"
#include "audio/AudioAnalyzer.h"
#include "audio/AudioPad.h"
#include "audio/Mic.h"
#include "display/Backlight.h"
#include "display/LvglPort.h"
#include "input/MatrixScanner.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include "rgb/ClickHighlight.h"
#include "rgb/RGBLightControl.h"
#include "ui/ui.h"
#include "ui/ui_AudioScreen.h"
#include "ui/ui_HaScreenSecondary.h"
#include "ui/ui_KeyMapped.h"
#include "ui/ui_KeyMappedSecondary.h"
#include "ui/ui_MainScreen.h"
#include "ui/ui_MusicScreen.h"
#include "ui/ui_MusicScreenSecondary.h"
#include "ui/ui_PcStatusScreen.h"
#include "ui/ui_SettingScreenSecondary.h"
#include "ui/ui_StatusBar.h"
#include "voice/VoiceRecognizer.h"

/*
 * A6 修复（占位）：Profile PNG 图标显示。
 *
 * 原计划：直接调用 lodepng_decode32() 解码 SPIFFS 上的 PNG。
 * 实现中发现 LV_USE_PNG=0 时 LVGL 整个 lv_png.c 被预处理空，LDF 不会链入
 * lodepng.c.o，导致 undefined reference to lodepng_decode32。
 * 简单可行的两个方案都会扩大变更面：
 *   1. 在 lv_conf.h 开 LV_USE_PNG=1（让 lv_png.c 真正起作用，链入 lodepng）
 *   2. 把 lodepng.c 复制到 src/util/ 并加入 build_src_filter
 * 两者都需要先做实测权衡，这里先保持符号回退路径，
 * UI 入口 ui_*_set_profile_icon_image_data() 已经实现真接通，
 * 后续接 PNG 解码只需在 applyKeymapProfile 里把 nullptr 换成解码产物。
 */

namespace ekeys
{

    namespace
    {

        constexpr uint32_t kDisplayTaskStackDepth = 8192;
        constexpr uint8_t kDisplayTaskPriority = 1;
        constexpr UBaseType_t kDisplayMessageQueueLen = 10;
        /* ≤ 期望帧周期 16ms：队列空闲时保证 LVGL tick 节奏（docs/10 §3.7） */
        constexpr TickType_t kDisplayMessageBlockTicks = pdMS_TO_TICKS(10);

        /* 一级页无操作 5s 后自动返回主页 */
        constexpr TickType_t kAutoReturnToMainTicks = pdMS_TO_TICKS(5000);
        /* 频谱 Mic::begin 失败重试退避：避免每 ms tick 重刷 install 报错 */
        constexpr uint32_t kSpectrumRetryMs = 1000;

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

        /* 池水位快照（一次性）：11 屏全建完后 LVGL 池的用量与碎片率，
         * 用于发现"加屏耗尽 LV_MEM_SIZE"类问题（2026-09-11 启动崩溃根因） */
        {
            lv_mem_monitor_t mon;
            lv_mem_monitor(&mon);
            LOG_INFO("DISP", "LVGL pool: used %u/%u, frag %u%%",
                     (unsigned)(mon.total_size - mon.free_size),
                     (unsigned)mon.total_size, (unsigned)mon.frag_pct);
        }

        /*
         * C7 修复：移除 status_bar_set_working_mode(WIRED_KEYBOARD_MODE) 硬编码。
         * 紧随其后的"启动快照"块会从 Configuration 读出真实 work_mode
         * 并通过 applySetting → status_bar_set_working_mode() 覆盖，
         * 这里写死反而会在首帧渲染前多一次冗余赋值。
         */
        status_bar_set_recording_state(false);
        status_bar_set_volume(0);
        /* 电量由 MainTask 5s 节流后投递 BatteryStatus 更新，避免此处硬编码 100 */
        status_bar_set_wifi_status(false, false, -100);
        status_bar_set_module_status(UI_MODA, false);
        status_bar_set_module_status(UI_MODB, false);

        /* 启动快照：把当前配置刷到主屏 / 设置屏（背光 / 模式 / 音量） */
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            /* RGB 灯效初始化（RGBDriver::begin 在 DisplayTask 上下文统一驱动） */
            RGBLightControl::instance().applySettings(snap);
            ClickHighlight::applySettings(snap);
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
                /* 频谱调度（阶段 07 7.2）：仅音乐屏可见时占用 Mic/CPU */
                updateSpectrum();
                last = now;
            }

            if (xQueueReceive(queue_, &msg, kDisplayMessageBlockTicks) == pdTRUE)
            {
                applyMessage(msg);
            }

            /* 一级页 5s 无操作 → 自动回主页（run() 主循环每帧检查） */
            checkAutoReturn();
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
            /*
             * C8 修复：长度 ≥ 8 才解析，避免上游格式变化（如 "H:MM:SS"）
             * 导致 t[7] 越界。冒号位（t[2] / t[5]）也校验为 ':'，双重保险。
             */
            if (strlen(t) >= 8 && t[2] == ':' && t[5] == ':')
            {
                static char hm[6];
                static char ss[3];
                hm[0] = t[0];
                hm[1] = t[1];
                hm[2] = ':';
                hm[3] = t[3];
                hm[4] = t[4];
                hm[5] = '\0';
                ss[0] = t[6];
                ss[1] = t[7];
                ss[2] = '\0';
                lv_label_set_text(ui_LabelTime, hm);
                lv_label_set_text(ui_LabelSecond, ss);
            }

            /*
             * 日期 + 星期：MainTask 已按 ui_lang 生成最终显示文本
             * （中文 "09月08日"/"星期一"，英文 "SEP 08"/"MON"），
             * 字段为空（未同步）时跳过，避免覆盖初始占位。
             */
            if (msg.date_text[0] != '\0')
            {
                lv_label_set_text(ui_LabelData, msg.date_text);
            }
            if (msg.week_text[0] != '\0')
            {
                lv_label_set_text(ui_LabelWeek, msg.week_text);
            }
            break;
        }

        case DisplayMessageType::SettingUpdate:
            applySetting(msg);
            break;

        case DisplayMessageType::ActionInput:
        {
            /*
             * 旋钮 / 矩阵键都是用户主动操作，重置 5s 自动回主页计时。
             */
            bumpActivity();
            /*
             * 矩阵键动作编码 = kMatrixKeyActionBase + key_id（MainTask 侧编码，
             * 与 LV_KEY_* 数值不重叠；原裸传 key_id 时 key_id=10 与
             * LV_KEY_ENTER=10 冲突，导致按键 10 在主页被当成旋钮单击触发导航）。
             * 路由：
             *   - KEYMAPPED 屏：截胡 → 跳 KEYMAPPED_SECONDARY 并聚焦该键；
             *   - KEYMAPPED_SECONDARY / SETTING_SECONDARY：编码后的 action
             *     （BASE + key_id = 101~111）透传进 UI，与 LV_KEY_* 不重叠
             *     （2026-09-11 修复：原裸传 key_id 时矩阵键 10 与旋钮单击冲突）；
             *   - AUDIO 屏：截胡 → 触发音效板播放并直接同线程亮键位高亮
             *     （不另发消息；播完由 AudioPad::loop 经 AudioPad 消息清除）；
             *   - 其它屏：矩阵键为 HID 专用，不触发 UI 导航。
             */
        const uint8_t action = msg.action;
        if (action > kMatrixKeyActionBase &&
            action <= kMatrixKeyActionBase + kMatrixKeyCount)
        {
            const uint8_t key_id =
                static_cast<uint8_t>(action - kMatrixKeyActionBase);
            const ui_screen_tag_t tag = ui_get_active_screen_tag();
            if (tag == UI_SCREEN_KEYMAPPED)
            {
                ui_KeyMappedSecondary_set_focus(key_id);
                navigateNow(UI_SCREEN_KEYMAPPED_SECONDARY);
            }
            else if (tag == UI_SCREEN_KEYMAPPED_SECONDARY ||
                     tag == UI_SCREEN_SETTING_SECONDARY)
            {
                lv_obj_t *active_screen = lv_scr_act();
                if (active_screen != nullptr)
                {
                    lv_event_send(active_screen, LV_EVENT_KEY,
                                  (void *)(uintptr_t)action);
                }
            }
            else if (tag == UI_SCREEN_AUDIO)
            {
                if (AudioPad::instance().trigger(key_id))
                {
                    ui_AudioScreen_set_playing(key_id);
                }
            }
            break;
        }
            /*
             * SettingScreenSecondary 旋钮行为：旋转直接发出 LV_KEY_LEFT/RIGHT
             * 进 UI，由 setting_secondary_handle_key 走调值分支
             * （adjust_value ±1）。SettingScreenSecondary 上的
             * LV_KEY_UP/DOWN 分支保留但当前无物理输入触发。
             */
            lv_obj_t *active_screen = lv_scr_act();
            if (active_screen != nullptr)
            {
                lv_event_send(active_screen, LV_EVENT_KEY,
                              (void *)(uintptr_t)action);
            }
            break;
        }

        case DisplayMessageType::Navigate:
            /* 主动导航也是用户操作；navigateNow 内部会按目标屏决定是否禁用计时 */
            navigateNow(static_cast<ui_screen_tag_t>(msg.navigate_target));
            bumpActivity();
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

        case DisplayMessageType::BatteryStatus:
            status_bar_set_battery_level(msg.battery_percent);
            break;

        case DisplayMessageType::AudioPad:
            applyAudioPad(msg);
            break;

        default:
            break;
        }
    }

    void DisplayTask::applyAudioPad(const DisplayMessage &msg)
    {
        /* 绑定变更 / 播完清高亮统一走这里（AudioPad 模块投递） */
        ui_AudioScreen_set_pads(msg.audio_pad.files);
        ui_AudioScreen_set_playing(msg.audio_pad.playing_key);
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
            ClickHighlight::applySettings(snap);
        }

        /* 状态条：工作模式 + 音量 */
        status_bar_set_working_mode(s.work_mode);
        status_bar_set_volume(s.device_volume);

        /* 主屏文字 */
        ui_MainScreen_set_work_mode((char *)workModeText(s.work_mode));
        ui_MainScreen_set_rgb_light(s.rgb_brightness);
        ui_MainScreen_set_tft_light(s.tft_brightness);

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
        case UI_SCREEN_AUDIO:
            target = ui_AudioScreen;
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
            /*
             * 进入 MAIN 或任何二级页：禁用 5s 自动回主页计时。
             * 进入一级页（非 MAIN）：清零，由后续 bumpActivity() 触发开始计时。
             * 无论切到哪都先清零：navigateNow 触发可能是由 ActionInput /
             * Navigate（已经 bumpActivity）或 checkAutoReturn 自动回主页，
             * 后者必须清零避免触发链式回主页。
             */
            last_activity_tick_ = 0;
        }
    }

    void DisplayTask::applyKeymapProfile(const DisplayMessage &msg)
    {
        const KeymapProfileInfo &p = msg.keymap_profile;

        /* 一律用 profile_index（applied 消息中它恒等于 active_profile） */
        char fileName[32] = {0};
        snprintf(fileName, sizeof(fileName), "config_profile_%u.ini",
                 static_cast<unsigned>(p.profile_index));

        if (p.is_preview)
        {
            /*
             * 预览消息：只刷新二级页展示，不应用不落盘。
             * - update_main_summary=false：预览不得污染主屏 summary（bind 缓存）
             * - 跳过 SPIFFS.exists 图标检查与两个 set_profile_icon_image_data：
             *   最贵的 SPIFFS 探测在预览高频路径上没必要（当前 icon 恒为
             *   符号回退，无视觉差异），且避免一级屏图标被预览污染
             */
            ui_KeyMappedSecondary_set_profile(p.profile_icon, p.profile_name,
                                              fileName,
                                              /*update_main_summary=*/false);
            {
                DeviceSettings snap;
                Configuration::instance().snapshot(snap);
                ui_KeyMappedSecondary_set_fun_keys(snap.fun_key1, snap.fun_key2);
            }
            for (uint8_t i = 0; i < 11; ++i)
            {
                ui_KeyMappedSecondary_set_key_label(i, p.keymap_labels[i]);
            }
            if (ui_get_active_screen_tag() == UI_SCREEN_KEYMAPPED_SECONDARY)
            {
                lv_refr_now(NULL);
            }
            return;
        }

        /* 已应用消息：先收尾等待遮罩 + 更新已应用标记 */
        ui_KeyMappedSecondary_hide_apply_waiting();
        ui_KeyMappedSecondary_set_applied_index(
            static_cast<unsigned>(p.profile_index) + 1u);

        ui_KeyMappedSecondary_set_profile(p.profile_icon, p.profile_name,
                                          fileName,
                                          /*update_main_summary=*/true);

        /*
         * FUN 键整体配色标识：把 fun_key1 / fun_key2 同步给二级页，
         * 对应键位 cell 底色 + 边框按 FUN1/FUN2 配色渲染。
         */
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            ui_KeyMappedSecondary_set_fun_keys(snap.fun_key1, snap.fun_key2);
        }

        /*
         * A6 修复：Profile 图标显示。
         *
         * 设计：ui_*_set_profile_icon_image_data() 接口已实现真接通（malloc
         * 拷贝 + 构造 lv_img_dsc_t + set src），只是缺少解码 PNG 的中间层。
         * 当前实现简化：仅检查 SPIFFS 上是否存在 PNG 文件，存在时用 file 路径
         * 记录，未来接入 PNG 解码器（lodepng / PNGdec）只需替换以下
         * readPngToRgba() 占位即可。两个 UI 入口保留 nullptr=回退符号 的语义。
         *
         * TODO：把 readPngToRgba() 接上 lodepng（当前 LV_USE_PNG=0 让 LVGL
         * 不链入 lodepng.c.o，链接失败；可考虑 LV_USE_PNG=1 或显式 src/util）。
         */
        const char *icon_path = Configuration::instance().getProfileIconPath(
            p.profile_index);
        const bool has_icon = SPIFFS.exists(icon_path);
        if (!has_icon)
        {
            ui_KeyMapped_set_profile_icon_image_data(nullptr, 0, 0, 0,
                                                     p.profile_icon);
            ui_KeyMappedSecondary_set_profile_icon_image_data(nullptr, 0, 0, 0,
                                                              p.profile_icon);
        }
        else
        {
            /* 占位：图标存在但解码管线未接 → 走符号回退。 */
            ui_KeyMapped_set_profile_icon_image_data(nullptr, 0, 0, 0,
                                                     p.profile_icon);
            ui_KeyMappedSecondary_set_profile_icon_image_data(nullptr, 0, 0, 0,
                                                              p.profile_icon);
            (void)icon_path;
        }

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

        ui_PcStatusScreen_set_network(pc.network_connected);
        ui_PcStatusScreen_set_net_up_kbps(pc.network_up_kbps);
        ui_PcStatusScreen_set_net_down_kbps(pc.network_down_kbps);
        ui_PcStatusScreen_set_cpu_percent(pc.cpu_usage_percent);
        ui_PcStatusScreen_set_cpu_temp_c(pc.cpu_temp_c);
        ui_PcStatusScreen_set_mem_percent(pc.memory_usage_percent);
        ui_PcStatusScreen_set_disk_io_percent(pc.disk_io_percent);
    }

    void DisplayTask::applyHaStatus(const DisplayMessage &msg)
    {
        const HaStatusInfo &ha = msg.ha_status;

        ui_MainScreen_set_host_connection(ha.tcp_connected);
        /* 状态栏 WiFi 图标：与 HA 二级页共用 wifi_enabled/connected/rssi 字段 */
        status_bar_set_wifi_status(ha.wifi_enabled,
                                   ha.wifi_connected,
                                   ha.wifi_rssi);
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

    void DisplayTask::updateSpectrum()
    {
        const ui_screen_tag_t tag = ui_get_active_screen_tag();
        const bool visible = (tag == UI_SCREEN_MUSIC ||
                              tag == UI_SCREEN_MUSIC_SECONDARY);

        if (!visible)
        {
            if (spectrum_active_)
            {
                spectrum_active_ = false;
                Mic::instance().end();
                VoiceRecognizer::instance().resume();
                LOG_INFO("DISP", "spectrum stopped");
            }
            return;
        }

        if (!spectrum_active_)
        {
            /* 正在录音（ASR）时推迟接管，避免双读 I2S */
            if (VoiceRecognizer::instance().isCapturing())
            {
                return;
            }
            /* 失败退避窗口内不重试（毫秒回绕安全比较） */
            if ((int32_t)(millis() - spectrum_retry_after_ms_) < 0)
            {
                return;
            }
            /* 挂起语音识别（docs/06：音乐屏 suspend / 离开 resume） */
            VoiceRecognizer::instance().suspend();
            /* BCLK=IO10 与 Speaker 互斥（PINOUT §2.7）：频谱接管 Mic 前停掉 Speaker */
            VoiceRecognizer::prepareI2sForMicCapture();
            if (!Mic::instance().begin())
            {
                spectrum_retry_after_ms_ = millis() + kSpectrumRetryMs;
                return; // 退避后重试
            }
            spectrum_retry_after_ms_ = 0;
            AudioAnalyzer::instance().begin();
            spectrum_active_ = true;
            LOG_INFO("DISP", "spectrum started");
        }

        /* 读一帧 PCM（512 样本 @16kHz = 32ms，天然节流）并做 FFT */
        static int16_t chunk[AudioAnalyzer::kFftSize];
        static float bands[AudioAnalyzer::kBandCount];
        const size_t n = Mic::instance().Read(chunk, AudioAnalyzer::kFftSize);
        if (n == 0)
        {
            return;
        }
        AudioAnalyzer::instance().process(chunk, n, bands, AudioAnalyzer::kBandCount);

        /* 0~1 → 0~255（ui_MusicScreen_drawAudioBandsCool 期望刻度） */
        float draw_bands[AudioAnalyzer::kBandCount];
        for (size_t i = 0; i < AudioAnalyzer::kBandCount; ++i)
        {
            draw_bands[i] = bands[i] * 255.0f;
        }
        ui_MusicScreen_drawAudioBandsCool(draw_bands);
    }

    /*
     * 是否处于二级页（详情页）。二级页不参与自动回主页：
     *   - 详情页往往需要较长时间停留（看歌词 / 看 PC 状态）
     *   - SETTING_SECONDARY 本身有 1s apply debounce（setting_secondary_apply_timer_cb）
     *   - 离开二级页回到一级页时，navigateNow() 已把 last_activity_tick_ 清零，
     *     下次 ActionInput/Navigate 进入一级页才会重新开始 5s 计时
     */
    bool DisplayTask::isSecondaryScreen(ui_screen_tag_t tag) const
    {
        switch (tag)
        {
        case UI_SCREEN_KEYMAPPED_SECONDARY:
        case UI_SCREEN_MUSIC_SECONDARY:
        case UI_SCREEN_PC_STATUS_SECONDARY:
        case UI_SCREEN_HA_SECONDARY:
        case UI_SCREEN_SETTING_SECONDARY:
            return true;
        default:
            return false;
        }
    }

    bool DisplayTask::isAutoReturnEnabled() const
    {
        const ui_screen_tag_t tag = ui_get_active_screen_tag();
        /* 主页不需要自动回；二级页禁用；UNKNOWN 视为禁用（启动首帧） */
        if (tag == UI_SCREEN_MAIN || tag == UI_SCREEN_UNKNOWN)
        {
            return false;
        }
        if (isSecondaryScreen(tag))
        {
            return false;
        }
        return true;
    }

    void DisplayTask::bumpActivity()
    {
        /* 0 表示尚未开始；用户操作 → 设当前 tick 作为基准 */
        last_activity_tick_ = xTaskGetTickCount();
    }

    void DisplayTask::checkAutoReturn()
    {
        if (!isAutoReturnEnabled())
        {
            /* 进入主页 / 二级页 / 启动首帧 → 清零计时器，等待下次进入一级页 */
            last_activity_tick_ = 0;
            return;
        }
        if (last_activity_tick_ == 0)
        {
            /* 一级页但尚未开始计时（navigateNow 切到一级页后还没收到任何输入）
             * → 不触发回主页 */
            return;
        }
        const TickType_t now = xTaskGetTickCount();
        if ((now - last_activity_tick_) >= kAutoReturnToMainTicks)
        {
            LOG_INFO("DISP", "auto return to MAIN after %lu ms idle",
                     (unsigned long)pdTICKS_TO_MS(now - last_activity_tick_));
            navigateNow(UI_SCREEN_MAIN);
            /* navigateNow 内部会把 last_activity_tick_ 清零 */
        }
    }

} // namespace ekeys
