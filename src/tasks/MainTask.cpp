/*
 * MainTask.cpp
 *
 * 阶段 05：
 *   - 5ms tick：MatrixScanner::scan() → KeyResolver → IKeyboard
 *   - EC11 旋钮轮询 → ActionInput
 *   - 设置屏反向同步：ui_settings_request_apply/save（LVGL 回调 →
 *     临界区暂存）→ 本任务 loop() 消费 → DeviceSettings + 持久化
 *   - 键映射加载后投递 KEYMAP_PROFILE_UPDATE（队列就绪后惰性发送）
 *   - 1s  tick：TIME_UPDATE（NTP 已同步用真实时间，否则 millis() 推算）
 *
 * 阶段 06（tick()）：
 *   - WiFi 重连状态机 / NTP 完成检测 / UDP 发现 / TCP 收发
 *   - 扬声器解码喂流 / ASR 录音攒流
 *   - HA 状态聚合（NetDiagnostics → HA 屏，2.5s 节流）
 */

#include "MainTask.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <time.h>

#include "app/AppContext.h"
#include "audio/AudioPad.h"
#include "audio/Speaker.h"
#include "config/Configuration.h"
#include "hardware/BatteryMonitor.h"
#include "keymap/KeyEventDispatcher.h"
#include "keymap/KeyNameTable.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include <time.h>
#include "network/DiscoveryService.h"
#include "network/NetDiagnostics.h"
#include "network/NtpSync.h"
#include "network/TcpChannel.h"
#include "network/WiFiManager.h"
#include "output/IKeyboard.h"
#include "protocol/SerialProtocol.h"
#include "ui/ui_KeyMapped.h"
#include "utils/keymap_types.h"
#include "voice/VoiceRecognizer.h"

namespace ekeys
{

    namespace
    {

        constexpr uint32_t kMainTaskTickPeriodMs = 5;
        constexpr uint32_t kMainTaskTimePostPeriodMs = 1000;
        constexpr uint32_t kHaStatusPeriodMs = 2500;
        constexpr uint32_t kBatteryStatusPeriodMs = 5000;

