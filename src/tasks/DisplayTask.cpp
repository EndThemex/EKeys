/*
 * DisplayTask.cpp
 *
 * 阶段 05：SquareLine 生成的 11 屏 UI 接管显示。
 *
 *   - run() 入口调用 ui_init()（所有屏幕一次性创建）；
 *   - 启动时从 Configuration 拉取快照刷新状态条 / 主屏；
 *   - 队列消息分发：SettingUpdate / TimeUpdate / ActionInput /
 *     KeymapProfile / AsrRecording / PcStatus /
 *     HaStatus / MusicPlayer / Navigate。
 *
 * 数据源尚未接入的消息（Pc/Ha/Music 等）处理路径已就绪，
 * 阶段 06/07 由协议层与网络模块投递。
 */

#include "DisplayTask.h"

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* 频谱滚动缓冲 memmove/memcpy */

#include "config/Configuration.h"
#include "config/PowerConfig.h"
#include "ai/AiFramePool.h"
#include "ai/AiMode.h"
#include "audio/AudioAnalyzer.h"
#include "audio/AudioPad.h"
#include "audio/Mic.h"
#include "display/Backlight.h"
#include "display/LvglPort.h"
#include "game/TetrisGame.h"
#include "input/MatrixScanner.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include "rgb/ClickHighlight.h"
#include "rgb/RGBLightControl.h"
#include "protocol/SerialProtocol.h"
#include "ui/ui_types.h"
#include "ui/ui_AiScreen.h"
#include "ui/ui_AiScreenSecondary.h"
#include "ui/ui_AudioScreen.h"
#include "ui/ui_GameScreen.h"
#include "ui/ui_GameSelectScreen.h"
#include "ui/ui_AudioScreenSecondary.h"
#include "ui/ui_FlipClock.h"
#include "ui/ui_HaScreen.h"
#include "ui/ui_HaScreenSecondary.h"
#include "ui/ui_KeyMapped.h"
#include "ui/ui_lang.h"
#include "ui/ui_KeyMappedSecondary.h"
#include "ui/ui_MainScreen.h"
#include "ui/ui_MusicScreen.h"
#include "ui/ui_MusicScreenSecondary.h"
#include "ui/ui_PcStatusScreen.h"
#include "ui/ui_PcStatusScreenSecondary.h"
#include "ui/ui_SettingScreen.h"
#include "ui/ui_SettingScreenSecondary.h"
#include "ui/ui_StatusBar.h"
#include "utils/ProfileIconImage.h"
#include "voice/VoiceRecognizer.h"

