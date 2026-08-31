/*
 * main.cpp
 *
 * 阶段 03 入口：
 *
 *     Serial.begin → 背光 → NV3007 begin → fill black
 *                → LVGL init
 *                → SPIFFS 挂载（失败则 LOG_ERROR 死循环）
 *                → AppContext::init()（配置加载 / MainTask / DisplayTask / ui_minimal）
 *                → loop() 中只跑 MainTask::loop()（DisplayTask 接管 LVGL tick）
 *
 * 屏幕驱动代码在 src/display/ 与 src/ui/，配置层在 src/config/ 与 src/services/。
 */

#include <Arduino.h>

#include "app/AppContext.h"
#include "display/Backlight.h"
#include "display/DisplayDriver.h"
#include "display/LvglPort.h"
#include "logging/LogManager.h"
#include "services/ConfigStore.h"

void setup()
{
    Serial.begin(115200);
    delay(200);

    LOG_INFO("MAIN", "===== EKeys boot (stage 03) =====");

    ekeys::Backlight backlight;
    backlight.begin();

    if (!ekeys::DisplayDriver::instance().begin(10000000))
    {
        LOG_ERROR("MAIN", "NV3007 init failed");
        while (true)
        {
            delay(1000);
        }
    }
    ekeys::DisplayDriver::instance().fillScreen(RGB565_BLACK);

    ekeys::LvglPort::instance().init();

    /*
     * SPIFFS 必须先于 AppContext::init()：
     * MainTask::begin() 里 Configuration::load() / resolver 加载键映射
     * 都依赖文件系统已就绪。
     */
    if (!ekeys::ConfigStore::mount())
    {
        LOG_ERROR("MAIN", "SPIFFS mount failed permanently");
        while (true)
        {
            delay(1000);
        }
    }

    ekeys::AppContext::instance().init();

    LOG_INFO("MAIN", "setup completed");
}

void loop()
{
    /*
     * DisplayTask (Core 0) 内部周期调用 LvglPort::tick() 与
     * 消费 DisplayMessage 队列。本函数只需驱动 MainTask 的
     * 5ms 扫描循环。
     */
    ekeys::AppContext::instance().mainTask().loop();
    delay(5);
}
