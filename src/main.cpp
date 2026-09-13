/*
 * main.cpp
 *
 * 启动顺序：
 *     Serial.begin → 背光 → NV3007 begin → fill black
 *                → LVGL init
 *                → SPIFFS 挂载（失败则 LOG_ERROR 死循环）
 *                → AppContext::init()（配置加载 / MainTask / DisplayTask；
 *                  阶段 06 服务（WiFi/NTP/TCP/扬声器）由 MainTask::begin() 初始化）
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
    /*
     * TinyUSB CDC begin 不阻塞，host 未打开端口（DTR）前日志本就会被
     * 丢弃，无需 delay 等 CDC 就绪（原 200ms 纯开机浪费，2026-09-13 提速移除）。
     */
    Serial.begin(115200);

    LOG_INFO("MAIN", "===== EKeys boot (stage 06) t=%lu ms =====", (unsigned long)millis());

    ekeys::Backlight::instance().begin();
    /* 黑屏起点锚点：背光已亮、面板尚未初始化 */
    LOG_INFO("MAIN", "t=%lu ms backlight on (black screen starts)", (unsigned long)millis());

    if (!ekeys::DisplayDriver::instance().begin(40000000))
    {
        LOG_ERROR("MAIN", "NV3007 init failed");
        while (true)
        {
            delay(1000);
        }
    }
    ekeys::DisplayDriver::instance().fillScreen(RGB565_BLACK);
    LOG_INFO("MAIN", "t=%lu ms panel ready", (unsigned long)millis());

    ekeys::LvglPort::instance().init();
    LOG_INFO("MAIN", "t=%lu ms lvgl port ready", (unsigned long)millis());

    /*
     * 开机画面（黑底 + 橙色大字 EKeys）：面板就绪后立即同步渲染上屏，
     * 覆盖后续 SPIFFS 挂载 / 配置加载 / ui_init 建屏期间的黑屏窗口。
     * 主 UI 建好后由 DisplayTask::run() 调 clearSplash() 销毁回收。
     */
    ekeys::LvglPort::instance().showSplash();
    LOG_INFO("MAIN", "t=%lu ms splash shown", (unsigned long)millis());

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

    LOG_INFO("MAIN", "t=%lu ms spiffs mounted", (unsigned long)millis());

    ekeys::AppContext::instance().init();

    LOG_INFO("MAIN", "setup completed at %lu ms", (unsigned long)millis());
}

void loop()
{
    /*
     * DisplayTask (Core 0) 内部周期调用 LvglPort::tick() 与
     * 消费 DisplayMessage 队列。本函数只需驱动 MainTask 的
     * 5ms 扫描循环。
     *
     * D2 修复：MainTask::loop() 内部已用 millis() 节流（5ms tick），
     * 外层 delay(5) 让实际周期变成 10ms，浪费调度时间。
     * 改为 delay(1) 让 FreeRTOS 调度器更频繁地切换到 WiFi/DisplayTask。
     */
    ekeys::AppContext::instance().mainTask().loop();
    delay(1);
}
