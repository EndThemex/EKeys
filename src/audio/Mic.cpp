/*
 * Mic.cpp
 *
 * 见 Mic.h。
 */

#include "Mic.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/i2s.h>

#include "hardware/PinMap.h"
#include "logging/LogManager.h"

namespace ekeys {

    namespace
    {
        /* F4 修复：Mic / I2S0 全局互斥信号量，跨任务（MainTask + DisplayTask）
         * 共享同一硬件，避免 begin/end 重叠造成 i2s_driver_install 冲突。 */
        SemaphoreHandle_t s_mic_mutex = nullptr;

        SemaphoreHandle_t micMutex()
        {
            if (s_mic_mutex == nullptr)
            {
                s_mic_mutex = xSemaphoreCreateBinary();
                if (s_mic_mutex != nullptr)
                {
                    xSemaphoreGive(s_mic_mutex);
                }
            }
            return s_mic_mutex;
        }
    } // namespace

Mic &Mic::instance()
{
    static Mic inst;
    return inst;
}

bool Mic::take()
{
    SemaphoreHandle_t m = micMutex();
    if (m == nullptr)
    {
        return false;
    }
    /* 短超时；外部已经串行化（prepareI2sForMicCapture/频谱串行） */
    return xSemaphoreTake(m, pdMS_TO_TICKS(2000)) == pdTRUE;
}

void Mic::give()
{
    SemaphoreHandle_t m = micMutex();
    if (m != nullptr)
    {
        xSemaphoreGive(m);
    }
}

bool Mic::begin()
{
    if (inited_)
    {
        return true;
    }
    /* F4 修复：取互斥量保护 i2s_driver_install；install 期间不允许其他任务 end */
    if (!take())
    {
        LOG_ERROR("MIC", "mic mutex take failed");
        return false;
    }

    i2s_config_t cfg = {};
    cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = kSampleRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = static_cast<int>(kDmaSamples);
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = false;
    cfg.fixed_mclk = 0;

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK)
    {
        LOG_ERROR("MIC", "i2s_driver_install failed");
        give();
        return false;
    }

    i2s_pin_config_t pins = {};
    /* D9 修复：IO10 与 Speaker BCLK 共用，运行时由 prepareI2sForMicCapture() 互斥。
     * 旧注释"专用 MIC_SCK"语义错误，易让维护者误以为两条 BCLK 独立。 */
    pins.bck_io_num = kPinI2sMicBclk;   // IO10（与 Speaker BCLK 共用，运行时互斥）
    pins.ws_io_num = kPinI2sMicWs;      // IO12
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = kPinI2sMicDin;   // IO11

    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK)
    {
        LOG_ERROR("MIC", "i2s_set_pin failed");
        i2s_driver_uninstall(I2S_NUM_0);
        give();
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    inited_ = true;
    LOG_INFO("MIC", "ICS43434 ready (sck=%u ws=%u din=%u, %ukHz)",
             kPinI2sMicBclk, kPinI2sMicWs, kPinI2sMicDin,
             static_cast<unsigned>(kSampleRate / 1000));
    /* F4：保持持锁直至 end()；调用方连续 read 不需要反复 take。 */
    return true;
}

void Mic::end()
{
    if (!inited_)
    {
        /* 没拿到互斥也要 release，避免对端 leak */
        give();
        return;
    }
    i2s_driver_uninstall(I2S_NUM_0);
    inited_ = false;
    give();
    LOG_INFO("MIC", "stopped");
}

size_t Mic::Read(int16_t *out, size_t max_samples)
{
    if (!inited_ || out == nullptr || max_samples == 0)
    {
        return 0;
    }
    size_t bytes_read = 0;
    const size_t want_bytes = max_samples * sizeof(int16_t);
    /*
     * B1 修复：DMA 一帧约 32ms（512 样本 / 16kHz），用 portMAX_DELAY
     * 会阻塞 MainTask::loop() 整个 tick，导致按键扫描 / TCP 心跳停摆。
     * 改用 20ms 超时：未填满返回 0，下次 loop 再读，避免长时间阻塞。
     */
    if (i2s_read(I2S_NUM_0, out, want_bytes, &bytes_read,
                 pdMS_TO_TICKS(20)) != ESP_OK)
    {
        return 0;
    }
    return bytes_read / sizeof(int16_t);
}

}  // namespace ekeys
