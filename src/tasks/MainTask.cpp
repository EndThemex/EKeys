/*
 * MainTask.cpp
 *
 * 阶段 05：
 *   - 5ms tick：MatrixScanner::scan() → KeyResolver → IKeyboard
 *   - EC11 旋钮轮询 → ActionInput
 *   - 设置屏反向同步：ui_settings_request_apply/save（LVGL 回调 →
 *     临界区暂存）→ 本任务 loop() 消费 → DeviceSettings + 持久化
 *   - 键映射加载后投递 KEYMAP_PROFILE_UPDATE（队列就绪后惰性发送）
 *   - 1s  tick：TIME_UPDATE（millis() 推算；阶段 06 换 NTP）
 */

#include "MainTask.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdio.h>

#include "app/AppContext.h"
#include "config/Configuration.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include "output/IKeyboard.h"
#include "protocol/SerialProtocol.h"

namespace ekeys
{

    namespace
    {

        constexpr uint32_t kMainTaskTickPeriodMs = 5;
        constexpr uint32_t kMainTaskTimePostPeriodMs = 1000;

        /*
         * 把 millis() 换算成 "HH:MM:SS"。不依赖 NTP；阶段 06 替换为
         * NtpSync::epochSeconds() 等接口。
         */
        void formatUptimeString(char *out, size_t cap, uint32_t now_ms)
        {
            uint32_t total_s = now_ms / 1000U;
            uint32_t h = (total_s / 3600U) % 24U;
            uint32_t m = (total_s / 60U) % 60U;
            uint32_t s = total_s % 60U;
            snprintf(out, cap, "%02u:%02u:%02u",
                     static_cast<unsigned>(h),
                     static_cast<unsigned>(m),
                     static_cast<unsigned>(s));
        }

        int clampInt(int v, int lo, int hi)
        {
            return (v < lo) ? lo : (v > hi) ? hi
                                            : v;
        }

        /*
         * 设置屏反向同步通道（FEATURE_DOC §8.4）：
         * LVGL 事件回调经 ui_settings_request_apply()/save() 写入，
         * MainTask::loop() 消费。临界区用 FreeRTOS spinlock。
         */
        struct PendingUiSettingsRequest
        {
            bool pending{false};
            bool persist{false};
            ui_settings_snapshot_t snapshot{};
        };

        portMUX_TYPE g_ui_settings_lock = portMUX_INITIALIZER_UNLOCKED;
        PendingUiSettingsRequest g_ui_settings_request{};
        MainTask *g_main_task = nullptr;

    } // namespace

    /*
     * 供 SquareLine 生成的 ui_SettingScreenSecondary.c 调用（C 链接）。
     * g_main_task 未就绪（启动早期）时返回 false，由 UI 侧忽略。
     */
    extern "C" bool ui_settings_request_apply(const ui_settings_snapshot_t *snapshot)
    {
        if (snapshot == nullptr || g_main_task == nullptr)
        {
            return false;
        }

        taskENTER_CRITICAL(&g_ui_settings_lock);
        g_ui_settings_request.snapshot = *snapshot;
        g_ui_settings_request.pending = true;
        g_ui_settings_request.persist = false;
        taskEXIT_CRITICAL(&g_ui_settings_lock);
        return true;
    }

    extern "C" bool ui_settings_request_save(const ui_settings_snapshot_t *snapshot)
    {
        if (snapshot == nullptr || g_main_task == nullptr)
        {
            return false;
        }

        taskENTER_CRITICAL(&g_ui_settings_lock);
        g_ui_settings_request.snapshot = *snapshot;
        g_ui_settings_request.pending = true;
        g_ui_settings_request.persist = true;
        taskEXIT_CRITICAL(&g_ui_settings_lock);
        return true;
    }

    MainTask::MainTask()
        : resolver_(Configuration::instance()),
          keyboard_(nullptr),
          display_queue_(nullptr),
          last_tick_ms_(0),
          last_time_post_ms_(0)
    {
    }

    void MainTask::begin()
    {
        /* 加载 /config.ini（文件缺失时使用默认值），再加载键映射 */
        Configuration::instance().load();

        scanner_.begin();
        resolver_.begin();

        g_main_task = this;
        encoder_.begin();
        encoder_.setCallback([this](uint8_t key)
                             { sendDisplayAction(key); });
        keymap_ui_pending_ = true;

        last_tick_ms_ = millis();
        last_time_post_ms_ = last_tick_ms_;
        LOG_INFO("MAIN", "MainTask started");
    }

    void MainTask::end()
    {
        g_main_task = nullptr;
        resolver_.end();
    }

    void MainTask::reloadKeymap()
    {
        resolver_.begin();
        keymap_ui_pending_ = true;
    }