/*
 * Profile PNG 图标显示（utils/ProfileIconImage）：
 * LV_USE_PNG=1 后 lodepng 链入，applyKeymapProfile 在 applied 推送时
 * 读 SPIFFS + 解码 + 转 16bpp TRUE_COLOR_ALPHA 后分发给 UI 层。
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
         * DeviceSettings.work_mode（0=USB 1=BLE）与
         * ui_StatusBar 的 WORKMODE 枚举同序（状态栏图标显示，主页不再有模式文字）。
         */

        /*
         * AI 页内置渐变测试图（docs/13 验收 P0）：进 AI 页且无流时，
         * 用 FramePool 槽 0 画一张 RGB 渐变整帧并直接 adopt 上屏，
         * 验证 PSRAM 帧槽 → lv_img 的渲染链路。
         */
        void fillAiTestPattern(uint8_t *px)
        {
            auto *rows = reinterpret_cast<uint16_t *>(px);
            for (uint16_t y = 0; y < 142; ++y)
            {
                uint16_t *row = rows + static_cast<uint32_t>(y) * 428;
                for (uint16_t x = 0; x < 428; ++x)
                {
                    const uint8_t r = static_cast<uint8_t>(x * 255 / 427);
                    const uint8_t g = static_cast<uint8_t>(y * 255 / 141);
                    const uint8_t b = 96 + static_cast<uint8_t>((x + y) % 64);
                    row[x] = static_cast<uint16_t>(((r & 0xF8) << 8) |
                                                   ((g & 0xFC) << 3) | (b >> 3));
                }
            }
        }

        void showAiTestPattern()
        {
            ai::AiFramePool &pool = ai::AiFramePool::instance();
            if (!pool.ensureAllocated())
            {
                ui_AiScreenSecondary_set_state(UI_AI_STATE_ERROR, "PSRAM ALLOC FAILED");
                return;
            }
            const int slot = pool.acquireWriteSlot();
            if (slot < 0)
            {
                return;
            }
            fillAiTestPattern(pool.px(slot));
            pool.adoptDisplaying(slot);
            ui_AiScreenSecondary_show_frame(pool.dsc(slot));
            /* 初始状态：WiFi 就绪 → WAITING；无 WiFi → 串口模式（USB CDC 兜底） */
            ui_AiScreenSecondary_set_state(
                ai::AiMode::instance().netReady() ? UI_AI_STATE_WAITING
                                                  : UI_AI_STATE_SERIAL,
                nullptr);
            LOG_INFO("AI", "test pattern shown (slot %d)", slot);
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
        /* 电源模式空闲计时基准：开机即开始计时，避免首帧被误判为长时间空闲 */
        last_input_tick_ = xTaskGetTickCount();

        /* SquareLine UI：一次性创建 13 屏（ui_init 全部常驻，切屏不销毁） */
        uint32_t t_ui0 = millis();
        ui_init();
        /*
         * 主屏已接管（ui_init 末尾 lv_disp_load_scr(ui_MainScreen)）：
         * 销毁 main.cpp 阶段渲染的开机画面，回收 LVGL 池内存。
         */
        LvglPort::instance().clearSplash();
        LOG_INFO("DISP", "ui_init %lu ms, ready at %lu ms uptime",
                 (unsigned long)(millis() - t_ui0), (unsigned long)millis());

        /*
         * LVGL 池水位监控（每 30s，首轮立即打）：13 屏对象全部常驻于
         * lv_conf.h LV_MEM_SIZE 的 PSRAM 池，2026-09-11（48KB 启动崩）与
         * 2026-09-12（64KB，音乐二级页渲染瞬时分配 OOM → LV_ASSERT_MALLOC
         * while(1) → IDLE0 饿死 WDT abort）两次耗尽后保留水位日志，
         * 用于在下次爆池前发现增长苗头。
         */
        {
            static uint32_t s_last_pool_log_ms = 0;
            if ((uint32_t)(millis() - s_last_pool_log_ms) >= 30000)
            {
                s_last_pool_log_ms = millis();
                lv_mem_monitor_t mon;
                lv_mem_monitor(&mon);
                LOG_INFO("DISP", "LVGL pool: used %u/%u, frag %u%%",
                         (unsigned)(mon.total_size - mon.free_size),
                         (unsigned)mon.total_size, (unsigned)mon.frag_pct);
            }
        }

        /*
         * C7 修复：移除工作模式硬编码（现由 applySetting →
         * status_bar_set_working_mode() 覆盖）。
         * 紧随其后的"启动快照"块会从 Configuration 读出真实 work_mode，
         * 这里写死反而会在首帧渲染前多一次冗余赋值。
         */
        status_bar_set_recording_state(false);
        status_bar_set_volume(0);
        /* 电量由 MainTask 5s 节流后投递 BatteryStatus 更新，避免此处硬编码 100 */
        status_bar_set_wifi_status(false, false, -100);

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
                /*
                 * 俄罗斯方块重力驱动（docs/14 §7）：仅游戏页激活时推进，
                 * delta 与 LVGL tick 共用同一时基。
                 */
                if (tetrisGame_isActive())
                {
                    tetrisGame_tick(static_cast<uint16_t>(delta > 65535 ? 65535 : delta));
                }
                last = now;
            }

            /*
             * AI 页激活钩子（docs/13 §7.2）：run() 每帧 tag 比对（比
             * navigateNow 里埋点更稳——旋钮/矩阵键/触屏按钮/协议导航
             * 全都覆盖）。只有进入二级功能页才启停 AiMode 并渲染测试图；
             * 一级入口页仅显示图标标题，不开启串口/WiFi 握手。
             */
            {
                const ui_screen_tag_t cur_tag = ui_get_active_screen_tag();
                if (cur_tag != last_screen_tag_)
                {
                    last_screen_tag_ = cur_tag;
                    ai::AiMode::instance().setScreenActive(cur_tag == UI_SCREEN_AI_SECONDARY);
                    if (cur_tag == UI_SCREEN_AI_SECONDARY)
                    {
                        showAiTestPattern();
                    }
                }
            }

            if (xQueueReceive(queue_, &msg, kDisplayMessageBlockTicks) == pdTRUE)
            {
                applyMessage(msg);
            }

            /* 一级页 5s 无操作 → 自动回主页（run() 主循环每帧检查） */
            checkAutoReturn();

            /* 电源模式：长时间无操作 → 降亮度（模式 1）/ 关背光（模式 2） */
            checkPowerIdle();

            /*
             * 音乐控制链路（2026-09-23 接通）：音乐二级页旋钮/触屏按钮置位的
             * PREV/TOGGLE/NEXT 请求在此消费，以 0x0f 主动推送转给桌面 App
             * （App 侧转系统媒体键）。请求仅在 MUSIC_SECONDARY 屏按键时置位，
             * 每轮无条件消费即可，最多发一帧。
             */
            const uint8_t music_req = ui_MusicScreenSecondary_consume_control_request();
            if (music_req != UI_MUSIC_CONTROL_NONE)
            {
                const char *music_action = (music_req == UI_MUSIC_CONTROL_PREV)   ? "prev"
                                           : (music_req == UI_MUSIC_CONTROL_NEXT) ? "next"
                                                                                  : "toggle";
                SerialProtocol::instance().sendMusicControl(music_action);
            }

            /*
             * 每轮无条件让出一个 tick：重渲染屏（切屏 init + lv_refr_now 同步整帧、
             * 频谱每帧重绘）配合同步 SPI flush 会长时间占用 CPU0，唯一的让出点
             * xQueueReceive 在消息持续到达时立即返回 → IDLE0 被 5s 饿死，
             * task_wdt abort（2026-09-12 进入音乐二级页崩溃）。
             */
            vTaskDelay(1);
        }
    }

    void DisplayTask::applyMessage(const DisplayMessage &msg)
    {
        switch (msg.type)
        {
        case DisplayMessageType::TimeUpdate:
        {
            /* "HH:MM:SS" → 主屏翻页时钟（HH:MM 大卡片 + SS 小卡片 + 冒号闪烁） */
            ui_FlipClock_update(msg.time_text);

            /* PC 状态二级页 TIME 槽位复用同一本地时间源（二级屏未创建时 setter 内部跳过） */
            ui_PcStatusScreen_set_time(msg.time_text);

            /*
             * 日期 + 星期：MainTask 已按 ui_lang 生成最终显示文本
             * （中文 "09月08日"/"星期一"，英文 "SEP 08"/"MON"），
             * 统一走 ui_MainScreen_set_date_week：空字段跳过，
             * 且星期相对日期的右对齐随文本宽度变化重算。
             */
            ui_MainScreen_set_date_week(msg.date_text, msg.week_text);
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
            bumpInputActivity();
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

            /*
             * 俄罗斯方块竖屏页（docs/14 §5.3）：旋钮与矩阵键全部直送游戏门面，
             * 不走通用 lv_event_send 导航（避免 ESC 被当成切屏、ENTER 误触）。
             * GAME_SELECT 二级页的旋钮仍走下方通用路由（由页面自身处理）。
             */
            if (ui_get_active_screen_tag() == UI_SCREEN_TETRIS)
            {
                tetrisGame_handleInput(action);
                break;
            }

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
                else if (tag == UI_SCREEN_AUDIO_SECONDARY)
                {
                    /* 只置播放请求（MainTask service 消费执行，Audio 实例跨核
                     * 串行保护），绑定存在即亮键位高亮；播放被拒时 service
                     * 重投 AudioPad 消息纠正 */
                    if (AudioPad::instance().requestTrigger(key_id))
                    {
                        ui_AudioScreenSecondary_set_playing(key_id);
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
            bumpInputActivity();
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

        case DisplayMessageType::AiFrame:
        {
            /* AI 帧上屏（docs/13 §7.2）：只换 lv_img 数据指针（AiFramePool 槽），
             * Queued → Displaying 并释放上一个 Displaying 槽 */
            ai::AiFramePool::instance().presentSlot(msg.ai_slot);
            ui_AiScreenSecondary_show_frame(ai::AiFramePool::instance().dsc(msg.ai_slot));
            break;
        }

        case DisplayMessageType::AiStatus:
        {
            /* HUD 统计 2Hz（AiTask 投递）：顺带覆盖等待/流式状态文案 */
            const AiStatusInfo &st = msg.ai_status;
            ui_AiScreenSecondary_set_stats(st.fps_x10, st.bytes_per_s, st.format,
                                           st.dropped, st.width, st.height);
            ui_AiScreenSecondary_set_state(st.state, nullptr);
            break;
        }

        default:
            break;
        }
    }

    void DisplayTask::applyAudioPad(const DisplayMessage &msg)
    {
        /* 绑定变更 / 播完清高亮统一走这里（AudioPad 模块投递） */
        ui_AudioScreenSecondary_set_pads(msg.audio_pad.files);
        ui_AudioScreenSecondary_set_playing(msg.audio_pad.playing_key);
    }

    void DisplayTask::applySetting(const DisplayMessage &msg)
    {
        const ui_settings_snapshot_t &s = msg.setting;

        /* 标题等 UI 文本按系统语言刷新（0=中文 1=English，默认中文）。
         * 开机快照与运行时变更都走这里，ui_init 之后调用时屏幕已存在。 */
        ui_lang_set(s.ui_lang);

        /*
         * 背光即时生效：
         *   1) 先落缓存（用户亮度 / 电源模式），供空闲策略恢复时使用；
         *   2) 正常态直接按配置亮度刷新；
         *   3) checkPowerIdle() 处理"改配置时仍处于空闲省电态"的情况
         *      （如 App 在关屏期间把模式改回 0 → 立即恢复亮度）。
         */
        tft_brightness_ = static_cast<uint8_t>(s.tft_brightness);
        power_mode_ = static_cast<uint8_t>(s.power_mode);
        if (power_idle_state_ == PowerIdleState::Normal)
        {
            Backlight::instance().setDuty(tft_brightness_);
        }
        checkPowerIdle();

        /* RGB 灯效即时生效（以配置层权威值为准，DisplayTask 上下文统一驱动） */
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            RGBLightControl::instance().applySettings(snap);
            ClickHighlight::applySettings(snap);
        }

        /* 状态栏：工作模式图标、音量 */
        status_bar_set_working_mode(s.work_mode);
        status_bar_set_volume(s.device_volume);

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
        case UI_SCREEN_AUDIO_SECONDARY:
            target = ui_AudioScreenSecondary;
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
        case UI_SCREEN_AI:
            target = ui_AiScreen;
            break;
        case UI_SCREEN_AI_SECONDARY:
            target = ui_AiScreenSecondary;
            break;
        case UI_SCREEN_GAMES:
            target = ui_GameScreen;
            break;
        case UI_SCREEN_GAME_SELECT:
            target = ui_GameSelectScreen;
            break;
        case UI_SCREEN_TETRIS:
            /* 竖屏游戏页由 tetrisGame_enter() 统一切屏 + 开局；协议/主动导航兜底 */
            tetrisGame_enter();
            return;
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
             * - 图标走符号回退：不做 SPIFFS 读 + 解码（预览高频路径，
             *   逐 profile 解码不值得），也不释放像素缓存
             */
            ui_KeyMappedSecondary_set_profile(p.profile_icon, p.profile_name,
                                              fileName,
                                              /*update_main_summary=*/false);
            /*
             * 预览态图标：只显示符号回退（set_profile_icon_source 会隐藏
             * 位图 widget 并显示符号）。不触碰像素缓存——预览切换/退出后
             * bind 恢复时仍用缓存展示已应用方案的图标，避免预览污染。
             */
            ui_KeyMappedSecondary_set_profile_icon_source(p.profile_icon);
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
         * Profile 图标显示（解码管线已接入，见 utils/ProfileIconImage）：
         * 图标存在性读 Configuration 内存缓存（避免跨任务 SPIFFS.exists），
         * 存在时读文件 + lodepng 解码 + 转 16bpp TRUE_COLOR_ALPHA，再经
         * ui_*_set_profile_icon_image_data 分发（内部缓存像素并在
         * bind / screen 重建时恢复）；失败或不存在走符号回退。
         * 单次解码 <10ms（≤64×64），applied 推送为低频路径，可接受。
         */
        const bool has_icon = Configuration::instance().isProfileIconPresent(
            p.profile_index);
        uint8_t *icon_px = nullptr;
        size_t icon_px_size = 0;
        uint16_t icon_w = 0;
        uint16_t icon_h = 0;
        const bool icon_loaded =
            has_icon && profileIconLoadTrueColorAlpha(p.profile_index,
                                                      &icon_px, &icon_px_size,
                                                      &icon_w, &icon_h);
        ui_KeyMapped_set_profile_icon_image_data(icon_loaded ? icon_px : nullptr,
                                                 icon_loaded ? icon_px_size : 0,
                                                 icon_w, icon_h,
                                                 p.profile_icon);
        ui_KeyMappedSecondary_set_profile_icon_image_data(
            icon_loaded ? icon_px : nullptr,
            icon_loaded ? icon_px_size : 0,
            icon_w, icon_h,
            p.profile_icon);
        if (icon_loaded)
        {
            free(icon_px); /* UI 侧 lv_mem_alloc 拷贝后即可释放 */
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

        /* 联网状态已从 UI 移除（USB 直连场景无意义），不再消费 network_connected */
        ui_PcStatusScreen_set_net_up_kbps(pc.network_up_kbps);
        ui_PcStatusScreen_set_net_down_kbps(pc.network_down_kbps);
        ui_PcStatusScreen_set_cpu_percent(pc.cpu_usage_percent);
        ui_PcStatusScreen_set_gpu_usage_percent(pc.gpu_usage_percent);
        ui_PcStatusScreen_set_mem_percent(pc.memory_usage_percent);
        ui_PcStatusScreen_set_disk_io_percent(pc.disk_io_percent);
        ui_PcStatusScreen_set_disk_space_percent(pc.disk_space_percent);
    }

    void DisplayTask::applyHaStatus(const DisplayMessage &msg)
    {
        const HaStatusInfo &ha = msg.ha_status;

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
        ui_HaScreenSecondary_set_ha_status(ha.ha_api_connected);
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
        const bool music_visible = (tag == UI_SCREEN_MUSIC ||
                                    tag == UI_SCREEN_MUSIC_SECONDARY);
        /* 拾音 RGB 模式与音乐屏共用 Mic 链路（调度/退避/ASR 互斥同套逻辑） */
        const bool rgb_sound = RGBLightControl::instance().wantsMic();
        const bool visible = music_visible || rgb_sound;

        /*
         * 频谱滚动缓冲（跨轮次累积 PCM，凑满一个完整 512 点窗才做 FFT）。
         * Mic::Read 超时 20ms < 512 样本所需 32ms，单次直读 512 必然拿到
         * 残缺帧（尾部补零 + 汉宁窗在中途、窗值仍 ~0.85 处硬截断 → 频谱泄漏），
         * 且帧间窗口长度随机 → 频段值逐帧跳变（灯效抖动的根因）。
         * 每轮只取 kHop=256 样本（16ms）累积，凑满后做 FFT 再整体前移 kHop
         * （50% 重叠，相邻帧共享一半样本，频谱更稳）。
         */
        static constexpr size_t kHop = AudioAnalyzer::kFftSize / 2;
        static int16_t roll[AudioAnalyzer::kFftSize];
        static size_t roll_n = 0;

        if (!visible)
        {
            if (spectrum_active_)
            {
                spectrum_active_ = false;
                Mic::instance().end();
                /* 与 Mic::end() 对称释放 PSRAM 双缓冲，并复位长时峰值跟踪，
                 * 避免下次进入拾音时沿用上一段会话的高增益放大底噪 */
                AudioAnalyzer::instance().end();
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
            roll_n = 0; /* 会话重启：丢弃上次残留样本，避免跨段拼窗 */
            LOG_INFO("DISP", "spectrum started");
        }

        /* 读 kHop 样本累积进滚动缓冲，凑满完整窗才做 FFT（见函数开头注释） */
        static int16_t chunk[kHop];
        static float bands[AudioAnalyzer::kBandCount];
        const size_t n = Mic::instance().Read(chunk, kHop);
        if (n == 0)
        {
            return;
        }
        memcpy(roll + roll_n, chunk, n * sizeof(int16_t));
        roll_n += n;
        if (roll_n < AudioAnalyzer::kFftSize)
        {
            return; /* 未凑满 512 点，等下一轮补齐 */
        }
        AudioAnalyzer::instance().process(roll, AudioAnalyzer::kFftSize,
                                          bands, AudioAnalyzer::kBandCount);
        /* 50% 重叠：前移 kHop，保留后半窗样本供下一帧复用 */
        memmove(roll, roll + kHop,
                (AudioAnalyzer::kFftSize - kHop) * sizeof(int16_t));
        roll_n = AudioAnalyzer::kFftSize - kHop;

        if (rgb_sound)
        {
            RGBLightControl::instance().setAudioBands(bands, AudioAnalyzer::kBandCount);
        }

        if (music_visible)
        {
            /* 0~1 → 0~255（ui_MusicScreen_drawAudioBandsCool 期望刻度） */
            float draw_bands[AudioAnalyzer::kBandCount];
            for (size_t i = 0; i < AudioAnalyzer::kBandCount; ++i)
            {
                draw_bands[i] = bands[i] * 255.0f;
            }
            ui_MusicScreen_drawAudioBandsCool(draw_bands);
        }
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
        case UI_SCREEN_AUDIO_SECONDARY:
        case UI_SCREEN_PC_STATUS_SECONDARY:
        case UI_SCREEN_HA_SECONDARY:
        case UI_SCREEN_SETTING_SECONDARY:
        case UI_SCREEN_AI_SECONDARY:
        case UI_SCREEN_GAME_SELECT: /* docs/14：游戏二级页与竖屏游戏页不参与 5s 回主页 */
        case UI_SCREEN_TETRIS:
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
        /* AI 二级功能页（图像流）常驻展示，不参与 5s 自动回主页（docs/13 §4.1）；
         * 一级入口页与其它一级页一致，仍参与自动回主页 */
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

    void DisplayTask::bumpInputActivity()
    {
        last_input_tick_ = xTaskGetTickCount();
        /* 省电态（降亮/关屏）下收到任意输入 → 立即恢复用户配置亮度 */
        if (power_idle_state_ != PowerIdleState::Normal)
        {
            power_idle_state_ = PowerIdleState::Normal;
            Backlight::instance().setDuty(tft_brightness_);
            LOG_INFO("DISP", "power idle: input detected, restore backlight to %u%%",
                     static_cast<unsigned>(tft_brightness_));
        }
    }

    void DisplayTask::checkPowerIdle()
    {
        PowerIdleState desired = PowerIdleState::Normal;
        if (power_mode_ == kPowerModeLowPower ||
            power_mode_ == kPowerModeLightSleep)
        {
            const TickType_t now = xTaskGetTickCount();
            if ((now - last_input_tick_) >= pdMS_TO_TICKS(kPowerIdleTimeoutMs))
            {
                desired = (power_mode_ == kPowerModeLowPower)
                              ? PowerIdleState::Dimmed
                              : PowerIdleState::Off;
            }
        }

        /* AI 二级功能页常亮豁免（docs/13 §4.1）：图像流展示期间强制 Normal，
         * 若已处于省电态（进页前已空闲）立即恢复亮度 */
        if (ui_get_active_screen_tag() == UI_SCREEN_AI_SECONDARY)
        {
            desired = PowerIdleState::Normal;
        }
        /* 俄罗斯方块游戏页常亮豁免（docs/14 §7）：落块过程不因空闲被降亮/关屏 */
        if (tetrisGame_isActive())
        {
            desired = PowerIdleState::Normal;
        }

        if (desired == power_idle_state_)
        {
            return;
        }
        power_idle_state_ = desired;

        switch (desired)
        {
        case PowerIdleState::Dimmed:
        {
            const uint8_t dim = (tft_brightness_ < kPowerIdleDimPercent)
                                    ? tft_brightness_
                                    : kPowerIdleDimPercent;
            Backlight::instance().setDuty(dim);
            LOG_INFO("DISP", "power idle: dim backlight to %u%% (mode %u)",
                     static_cast<unsigned>(dim),
                     static_cast<unsigned>(power_mode_));
            break;
        }
        case PowerIdleState::Off:
            Backlight::instance().off();
            LOG_INFO("DISP", "power idle: backlight off (mode %u)",
                     static_cast<unsigned>(power_mode_));
            break;
        default:
            Backlight::instance().setDuty(tft_brightness_);
            LOG_INFO("DISP", "power idle: restore backlight to %u%%",
                     static_cast<unsigned>(tft_brightness_));
            break;
        }
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
            const unsigned long idle_ms = (unsigned long)pdTICKS_TO_MS(now - last_activity_tick_);
            /* 先清零再导航：即使 navigateNow 因目标屏已是当前屏而短路，
             * 也不会下一 tick 重复触发（防日志刷屏与 tag 失配死循环） */
            last_activity_tick_ = 0;
            LOG_INFO("DISP", "auto return to MAIN after %lu ms idle", idle_ms);
            navigateNow(UI_SCREEN_MAIN);
        }
    }

} // namespace ekeys
