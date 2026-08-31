/*
 * main.cpp
 *
 * 阶段 01 入口：
 *
 *     Serial.begin → 背光 → NV3007 begin → fill black
 *                → LVGL init
 *                → KeyboardFactory::create(USBKeyboardImpl)
 *                → AppContext::init() → MainTask::loop()
 *
 * 屏幕示例主屏已移除（详见 docs/01-minimal-hid.md 变更记录），
 * 阶段 02 由 DisplayTask 接管 LVGL tick。
 */

#include <Arduino.h>

#include "app/AppContext.h"
#include "display/Backlight.h"
#include "display/DisplayDriver.h"
#include "display/LvglPort.h"
#include "logging/LogManager.h"

void setup()
{
    Serial.begin(115200);
    delay(200);

    LOG_INFO("MAIN", "===== EKeys boot (stage 01) =====");

    ekeys::Backlight backlight;
    backlight.begin();

    if (!ekeys::DisplayDriver::instance().begin(10000000)) {
        LOG_ERROR("MAIN", "NV3007 init failed");
        while (true) {
            delay(1000);
        }
    }
    ekeys::DisplayDriver::instance().fillScreen(RGB565_BLACK);

    ekeys::LvglPort::instance().init();

    auto &ctx = ekeys::AppContext::instance();
    ctx.init();
    ctx.mainTask().setKeyboard(ctx.keyboard());

    LOG_INFO("MAIN", "setup completed");
}

void loop()
{
    static uint32_t last = millis();
    uint32_t now = millis();
    if (now != last) {
        ekeys::LvglPort::instance().tick(now - last);
        last = now;
    }

    ekeys::AppContext::instance().mainTask().loop();
    delay(5);
}
