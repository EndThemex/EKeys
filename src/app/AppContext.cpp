/*
 * AppContext.cpp
 *
 * 阶段 03 启动顺序（SPIFFS 由 main.cpp 提前挂载）：
 *   1) 创建 KeymapRepository 并注入 Configuration 单例
 *   2) 键盘后端（USB CDC 等）
 *   3) MainTask.begin()（内部 Configuration::load() → resolver 加载键映射）
 *   4) DisplayTask.begin()（run() 内 ui_init() 创建 11 屏）
 */

#include "AppContext.h"

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
        /* 配置层：先于 MainTask 准备好，MainTask::begin() 中 Configuration::load() */
        keymap_repo_ = std::make_unique<KeymapRepository>();
        configuration_ = &Configuration::instance();
        configuration_->setRepository(keymap_repo_.get());

        main_task_.begin(); // 内部 Configuration::load()

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

        /*
         * 协议层（阶段 04）：注册命令 handler 并启动 CDC JSON 行收发。
         * registration 在 main_task_.begin() 之后统一执行（FEATURE_DOC §5.4）。
         */
        SerialProtocol::instance().begin();
        protocol::registration::registerAllCommandHandlers();

        /*
         * 启动 DisplayTask：其 run() 内部调用 ui_init() 创建 11 屏
         * （SquareLine 生成的 src/ui），并把队列句柄注入 MainTask。
         */
        DisplayTask::instance().begin();

        /* F1 修复：键盘已按 work_mode 创建；DisplayTask 仅补注入队列句柄 */
        main_task_.setDisplayQueue(DisplayTask::instance().queueHandle());

        LOG_INFO("APP", "AppContext initialized");
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
