/*
 * AppContext.cpp
 */

#include "AppContext.h"

#include "logging/LogManager.h"
#include "output/KeyboardFactory.h"

namespace ekeys {

AppContext &AppContext::instance()
{
    static AppContext inst;
    return inst;
}

void AppContext::init()
{
    setKeyboard(KeyboardFactory::create(WorkMode::Wired));
    main_task_.begin();
    LOG_INFO("APP", "AppContext initialized");
}

void AppContext::shutdown()
{
    main_task_.end();
    keyboard_.reset();
}

}  // namespace ekeys