    void MainTask::loop()
    {
        /* 协议层轮询（CDC JSON 行收发），不依赖 keyboard_ 注入 */
        SerialProtocol::instance().poll();

        /* EC11 旋钮（单击进入 / 双击返回 / 旋转左右） */
        encoder_.loop();

        /* 设置屏反向同步请求（apply / save） */
        ui_settings_snapshot_t pending{};
        bool persist = false;
        bool hasPending = false;
        {
            taskENTER_CRITICAL(&g_ui_settings_lock);
            if (g_ui_settings_request.pending)
            {
                pending = g_ui_settings_request.snapshot;
                persist = g_ui_settings_request.persist;
                g_ui_settings_request.pending = false;
                g_ui_settings_request.persist = false;
                hasPending = true;
            }
            taskEXIT_CRITICAL(&g_ui_settings_lock);
        }
        if (hasPending)
        {
            applyUiSettingsSnapshot(pending, persist);
        }

        /* 队列就绪后补发键映射屏数据（AppContext 在 begin() 后注入队列） */
        if (keymap_ui_pending_ && display_queue_ != nullptr)
        {
            sendKeymapProfileUi();
            keymap_ui_pending_ = false;
        }

        if (keyboard_ == nullptr)
        {
            return;
        }
        uint32_t now = millis();

        /* 5ms tick */
        if ((now - last_tick_ms_) >= kMainTaskTickPeriodMs)
        {
            last_tick_ms_ = now;
            scanner_.scan();

            uint8_t pressed[kMatrixKeyCount];
            uint8_t released[kMatrixKeyCount];
            uint8_t pc = 0;
            uint8_t rc = 0;
            scanner_.getPressedKeys(pressed, pc);
            scanner_.getReleasedKeys(released, rc);

            for (uint8_t i = 0; i < pc; ++i)
            {
                resolver_.press(pressed[i], *keyboard_);
            }
            for (uint8_t i = 0; i < rc; ++i)
            {
                resolver_.release(released[i], *keyboard_);
            }
        }

        /* 1s tick：投递 TIME_UPDATE */
        if ((now - last_time_post_ms_) >= kMainTaskTimePostPeriodMs)
        {
            last_time_post_ms_ = now;
            DisplayMessage msg;
            msg.type = DisplayMessageType::TimeUpdate;
            formatUptimeString(msg.time_text,
                               sizeof(msg.time_text), now);
            postMessage(msg);
        }
    }

    void MainTask::postMessage(const DisplayMessage &msg)
    {
        if (display_queue_ == nullptr)
        {
            return;
        }
        xQueueSend(static_cast<QueueHandle_t>(display_queue_), &msg, 0);
    }

    void MainTask::sendDisplayAction(uint8_t action)
    {
        DisplayMessage msg;
        msg.type = DisplayMessageType::ActionInput;
        msg.action = action;
        postMessage(msg);
    }

    void MainTask::sendKeymapProfileUi()
    {
        Configuration &config = Configuration::instance();

        DeviceSettings snap;
        config.snapshot(snap);

        DisplayMessage msg;
        msg.type = DisplayMessageType::KeymapProfile;
        KeymapProfileInfo &p = msg.keymap_profile;
        p.active_profile = snap.active_keymap_profile;

        snprintf(p.profile_name, sizeof(p.profile_name), "%s",
                 config.getProfileDisplayName(p.active_profile));
        snprintf(p.profile_icon, sizeof(p.profile_icon), "%s",
                 LV_SYMBOL_SETTINGS);

        for (uint8_t i = 0; i < kMatrixKeyCount; ++i)
        {
            const KeyMapping &mapping = resolver_.get(static_cast<uint8_t>(i + 1));
            String combo;
            if (!mapping.function_key.isEmpty())
            {
                combo = mapping.function_key;
            }
            else
            {
                for (uint8_t j = 0; j < kKeyMappingNormalCount; ++j)
                {
                    if (mapping.normal_key[j].isEmpty())
                    {
                        continue;
                    }
                    if (!combo.isEmpty())
                    {
                        combo += "+";
                    }
                    combo += mapping.normal_key[j];
                }
            }
            snprintf(p.keymap_labels[i], sizeof(p.keymap_labels[i]), "%u:%s",
                     static_cast<unsigned>(i + 1),
                     combo.isEmpty() ? "--" : combo.c_str());
        }

        postMessage(msg);
    }

