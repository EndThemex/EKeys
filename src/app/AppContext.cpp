/*
 * AppContext.cpp
 *
 * 阶段 03 启动顺序（SPIFFS 由 main.cpp 提前挂载）：
 *   1) 创建 KeymapRepository 并注入 Configuration 单例
 *   2) 键盘后端（USB CDC 等）
 *   3) MainTask.begin()（内部 Configuration::load() → resolver 加载键映射）
 *   4) DisplayTask.begin() / ui_minimal::create()
 */

#include "AppContext.h"

#include "config/Configuration.h"
#include "logging/LogManager.h"
#include "output/KeyboardFactory.h"
#include "services/KeymapRepository.h"
#include "tasks/DisplayTask.h"
#include "ui/ui_minimal.h"

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

        setKeyboard(KeyboardFactory::create(WorkMode::Wired));

        main_task_.begin();

        /*
         * 先启动 DisplayTask，让它内部创建好队列；
         * 再 create 主屏（LVGL 在 LvglPort::init() 中已经可用）。
         * 然后把队列句柄注入 MainTask。
         */
        DisplayTask::instance().begin();

        ui_minimal::create();

        main_task_.setKeyboard(keyboard());
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

} // namespace ekeys
