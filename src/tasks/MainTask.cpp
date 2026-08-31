/*
 * MainTask.cpp
 *
 * 阶段 01：5ms tick → MatrixScanner::scan → resolve edges → IKeyboard。
 */

#include "MainTask.h"

#include "logging/LogManager.h"
#include "output/IKeyboard.h"

namespace ekeys {

constexpr uint32_t kMainTaskTickPeriodMs = 5;

MainTask::MainTask()
    : keyboard_(nullptr), last_tick_ms_(0)
{
}

void MainTask::begin()
{
    scanner_.begin();
    resolver_.begin();
    last_tick_ms_ = millis();
    LOG_INFO("MAIN", "MainTask started");
}

void MainTask::end()
{
    resolver_.end();
}

void MainTask::loop()
{
    if (keyboard_ == nullptr) {
        return;
    }
    uint32_t now = millis();
    if ((now - last_tick_ms_) < kMainTaskTickPeriodMs) {
        return;
    }
    last_tick_ms_ = now;

    scanner_.scan();

    uint8_t pressed[kMatrixKeyCount];
    uint8_t released[kMatrixKeyCount];
    uint8_t pc = 0;
    uint8_t rc = 0;
    scanner_.getPressedKeys(pressed, pc);
    scanner_.getReleasedKeys(released, rc);

    for (uint8_t i = 0; i < pc; ++i) {
        resolver_.press(pressed[i], *keyboard_);
    }
    for (uint8_t i = 0; i < rc; ++i) {
        resolver_.release(released[i], *keyboard_);
    }
}

void MainTask::tick()
{
    /*
     * 阶段 01 暂留空。后续阶段（06 等）在此调度 WiFi / BLE / ASR。
     */
}

}  // namespace ekeys
