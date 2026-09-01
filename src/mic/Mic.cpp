#include "Mic.h"

namespace ekeys
{

    Mic::Mic() = default;

    Mic::~Mic()
    {
        end();
    }

    bool Mic::begin()
    {
        if (initialized_)
            return true;

        i2s_config_ = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = MIC_SAMPLE_RATE,
            .bits_per_sample = (i2s_bits_per_sample_t)MIC_BITS_PER_SAMPLE,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = MIC_BUFFER_SAMPLES / 4,
            .use_apll = false,
            .tx_desc_auto_clear = false,
            .fixed_mclk = 0};

        pin_config_ = {
            .bck_io_num = MIC_I2S_BCLK,
            .ws_io_num = MIC_I2S_WS,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = MIC_I2S_DATA};

        esp_err_t err = i2s_driver_install(MIC_I2S_NUM, &i2s_config_, 0, nullptr);
        if (err != ESP_OK)
            return false;

        err = i2s_set_pin(MIC_I2S_NUM, &pin_config_);
        initialized_ = (err == ESP_OK);
        return initialized_;
    }

    size_t Mic::read(int32_t *buffer, size_t samples_count)
    {
        if (!initialized_ || buffer == nullptr || samples_count == 0)
            return 0;

        size_t bytes_read = 0;
        i2s_read(MIC_I2S_NUM,
                 buffer,
                 samples_count * sizeof(int32_t),
                 &bytes_read,
                 portMAX_DELAY);
        return bytes_read / sizeof(int32_t);
    }

    void Mic::end()
    {
        if (initialized_)
        {
            i2s_driver_uninstall(MIC_I2S_NUM);
            initialized_ = false;
        }
    }

    bool Mic::reset()
    {
        initialized_ = false;
        i2s_driver_uninstall(MIC_I2S_NUM);
        i2s_zero_dma_buffer(MIC_I2S_NUM);
        delay(20);

        esp_err_t err = i2s_driver_install(MIC_I2S_NUM, &i2s_config_, 0, nullptr);
        if (err != ESP_OK)
            return false;
        err = i2s_set_pin(MIC_I2S_NUM, &pin_config_);
        initialized_ = (err == ESP_OK);
        return initialized_;
    }

} // namespace ekeys