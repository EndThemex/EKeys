/*
 * DisplayTask.cpp
 */

#include "DisplayTask.h"

#include "display/LvglPort.h"
#include "logging/LogManager.h"
#include "message_types.h"
#include "ui/ui_minimal.h"

namespace ekeys {

namespace {

constexpr uint32_t kDisplayTaskStackDepth   = 8192;
constexpr uint8_t  kDisplayTaskPriority     = 1;
constexpr UBaseType_t kDisplayMessageQueueLen = 10;
constexpr TickType_t  kDisplayMessageBlockTicks = pdMS_TO_TICKS(50);

}  // namespace

DisplayTask::DisplayTask()
    : queue_(nullptr)
{
}

DisplayTask &DisplayTask::instance()
{
    static DisplayTask inst;
    return inst;
}

void DisplayTask::begin()
{
    if (queue_ != nullptr) {
        return;
    }
    queue_ = xQueueCreate(kDisplayMessageQueueLen, sizeof(DisplayMessage));
    if (queue_ == nullptr) {
        LOG_ERROR("DISP", "xQueueCreate failed");
        return;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        &DisplayTask::taskEntry,
        "DisplayTask",
        kDisplayTaskStackDepth,
        this,
        kDisplayTaskPriority,
        nullptr,
        0); /* Core 0 */

    if (ok != pdPASS) {
        LOG_ERROR("DISP", "xTaskCreatePinnedToCore failed");
    } else {
        LOG_INFO("DISP", "DisplayTask started on core 0");
    }
}

BaseType_t DisplayTask::post(const DisplayMessage &msg, TickType_t wait_ticks)
{
    if (queue_ == nullptr) {
        return pdFALSE;
    }
    return xQueueSend(queue_, &msg, wait_ticks);
}

void DisplayTask::taskEntry(void *arg)
{
    auto *self = static_cast<DisplayTask *>(arg);
    self->run();
    vTaskDelete(nullptr);
}

void DisplayTask::run()
{
    uint32_t last = millis();
    DisplayMessage msg;

    for (;;) {
        uint32_t now = millis();
        if (now != last) {
            LvglPort::instance().tick(now - last);
            last = now;
        }

        if (xQueueReceive(queue_, &msg, kDisplayMessageBlockTicks) == pdTRUE) {
            switch (msg.type) {
                case DisplayMessageType::TimeUpdate:
                    ui_minimal::setTimeLabel(msg.payload.time_text);
                    break;
                case DisplayMessageType::SettingUpdate:
                    /* 阶段 02 占位：阶段 03/04 由 ui_helpers 接管 */
                    break;
                default:
                    break;
            }
        }
    }
}

}  // namespace ekeys
