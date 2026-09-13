/*
 * AppContext.cpp
 *
 * 阶段 03 启动顺序（SPIFFS 由 main.cpp 提前挂载）：
 *   1) 创建 KeymapRepository 并注入 Configuration 单例
 *   2) AudioPad::load + Configuration::load（DisplayTask 启动快照的前置依赖）
 *   3) DisplayTask.begin()（run() 内 ui_init() 创建 13 屏，Core 0 并行渲染）
 *   4) MainTask.begin()（键映射 / 外设 / WiFi，与 ui_init 并行）
 *   5) 键盘后端（USB CDC / BLE，BLE init 阻塞期间 UI 已在渲染）
 *   6) 协议层注册（含 SPIFFS 残留文件清理，与 ui_init 并行）
 */

#include "AppContext.h"

#include <Arduino.h> // millis() 启动打点

#include "audio/AudioPad.h"
#include "config/Configuration.h"
#include "logging/LogManager.h"
#include "network/WiFiManager.h"
#include "output/KeyboardFactory.h"
#include "protocol/SerialProtocol.h"
#include "protocol/registration.h"
#include "services/KeymapRepository.h"
#include "tasks/DisplayTask.h"
#include "ui/ui_settings_types.h"

namespace ekeys
{

    AppContext &AppContext::instance()
    {
        static AppContext inst;
        return inst;
    }

    void AppContext::init()
    {
        /* 配置层：先于其它模块准备好 */
        keymap_repo_ = std::make_unique<KeymapRepository>();
        configuration_ = &Configuration::instance();
        configuration_->setRepository(keymap_repo_.get());

        /* 音效板：先于 MainTask 加载 /audio_pad.ini 绑定表（SPIFFS 已挂载） */
        AudioPad::instance().load();
        LOG_INFO("APP", "t=%lu ms audiopad loaded", (unsigned long)millis());

        /* 配置加载提前（原在 MainTask::begin 内）：DisplayTask 启动快照
         * 需要真实配置，是启动 DisplayTask 的前置依赖。 */
        configuration_->load();
        LOG_INFO("APP", "t=%lu ms config loaded", (unsigned long)millis());

        /*
         * 启动提速（2026-09-13 黑屏优化）：
         * DisplayTask（Core 0）在 MainTask 之前启动——ui_init() 建 13 屏
         * （实测 ~723ms）与 Core 1 上 MainTask::begin 的键映射 / 外设 /
         * WiFi 初始化（~1.5s）、键盘后端（BLE 模式阻塞 1~3s）、协议注册
         * （含 SPIFFS 全目录清理 ~0.5s）全部并行，主页显示不再等它们。
         * 依赖检查：此刻 SPIFFS 已挂载、Configuration::load 已完成
         * （run() 启动快照读配置）、AudioPad 已 load。
         */
        DisplayTask::instance().begin();
        main_task_.setDisplayQueue(DisplayTask::instance().queueHandle());
        LOG_INFO("APP", "t=%lu ms display task started (ui building on core 0)",
                 (unsigned long)millis());

        main_task_.begin(); // Configuration::load 已提前完成
        LOG_INFO("APP", "t=%lu ms main task ready", (unsigned long)millis());

        /* F1 修复：按加载后的 work_mode 选择键盘后端，
         * 避免 BLE/2.4G 模式下重启仍为 USB。 */
        {
            DeviceSettings snap;
            configuration_->snapshot(snap);
            WorkMode wm = WorkMode::Wired;
            if (snap.work_mode == static_cast<uint8_t>(WorkMode::Bluetooth))
            {
                wm = WorkMode::Bluetooth;
            }
            else if (snap.work_mode ==
                     static_cast<uint8_t>(WorkMode::Wireless24G))
            {
                wm = WorkMode::Wireless24G;
            }
            setKeyboard(KeyboardFactory::create(wm));
            main_task_.setKeyboard(keyboard());
        }
        /* BLE 模式下 KeyboardFactory::create 内 BLEDevice::init 阻塞 1~3s，
         * 此打点用于确认其已被 UI 并行覆盖（黑屏窗口之外）。 */
        LOG_INFO("APP", "t=%lu ms keyboard backend ready", (unsigned long)millis());

        /*
         * 协议层（阶段 04）：注册命令 handler 并启动 CDC JSON 行收发。
         * registration 在 main_task_.begin() 之后统一执行（FEATURE_DOC §5.4）。
         */
        SerialProtocol::instance().begin();
        protocol::registration::registerAllCommandHandlers();

        LOG_INFO("APP", "AppContext initialized at %lu ms", (unsigned long)millis());
    }

    void AppContext::shutdown()
    {
        main_task_.end();
        keyboard_.reset();
        keymap_repo_.reset();
        configuration_ = nullptr;
    }

    void AppContext::applyWorkMode(uint8_t mode)
    {
        WorkMode wm = WorkMode::Wired;
        if (mode == static_cast<uint8_t>(WorkMode::Bluetooth))
        {
            wm = WorkMode::Bluetooth;
        }
        else if (mode == static_cast<uint8_t>(WorkMode::Wireless24G))
        {
            wm = WorkMode::Wireless24G;
        }

        /*
         * 切到 BLE 前先关停 WiFi：此时 Configuration 中 work_mode 已是新值，
         * scheduleConnect 内部因 isEnabled()==false（BLE 模式）走 stopReconnect
         * （WiFi.mode OFF），避免 WiFi 栈占堆导致 BLE init 失败回退 USB。
         * 开机路径不经本函数（init() 直接 create），无影响。
         */
        if (wm == WorkMode::Bluetooth)
        {
            WiFiManager::instance().scheduleConnect();
        }

        LOG_INFO("APP", "work_mode -> %u, recreating keyboard", mode);
        keyboard_.reset(); // 释放旧实例（回收 HID / BLE 资源）
        setKeyboard(KeyboardFactory::create(wm));
        main_task_.setKeyboard(keyboard_.get());
    }

    /*
     * C6 修复：cmd_config 与 MainTask::applyUiSettingsSnapshot 副作用去重。
     * 调用方在 Configuration 已落定后，传入 prev / curr 两份快照：
     *   - work_mode 变化 → 重建键盘实例 + 调度 WiFi
     *   - active_keymap_profile 变化 → 重载键映射
     *   - 任意配置变更但无线 WiFi 关键字段变化 → 仍调度一次 WiFi 重连
     */
    void AppContext::applyUiSideEffects(const DeviceSettings &prev, const DeviceSettings &curr)
    {
        const bool workModeChanged = (prev.work_mode != curr.work_mode);
        const bool profileChanged =
            (prev.active_keymap_profile != curr.active_keymap_profile);
        const bool wifiRelevantChanged =
            workModeChanged ||
            (prev.wifi_switch != curr.wifi_switch) ||
            (strcmp(prev.wifi_ssid, curr.wifi_ssid) != 0) ||
            (strcmp(prev.wifi_password, curr.wifi_password) != 0);

        if (workModeChanged)
        {
            applyWorkMode(curr.work_mode);
        }
        if (profileChanged)
        {
            main_task_.reloadKeymap();
        }
        if (wifiRelevantChanged)
        {
            WiFiManager::instance().scheduleConnect();
        }
    }

} // namespace ekeys