        /*
         * 把 millis() 换算成 "HH:MM:SS"。NTP 未同步时作为兜底显示。
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
         * 单键标签格式化（sendKeymapProfileUi 内 layerSummary 逻辑迁出，
         * 供 applied / preview 两条发送路径复用）。按 fun_layer 选择映射
         * 通道，生成可读摘要；空映射输出 "--"。
         * 注意：KeyMapping 含多个 String，必须按 const 引用传参。
         */
        void formatKeymapLabel(const KeyMapping &mapping, uint8_t fun_layer,
                               char *out, size_t cap)
        {
            auto layerSummary = [](const String &fk, const String &tk,
                                   const std::array<String, kKeyMappingNormalCount> &nk)
            {
                String s;
                if (!fk.isEmpty())
                {
                    /* 功能键同样可能存 usage code 字面量（如 "0x52"），转可读名 */
                    s = keyDisplayName(fk);
                }
                else if (!tk.isEmpty())
                {
                    /* 文本注入键直接显示内容（超长由调用方 snprintf 截断） */
                    s = tk;
                }
                else
                {
                    for (uint8_t j = 0; j < kKeyMappingNormalCount; ++j)
                    {
                        if (nk[j].isEmpty())
                        {
                            continue;
                        }
                        if (!s.isEmpty())
                        {
                            s += "+";
                        }
                        /* App 端可能存 "0x04" 这类 usage code 字面量，显示转可读名 */
                        s += keyDisplayName(nk[j]);
                    }
                }
                return s;
            };

            String combo;
            if (fun_layer == 1)
            {
                /* FUN1 按住：只显示该键组合层摘要（未配置显示 --） */
                combo = layerSummary(mapping.combo1_function_key,
                                     mapping.combo1_text_key,
                                     mapping.combo1_normal_key);
            }
            else if (fun_layer == 2)
            {
                combo = layerSummary(mapping.combo2_function_key,
                                     mapping.combo2_text_key,
                                     mapping.combo2_normal_key);
            }
            else
            {
                /* 单击视图：只显示单击配置；组合层在 FUN 键按住时整屏切换预览 */
                combo = layerSummary(mapping.function_key, mapping.text_key,
                                     mapping.normal_key);
            }
            snprintf(out, cap, "%s", combo.isEmpty() ? "--" : combo.c_str());
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

        /*
         * 键映射二级页旋钮请求（与设置屏反向同步同款通道）：
         * LVGL 事件回调经 ui_keymap_request_profile_switch() /
         * ui_keymap_request_profile_apply() 写入，MainTask::loop() 消费。
         * 旋转只累积"预览"步进（合并消费，一次 SPIFFS 读取）；
         * 单击 apply 在请求瞬间捕获目标（g_preview_profile_index），
         * 避免同 5ms 窗口内迟到的 step 干扰。
         */
        struct PendingProfileSwitchRequest
        {
            bool pending{false};
            int16_t step{0};
            bool apply_pending{false};
            uint8_t apply_target{0};
        };

        portMUX_TYPE g_profile_switch_lock = portMUX_INITIALIZER_UNLOCKED;
        PendingProfileSwitchRequest g_profile_switch_request{};

        /* 当前预览的 profile（0~7）：预览/应用的唯一事实源 */
        uint8_t g_preview_profile_index = 0;

        /* 预览 SPIFFS 读取最小间隔：连转旋钮时限制 INI 解析频率 */
        constexpr uint32_t kKeymapPreviewMinIntervalMs = 100;

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

    /*
     * 供 SquareLine 生成的 ui_KeyMappedSecondary.c 调用（C 链接）。
     * 旋钮旋转预览 Profile：step=+1 顺时针 / -1 逆时针，累积合并消费；
     * 只切预览视图，不应用不落盘（应用经 ui_keymap_request_profile_apply）。
     */
    extern "C" bool ui_keymap_request_profile_switch(int step)
    {
        if (step == 0 || g_main_task == nullptr)
        {
            return false;
        }

        taskENTER_CRITICAL(&g_profile_switch_lock);
        int32_t total = static_cast<int32_t>(g_profile_switch_request.step) + step;
        if (total > 100)
        {
            total = 100;
        }
        else if (total < -100)
        {
            total = -100;
        }
        g_profile_switch_request.step = static_cast<int16_t>(total);
        g_profile_switch_request.pending = true;
        taskEXIT_CRITICAL(&g_profile_switch_lock);
        return true;
    }

    /*
     * 供 ui_KeyMappedSecondary.c 调用（C 链接）。
     * 单击确认应用当前预览的 Profile：目标在请求瞬间捕获，
     * MainTask 消费后经 sendKeymapProfileUi 回发 applied 消息，
     * UI 侧收尾等待遮罩。
     */
    extern "C" bool ui_keymap_request_profile_apply(void)
    {
        if (g_main_task == nullptr)
        {
            return false;
        }

        taskENTER_CRITICAL(&g_profile_switch_lock);
        g_profile_switch_request.apply_target = g_preview_profile_index;
        g_profile_switch_request.apply_pending = true;
        taskEXIT_CRITICAL(&g_profile_switch_lock);
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
        /* /config.ini 已由 AppContext::init 提前加载（DisplayTask 并行
         * 启动的前提），这里只取活动配置档索引，随后加载键映射 */
        g_preview_profile_index = Configuration::instance().activeProfile();

        scanner_.begin();
        resolver_.begin();

        /* A1 修复：注入 resolver，使 isVoiceTriggerKey 能命中 function_key=KEY_FUNCTION_ASR */
        KeyEventDispatcher::init(&resolver_);

        g_main_task = this;
        /* N2：先 setCallback 再 begin，避免 begin 内部 attach 时 callback_ 仍为 nullptr */
        encoder_.setCallback([this](uint8_t key)
                             { sendDisplayAction(key); });
        encoder_.begin();
        keymap_ui_pending_ = true;

        /*
         * 阶段 06 服务初始化（宿主 = MainTask，各模块头文件约定）：
         *   - WiFi 状态机（BLE 模式 / wifi_switch=0 时保持关闭）
         *   - WiFi 连上 → NTP 同步 + UDP 发现；发现 App IP → TCP 连接
         *   - I2S 扬声器（幂等 begin，后续 tick() 喂流）
         *   - 启动时按当前配置调度 WiFi 连接
         */
        WiFiManager::instance().begin();
        WiFiManager::instance().setOnConnected([]()
                                               {
            NtpSync::instance().requestSync();
            DiscoveryService::instance().start(); });
        DiscoveryService::instance().setOnDiscovered(
            [](const char *ip)
            { TcpChannel::instance().connectTo(ip); });
        Speaker::instance().begin();
        BatteryMonitor::instance().begin();
        if (WiFiManager::instance().isEnabled())
        {
            WiFiManager::instance().scheduleConnect();
        }

        last_tick_ms_ = millis();
        last_time_post_ms_ = last_tick_ms_;
        last_ha_status_ms_ = last_tick_ms_;
        last_battery_status_ms_ = last_tick_ms_;
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

    void MainTask::applyKeymap(
        const std::array<KeyMapping, kMatrixKeyCount + 1> &map, uint16_t keyMask)
    {
        resolver_.apply(map, keyMask);
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

        /*
         * 键映射二级页旋钮请求消费：apply 优先（applied 消息先入队），
         * 预览步进合并消费（只切预览视图，不应用不落盘）。
         */
        {
            bool apply_now = false;
            uint8_t apply_target = 0;
            int16_t profile_step = 0;
            taskENTER_CRITICAL(&g_profile_switch_lock);
            if (g_profile_switch_request.apply_pending)
            {
                apply_now = true;
                apply_target = g_profile_switch_request.apply_target;
                g_profile_switch_request.apply_pending = false;
                /* 消费 apply 时丢弃未处理的预览步进（目标已在请求时捕获） */
                g_profile_switch_request.pending = false;
                g_profile_switch_request.step = 0;
            }
            else if (g_profile_switch_request.pending)
            {
                profile_step = g_profile_switch_request.step;
                g_profile_switch_request.pending = false;
                g_profile_switch_request.step = 0;
            }
            taskEXIT_CRITICAL(&g_profile_switch_lock);

            if (apply_now)
            {
                const uint8_t active =
                    Configuration::instance().activeProfile();
                if (apply_target != active)
                {
                    Configuration::instance().switchActiveProfile(apply_target);
                    reloadKeymap();
                }
                else
                {
                    /* 同 profile：跳过 SPIFFS 原子写，applied 消息照发供 UI 收尾遮罩 */
                    keymap_ui_pending_ = true;
                }
                LOG_INFO("MAIN", "keymap secondary profile apply -> %u",
                         static_cast<unsigned>(apply_target));
            }
            else if (profile_step != 0)
            {
                const uint8_t count = Configuration::CONFIG_PROFILE_COUNT;
                int next = static_cast<int>(g_preview_profile_index) + profile_step;
                next = ((next % count) + count) % count;
                g_preview_profile_index = static_cast<uint8_t>(next);
                keymap_preview_ui_pending_ = true;
            }
        }

        /* 已应用视图：post 失败不清标志，下轮重试（防队列满丢消息后遮罩永挂） */
        if (keymap_ui_pending_ && display_queue_ != nullptr)
        {
            if (sendKeymapProfileUi(fun_ui_layer_))
            {
                keymap_ui_pending_ = false;
            }
        }

        /* 预览视图（节流：连转旋钮时限制 SPIFFS INI 解析频率） */
        if (keymap_preview_ui_pending_ && display_queue_ != nullptr &&
            (millis() - last_preview_load_ms_) >= kKeymapPreviewMinIntervalMs)
        {
            if (sendKeymapProfilePreview(fun_ui_layer_))
            {
                keymap_preview_ui_pending_ = false;
                last_preview_load_ms_ = millis();
            }
        }

        /* 阶段 06 服务调度（网络 / 扬声器 / ASR，不依赖 keyboard_ 注入） */
        tick();

        uint32_t now = millis();

        /*
         * 1s tick：投递 TIME_UPDATE（NTP 已同步用真实时间，否则开机时长）。
         *
         * F10 修复：必须放在 `if (keyboard_ == nullptr) return;` 之前。
         * applyWorkMode() 在 USBKeyboardImpl::begin() 失败时会把 keyboard_ 设成 nullptr
         * （BLE 模式下切换回 USB，USB host 未就绪），后续循环在早期 return 处
         * 一直跳出，导致 1s tick 永不再触发，主屏时间永远停留在最后一次 TIME_UPDATE
         * 的值——典型表现是"App 连接后再切回 work_mode=USB，主屏时间不再变化"。
         * HA 状态 / 电池 / 频谱也类似，但用户体验上时间最显眼。
         */
        if ((now - last_time_post_ms_) >= kMainTaskTimePostPeriodMs)
        {
            last_time_post_ms_ = now;
            DisplayMessage msg;
            msg.type = DisplayMessageType::TimeUpdate;
            if (!NtpSync::instance().getLocalTimeStr(
                    msg.time_text, sizeof(msg.time_text)))
            {
                formatUptimeString(msg.time_text,
                                   sizeof(msg.time_text), now);
            }
            else
            {
                /* 已同步：按 ui_lang 生成最终显示文本（UI 端无需再解析） */
                struct tm tm_now;
                if (getLocalTime(&tm_now, 20))
                {
                    const uint8_t lang =
                        ekeys::Configuration::instance().settings().ui_lang;
                    if (lang == 1)
                    {
                        /* 英文："SEP 08" / "MON" */
                        static const char *const kMonthEn[12] = {
                            "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
                        static const char *const kWeekEn[7] = {
                            "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
                        const int mm = tm_now.tm_mon + 1;
                        snprintf(msg.date_text, sizeof(msg.date_text),
                                 "%s %02d", kMonthEn[mm - 1], tm_now.tm_mday);
                        if (tm_now.tm_wday >= 0 && tm_now.tm_wday < 7)
                        {
                            snprintf(msg.week_text, sizeof(msg.week_text),
                                     "%s", kWeekEn[tm_now.tm_wday]);
                        }
                    }
                    else
                    {
                        /* 中文（默认）："09月08日" / "星期一" */
                        static const char *const kWeekZh[7] = {
                            "星期日", "星期一", "星期二", "星期三",
                            "星期四", "星期五", "星期六"};
                        snprintf(msg.date_text, sizeof(msg.date_text),
                                 "%02d月%02d日",
                                 tm_now.tm_mon + 1, tm_now.tm_mday);
                        if (tm_now.tm_wday >= 0 && tm_now.tm_wday < 7)
                        {
                            snprintf(msg.week_text, sizeof(msg.week_text),
                                     "%s", kWeekZh[tm_now.tm_wday]);
                        }
                    }
                }
            }
            postMessage(msg);
        }

        if (keyboard_ == nullptr)
        {
            return;
        }

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

            const bool suppress_hid =
                ui_get_active_screen_tag() == UI_SCREEN_SETTING_SECONDARY ||
                ui_get_active_screen_tag() == UI_SCREEN_AUDIO_SECONDARY;
            if (!suppress_hid)
            {
                /*
                 * FUN 键同 tick 预扫描：FUN 与其它键同一 5ms tick 按下时，
                 * 若 FUN 键扫描顺序靠后，先按下其它键会误触发单击层。
                 * 这里先把 FUN 置位（press 幂等），再走正常循环。
                 */
                const uint8_t fk1 = resolver_.funKey1();
                const uint8_t fk2 = resolver_.funKey2();
                for (uint8_t i = 0; i < pc; ++i)
                {
                    if ((fk1 != 0 && pressed[i] == fk1) ||
                        (fk2 != 0 && pressed[i] == fk2))
                    {
                        resolver_.press(pressed[i], *keyboard_);
                    }
                }
            }

            for (uint8_t i = 0; i < pc; ++i)
            {
                /*
                 * SettingScreenSecondary 下，矩阵键仅用于 UI 导航（按键 7/11
                 * 切焦点等），不向主机发送 HID。离开该屏后 release 仍正常派发，
                 * 避免卡键。
                 */
                if (!suppress_hid)
                {
                    KeyEventDispatcher::onKeyEdge(pressed[i], true);
                    resolver_.press(pressed[i], *keyboard_);
                }
                /*
                 * A1 修复：应用键 1~11 在 KEYMAPPED 屏被解释为"进入二级页并聚焦该键"。
                 * 编码 = kMatrixKeyActionBase + key_id（2026-09-08 修复：key_id=10
                 * 原来与 LV_KEY_ENTER=10 数值冲突，裸传会被 UI 层当成旋钮单击触发
                 * 导航；加基址偏移后与 LV_KEY_* 完全不重叠，由 DisplayTask 解码）。
                 */
                if (pressed[i] >= 1 && pressed[i] <= kMatrixKeyCount)
                {
                    DisplayMessage nav_msg{};
                    nav_msg.type = DisplayMessageType::ActionInput;
                    nav_msg.action =
                        static_cast<uint8_t>(kMatrixKeyActionBase + pressed[i]);
                    postMessage(nav_msg);
                }
            }
            for (uint8_t i = 0; i < rc; ++i)
            {
                KeyEventDispatcher::onKeyEdge(released[i], false);
                resolver_.release(released[i], *keyboard_);
            }

            /*
             * FUN 组合层切换 → 键映射屏实时预览：
             * FUN 键按住/松开时重推 11 键标签，格子切换显示组合层摘要。
             * 仅在键映射屏推送（2026-09-10）：applyKeymapProfile 内含隐藏
             * label 刷新 + SPIFFS.exists 图标检查，会拖慢 DisplayTask 的
             * RGB 渲染帧，导致 FUN 键点击高亮比其它键晚一帧亮起。
             */
            const ui_screen_tag_t screen = ui_get_active_screen_tag();
            const bool on_keymap_screen =
                (screen == UI_SCREEN_KEYMAPPED ||
                 screen == UI_SCREEN_KEYMAPPED_SECONDARY);

            const uint8_t fun_layer = resolver_.activeFunLayer();
            if (fun_layer != fun_ui_layer_)
            {
                fun_ui_layer_ = fun_layer;
                if (on_keymap_screen)
                {
                    pushCurrentKeymapView(fun_layer);
                }
            }
            else if (on_keymap_screen && screen != keymap_ui_screen_)
            {
                /* 进入键映射屏（含 FUN 按住时进入）→ 补推当前层视图 */
                pushCurrentKeymapView(fun_layer);
            }
            keymap_ui_screen_ = static_cast<uint8_t>(screen);
        }
    }

    bool MainTask::postMessage(const DisplayMessage &msg)
    {
        if (display_queue_ == nullptr)
        {
            return false;
        }
        return xQueueSend(static_cast<QueueHandle_t>(display_queue_), &msg, 0) == pdTRUE;
    }

    void MainTask::sendDisplayAction(uint8_t action)
    {
        DisplayMessage msg;
        msg.type = DisplayMessageType::ActionInput;
        msg.action = action;
        postMessage(msg);
    }

    bool MainTask::sendKeymapProfileUi(uint8_t fun_layer)
    {
        Configuration &config = Configuration::instance();

        DeviceSettings snap;
        config.snapshot(snap);

        DisplayMessage msg;
        msg.type = DisplayMessageType::KeymapProfile;
        KeymapProfileInfo &p = msg.keymap_profile;
        p.active_profile = snap.active_keymap_profile;
        /* applied 消息：描述的 profile 即激活 profile，is_preview 默认 false */
        p.profile_index = snap.active_keymap_profile;

        snprintf(p.profile_name, sizeof(p.profile_name), "%s",
                 /* 仅 APP 自定义名称下发 UI（UTF-8 中文）；无自定义时传空，
                  * UI 回退 "Conf%u"——内置 LV_SYMBOL 文本在 CKJGT 中文字体
                  * 下无法渲染，不能直接透传 */
                 config.isProfileNameCustom(p.profile_index)
                     ? config.getProfileDisplayName(p.profile_index)
                     : "");
        snprintf(p.profile_icon, sizeof(p.profile_icon), "%s",
                 LV_SYMBOL_SETTINGS);

        for (uint8_t i = 0; i < kMatrixKeyCount; ++i)
        {
            char label[20];
            formatKeymapLabel(resolver_.get(static_cast<uint8_t>(i + 1)),
                              fun_layer, label, sizeof(label));
            snprintf(p.keymap_labels[i], sizeof(p.keymap_labels[i]), "%u:%s",
                     static_cast<unsigned>(i + 1), label);
        }

        /*
         * 尾部 resync：外部切 profile 路径（cmd_config / 设置屏反向同步 /
         * 0x06 applyKeymap）全部汇聚到 keymap_ui_pending_ → 本函数，
         * 在此把预览索引对齐激活 profile，避免预览态与实际激活脱节。
         */
        g_preview_profile_index = p.profile_index;

        return postMessage(msg);
    }

    bool MainTask::sendKeymapProfilePreview(uint8_t fun_layer)
    {
        Configuration &config = Configuration::instance();

        /* 读目标 profile 键映射（仅内存展示，不应用不落盘） */
        /* KeymapArray 约 4.3KB，放静态存储避免 loopTask 栈溢出（同 cmd_keymap.cpp） */
        static Configuration::KeymapArray map{};
        if (!config.loadProfileKeyMapping(g_preview_profile_index, map))
        {
            /* 文件缺失：与 KeyResolver::begin() 相同的默认映射回退 */
            keymapFillDefaults(map);
        }

        DisplayMessage msg;
        msg.type = DisplayMessageType::KeymapProfile;
        KeymapProfileInfo &p = msg.keymap_profile;
        p.active_profile = config.activeProfile(); /* 真实激活值，≠ profile_index */
        p.profile_index = g_preview_profile_index;
        p.is_preview = true;

        snprintf(p.profile_name, sizeof(p.profile_name), "%s",
                 /* 同 sendKeymapProfileUi：仅 APP 自定义名称下发 UI */
                 config.isProfileNameCustom(p.profile_index)
                     ? config.getProfileDisplayName(p.profile_index)
                     : "");
        snprintf(p.profile_icon, sizeof(p.profile_icon), "%s",
                 LV_SYMBOL_SETTINGS);

        for (uint8_t i = 0; i < kMatrixKeyCount; ++i)
        {
            char label[20];
            formatKeymapLabel(map[static_cast<uint8_t>(i + 1)], fun_layer,
                              label, sizeof(label));
            snprintf(p.keymap_labels[i], sizeof(p.keymap_labels[i]), "%u:%s",
                     static_cast<unsigned>(i + 1), label);
        }

        return postMessage(msg);
    }

    void MainTask::pushCurrentKeymapView(uint8_t fun_layer)
    {
        if (ui_get_active_screen_tag() == UI_SCREEN_KEYMAPPED_SECONDARY)
        {
            /* 二级页推预览消息：applied 消息会把 UI 预览态冲掉 */
            if (!sendKeymapProfilePreview(fun_layer))
            {
                keymap_preview_ui_pending_ = true;
            }
        }
        else if (!sendKeymapProfileUi(fun_layer))
        {
            keymap_ui_pending_ = true;
        }
    }

    void MainTask::applyUiSettingsSnapshot(const ui_settings_snapshot_t &requested,
                                           bool persist)
    {
        ui_settings_snapshot_t s = requested;

        /* 与 parseConfigSetCommand 相同的取值约束（FEATURE_DOC §6） */
        s.work_mode = clampInt(s.work_mode, 0, 1);
        s.rgb_brightness = clampInt(s.rgb_brightness, 0, 100);
        s.tft_brightness = clampInt(s.tft_brightness, 5, 100);
        s.device_volume = clampInt(s.device_volume, 0, 100);
        s.active_keymap_profile = s.active_keymap_profile >= Configuration::CONFIG_PROFILE_COUNT
                                      ? 0
                                      : s.active_keymap_profile;
        s.rgb_single_color[sizeof(s.rgb_single_color) - 1] = '\0';
        const int rgbSingleColor = clampInt(atoi(s.rgb_single_color), 0, 255);

        Configuration &config = Configuration::instance();

        /* C6 修复：mutateSettings 入口前抓 prev，结束后取 curr，统一交给 applyUiSideEffects */
        DeviceSettings prev{};
        DeviceSettings curr{};
        config.snapshot(prev);
        curr = prev;

        bool changed = false;
        config.mutateSettings([&](DeviceSettings &d)
                              {
            if (d.work_mode != static_cast<uint8_t>(s.work_mode))
            {
                d.work_mode = static_cast<uint8_t>(s.work_mode);
                changed = true;
            }
            if (d.rgb_mode != static_cast<uint8_t>(s.rgb_mode))
            {
                d.rgb_mode = static_cast<uint8_t>(s.rgb_mode);
                changed = true;
            }
            if (d.rgb_single_color != static_cast<uint8_t>(rgbSingleColor))
            {
                d.rgb_single_color = static_cast<uint8_t>(rgbSingleColor);
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
            }
            if (d.ui_lang != static_cast<uint8_t>(s.ui_lang))
            {
                d.ui_lang = static_cast<uint8_t>(s.ui_lang);
                changed = true;
            } });

        if (!changed)
        {
            return;
        }
        config.snapshot(curr);

        LOG_INFO("MAIN", "ui settings %s (work_mode=%d tft=%d rgb=%d vol=%d)",
                 persist ? "save" : "apply",
                 static_cast<int>(s.work_mode),
                 static_cast<int>(s.tft_brightness),
                 static_cast<int>(s.rgb_brightness),
                 static_cast<int>(s.device_volume));

        /*
         * 持久化（saveSettings 内部自行加锁）。
         * apply（persist=false）只改内存，不写 INI。
         * 批量接口单次 config.ini 读/写覆盖全部键（2026-09-11 优化：
         * 原逐键 saveSetting 为 14 次「整文件读入+解析+原子写」，
         * SPIFFS 写放大严重且阻塞 MainTask 数秒）。
         */
        if (persist)
        {
            if (!config.saveSettings({
                    {"work_mode", static_cast<int>(s.work_mode)},
                    {"rgb_mode", static_cast<int>(s.rgb_mode)},
                    {"rgb_single_color", rgbSingleColor},
                    {"rgb_click_mode", static_cast<int>(s.rgb_click_mode)},
                    {"rgb_brightness", static_cast<int>(s.rgb_brightness)},
                    {"tft_theme", static_cast<int>(s.tft_theme)},
                    {"tft_brightness", static_cast<int>(s.tft_brightness)},
                    {"device_volume", static_cast<int>(s.device_volume)},
                    {"power_mode", static_cast<int>(s.power_mode)},
                    {"audio_enable", static_cast<int>(s.audio_enable)},
                    {"connect_host", static_cast<int>(s.connect_host)},
                    {"voice_enable", static_cast<int>(s.voice_enable)},
                    {"active_keymap_profile",
                     static_cast<int>(s.active_keymap_profile)},
                    {"ui_lang", static_cast<int>(s.ui_lang)},
                }))
            {
                LOG_ERROR("MAIN", "persist ui settings failed");
            }
        }

        /* C6 修复：副作用统一走 AppContext::applyUiSideEffects，与 cmd_config 共用 */
        AppContext::instance().applyUiSideEffects(prev, curr);

        /* 音量即时生效（Speaker::loop 与本任务同上下文） */
        Speaker::instance().applyDeviceVolume(curr.device_volume);

        DisplayMessage msg;
        msg.type = DisplayMessageType::SettingUpdate;
        fillSettingPayload(curr, msg.setting);
        postMessage(msg);
    }

    void MainTask::tick()
    {
        /*
         * 阶段 06 服务调度（每轮 loop() 调用，各模块内部自行节流）：
         * WiFi 重连状态机 / NTP 同步完成检测 / UDP 发现 / TCP 收发 /
         * 扬声器解码喂流 / ASR 录音攒流。
         */
        WiFiManager::instance().process();
        NtpSync::instance().process();
        DiscoveryService::instance().process();
        TcpChannel::instance().process();
        /* 音效板：消费 DisplayTask（Core 0）的播放/停止请求。必须先于
         * Speaker::loop——启停与喂流同任务串行访问 Audio 实例（INT WDT 修复） */
        AudioPad::instance().service();
        Speaker::instance().loop();
        /* 音效板：检测键位播放结束 → 清键位高亮 */
        AudioPad::instance().loop();
        VoiceRecognizer::instance().feedCapture();

        /* HA 状态聚合 → HA 屏 / 状态条（2.5s 节流，与参考工程一致） */
        const uint32_t now = millis();
        if ((now - last_ha_status_ms_) >= kHaStatusPeriodMs)
        {
            last_ha_status_ms_ = now;

            HaStatusInfo ha;
            NetDiagnostics::fillNetworkFields(ha);

            /* 键盘主机连接（BLE/USB HID）→ 主页连接图标，2.5s 节流刷新 */
            ha.kb_connected = (keyboard_ != nullptr) && keyboard_->isConnected();

            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            ha.work_mode = snap.work_mode;
            ha.voice_enabled = (snap.voice_enable != 0) && (snap.work_mode == 0);
            ha.voice_recording = VoiceRecognizer::instance().isCapturing();

            DisplayMessage msg;
            msg.type = DisplayMessageType::HaStatus;
            msg.ha_status = ha;
            postMessage(msg);
        }

        /* 电池电量 → 状态条（5s 节流，避免每次 loop 都做 ADC 采样） */
        if ((now - last_battery_status_ms_) >= kBatteryStatusPeriodMs)
        {
            last_battery_status_ms_ = now;
            const uint8_t pct = BatteryMonitor::instance().readPercent();
            DisplayMessage msg;
            msg.type = DisplayMessageType::BatteryStatus;
            msg.battery_percent = pct;
            postMessage(msg);
        }
    }

} // namespace ekeys
