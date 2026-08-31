/*
 * Mic.cpp
 *
 * 见 Mic.h。
 */

#include "Mic.h"

#include <Arduino.h>
#include <driver/i2s.h>

#include "hardware/PinMap.h"
#include "logging/LogManager.h"

namespace ekeys {

Mic &Mic::instance()
{
    static Mic inst;
    return inst;
}

bool Mic::begin()
{
    if (inited_)
    {
        return true;
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
        return false;
    }

    i2s_pin_config_t pins = {};
    pins.bck_io_num = kPinI2sMicBclk;   // IO13（专用 MIC_SCK）
    pins.ws_io_num = kPinI2sMicWs;      // IO12
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = kPinI2sMicDin;   // IO11

    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK)
    {
        LOG_ERROR("MIC", "i2s_set_pin failed");
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    inited_ = true;
    LOG_INFO("MIC", "ICS43434 ready (sck=%u ws=%u din=%u, %ukHz)",
             kPinI2sMicBclk, kPinI2sMicWs, kPinI2sMicDin,
             static_cast<unsigned>(kSampleRate / 1000));
    return true;
}

void Mic::end()
{
    if (!inited_)
    {
        return;
    }
    i2s_driver_uninstall(I2S_NUM_0);
    inited_ = false;
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
    if (i2s_read(I2S_NUM_0, out, want_bytes, &bytes_read, portMAX_DELAY) != ESP_OK)
    {
        return 0;
    }
    return bytes_read / sizeof(int16_t);
}

}  // namespace ekeys