    void MainTask::applyUiSettingsSnapshot(const ui_settings_snapshot_t &requested,
                                           bool persist)
    {
        ui_settings_snapshot_t s = requested;

        /* 与 parseConfigSetCommand 相同的取值约束（FEATURE_DOC §6） */
        s.work_mode = clampInt(s.work_mode, 0, 2);
        s.rgb_brightness = clampInt(s.rgb_brightness, 0, 100);
        s.tft_brightness = clampInt(s.tft_brightness, 5, 100);
        s.device_volume = clampInt(s.device_volume, 0, 100);
        s.active_keymap_profile = s.active_keymap_profile >= Configuration::CONFIG_PROFILE_COUNT
                                      ? 0
                                      : s.active_keymap_profile;
        s.rgb_single_color[sizeof(s.rgb_single_color) - 1] = '\0';
        const int rgbSingleColor = clampInt(atoi(s.rgb_single_color), 0, 255);

        bool changed = false;
        bool workModeChanged = false;
        uint8_t newWorkMode = 0;
        bool profileChanged = false;

        Configuration &config = Configuration::instance();
        config.mutateSettings([&](DeviceSettings &d)
                              {
            if (d.work_mode != static_cast<uint8_t>(s.work_mode))
            {
                d.work_mode = static_cast<uint8_t>(s.work_mode);
                changed = true;
                workModeChanged = true;
                newWorkMode = d.work_mode;
            }
            if (d.rgb_mode != static_cast<uint8_t>(s.rgb_mode))
            {
                d.rgb_mode = static_cast<uint8_t>(s.rgb_mode);
                changed = true;
            }
            if (d.rgb_single_colar != static_cast<uint8_t>(rgbSingleColor))
            {
                d.rgb_single_colar = static_cast<uint8_t>(rgbSingleColor);
                changed = true;
            }
            if (d.rgb_click_mode != static_cast<uint8_t>(s.rgb_click_mode))
            {
                d.rgb_click_mode = static_cast<uint8_t>(s.rgb_click_mode);
                changed = true;
            }
            if (d.rgb_brightness != static_cast<uint8_t>(s.rgb_brightness))
            {
                d.rgb_brightness = static_cast<uint8_t>(s.rgb_brightness);
                changed = true;
            }
            if (d.tft_theme != static_cast<uint8_t>(s.tft_theme))
            {
                d.tft_theme = static_cast<uint8_t>(s.tft_theme);
                changed = true;
            }
            if (d.tft_brightness != static_cast<uint8_t>(s.tft_brightness))
            {
                d.tft_brightness = static_cast<uint8_t>(s.tft_brightness);
                changed = true;
            }
            if (d.device_volume != static_cast<uint8_t>(s.device_volume))
            {
                d.device_volume = static_cast<uint8_t>(s.device_volume);
                changed = true;
            }
            if (d.power_mode != static_cast<uint8_t>(s.power_mode))
            {
                d.power_mode = static_cast<uint8_t>(s.power_mode);
                changed = true;
            }
            if (d.audio_enable != static_cast<uint8_t>(s.audio_enable))
            {
                d.audio_enable = static_cast<uint8_t>(s.audio_enable);
                changed = true;
            }
            if (d.connect_host != static_cast<uint8_t>(s.connect_host))
            {
                d.connect_host = static_cast<uint8_t>(s.connect_host);
                changed = true;
            }
            if (d.voice_enable != static_cast<uint8_t>(s.voice_enable))
            {
                d.voice_enable = static_cast<uint8_t>(s.voice_enable);
                changed = true;
            }
            if (d.active_keymap_profile != s.active_keymap_profile)
            {
                d.active_keymap_profile = s.active_keymap_profile;
                changed = true;
                profileChanged = true;
            } });

        if (!changed)
        {
            return;
        }

        LOG_INFO("MAIN", "ui settings %s (work_mode=%d tft=%d rgb=%d vol=%d)",
                 persist ? "save" : "apply",
                 static_cast<int>(s.work_mode),
                 static_cast<int>(s.tft_brightness),
                 static_cast<int>(s.rgb_brightness),
                 static_cast<int>(s.device_volume));

        /*
         * 持久化（saveSetting 内部自行加锁，不能放进 mutator）。
         * apply（persist=false）只改内存，不写 INI。
         */
        if (persist)
        {
            config.saveSetting("work_mode", static_cast<int>(s.work_mode));
            config.saveSetting("rgb_mode", static_cast<int>(s.rgb_mode));
            config.saveSetting("rgb_single_colar", rgbSingleColor);
            config.saveSetting("rgb_click_mode", static_cast<int>(s.rgb_click_mode));
            config.saveSetting("rgb_brightness", static_cast<int>(s.rgb_brightness));
            config.saveSetting("tft_theme", static_cast<int>(s.tft_theme));
            config.saveSetting("tft_brightness", static_cast<int>(s.tft_brightness));
            config.saveSetting("device_volume", static_cast<int>(s.device_volume));
            config.saveSetting("power_mode", static_cast<int>(s.power_mode));
            config.saveSetting("audio_enable", static_cast<int>(s.audio_enable));
            config.saveSetting("connect_host", static_cast<int>(s.connect_host));
            config.saveSetting("voice_enable", static_cast<int>(s.voice_enable));
            config.saveSetting("active_keymap_profile", static_cast<int>(s.active_keymap_profile));
        }

        /* 副作用：工作模式重建键盘 / Profile 重载键映射 */
        if (workModeChanged)
        {
            AppContext::instance().applyWorkMode(newWorkMode);
        }
        if (profileChanged)
        {
            reloadKeymap();
        }

        /* 刷新显示（背光 / 状态条 / 设置屏快照），以配置层权威值为准 */
        DeviceSettings snap;
        config.snapshot(snap);
        DisplayMessage msg;
        msg.type = DisplayMessageType::SettingUpdate;
        fillSettingPayload(snap, msg.setting);
        postMessage(msg);
    }

    void MainTask::tick()
    {
        /*
         * 阶段 06 后会在此处调度 WiFi / BLE / ASR 等异步子模块。
         */
    }

} // namespace ekeys
