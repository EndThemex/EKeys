/*
 * MainTask.cpp
 *
 * 阶段 02：
 *   - 5ms tick：MatrixScanner::scan() → KeyResolver → IKeyboard
 *   - 1s  tick：向 DisplayTask 队列投递 TIME_UPDATE（millis() 推算的 HH:MM:SS）
 *
 * 时间源为 millis() 小时数；阶段 06 替换为 NTP 时间。
 */

#include "MainTask.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdio.h>

#include "logging/LogManager.h"
#include "config/Configuration.h"
#include "message_types.h"
#include "output/IKeyboard.h"

namespace ekeys
{

    namespace
    {

        constexpr uint32_t kMainTaskTickPeriodMs = 5;
        constexpr uint32_t kMainTaskTimePostPeriodMs = 1000;

        /*
         * 把 millis() 换算成 "HH:MM:SS"。不依赖 NTP；阶段 06 替换为
         * NtpSync::epochSeconds() 等接口。
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

    } // namespace

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
        /* 加载 /config.ini（文件缺失时使用默认值），再加载键映射 */
        Configuration::instance().load();

        scanner_.begin();
        resolver_.begin();
        last_tick_ms_ = millis();
        last_time_post_ms_ = last_tick_ms_;
        LOG_INFO("MAIN", "MainTask started");
    }

    void MainTask::end()
    {
        resolver_.end();
    }

    void MainTask::loop()
    {
        if (keyboard_ == nullptr)
        {
            return;
        }
        uint32_t now = millis();

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

            for (uint8_t i = 0; i < pc; ++i)
            {
                resolver_.press(pressed[i], *keyboard_);
            }
            for (uint8_t i = 0; i < rc; ++i)
            {
                resolver_.release(released[i], *keyboard_);
            }
        }

        /* 1s tick：投递 TIME_UPDATE */
        if (display_queue_ != nullptr &&
            (now - last_time_post_ms_) >= kMainTaskTimePostPeriodMs)
        {
            last_time_post_ms_ = now;
            DisplayMessage msg;
            msg.type = DisplayMessageType::TimeUpdate;
            formatUptimeString(msg.payload.time_text,
                               sizeof(msg.payload.time_text), now);
            xQueueSend(static_cast<QueueHandle_t>(display_queue_),
                       &msg, 0);
        }
    }

    void MainTask::tick()
    {
        /*
         * 阶段 06 后会在此处调度 WiFi / BLE / ASR 等异步子模块。
         */
    }

} // namespace ekeys
