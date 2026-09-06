/*
 * TestSpeaker.cpp
 *
 * T5 喇叭测试实现。
 *
 * I2S_NUM_1 (TX) 主模式：
 *   BCLK   = IO10 (kPinI2sBclkSpeaker)
 *   LRCLK  = IO9  (kPinI2sLrclkSpeaker)
 *   DIN    = IO11 (kPinI2sDataSpeaker，主控输出 → 功放输入)
 *
 * 16kHz 16bit 立体声；左右声道复制同一正弦样本。
 * 播放时大字显示频率/幅度 + 8 步序列指示条 + 实时进度条。
 */

#include "TestSpeaker.h"

#include <Arduino_GFX_Library.h>
#include <driver/i2s.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "HwTestMenu.h"
#include "HwTestUI.h"
#include "display/DisplayDriver.h"
#include "hardware/PinMap.h"
#include "logging/LogManager.h"

namespace ekeys
{
    namespace hwtest
    {

        constexpr uint32_t kSampleRate = 16000;
        constexpr uint16_t kBufSamples = 1024; // 1 帧 1024 样本 = 64ms

        // 音调阶段 (Hz, duration_ms)
        struct ToneStep
        {
            uint16_t freq;
            uint16_t duration_ms;
            uint16_t amplitude_pct; // 25/50/75/100
            const char *label;
        };

        static const ToneStep kSteps[] = {
            {440, 1000, 50, "440 Hz"},
            {1000, 1000, 50, "1 kHz"},
            {2000, 1000, 50, "2 kHz"},
            {4000, 1000, 50, "4 kHz"},
            // volume ladder at 1 kHz
            {1000, 1000, 25, "1k 25%"},
            {1000, 1000, 50, "1k 50%"},
            {1000, 1000, 75, "1k 75%"},
            {1000, 1000, 100, "1k 100%"},
        };
        constexpr uint8_t kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);

        struct TestSpeaker : public TestBase
        {
            Arduino_GFX *gfx = nullptr;
            int16_t *stereo_buf = nullptr;
            uint32_t phase_pos = 0; // sample position (mono)
            bool inited = false;
            uint8_t step_idx = 0;
            uint32_t step_started_ms = 0;
            bool running = false;
            uint32_t last_draw_ms = 0; // 进度条重绘节流

            const char *title() const override { return "T5 Speaker"; }

            void enter() override
            {
                gfx = DisplayDriver::instance().gfx();
                step_idx = 0;
                running = false;
                if (stereo_buf == nullptr)
                {
                    stereo_buf = (int16_t *)ps_malloc(kBufSamples * 2 * sizeof(int16_t));
                    LOG_INFO("T5", "stereo buf %u bytes %s",
                             (unsigned)(kBufSamples * 2 * sizeof(int16_t)),
                             stereo_buf ? "ok" : "NULL");
                }
                drawIdle();
                LOG_INFO("T5", "enter");
            }

            void loop(uint8_t key) override
            {
                if (key == ROT_ENTER)
                {
                    if (!running)
                    {
                        start();
                    }
                    else
                    {
                        // 跳过当前 step
                        step_started_ms = millis() - kSteps[step_idx].duration_ms;
                    }
                    return;
                }
                if (!running)
                    return;

                uint32_t now = millis();
                uint32_t step_age = now - step_started_ms;
                const ToneStep &s = kSteps[step_idx];
                if (step_age >= s.duration_ms)
                {
                    step_idx++;
                    if (step_idx >= kStepCount)
                    {
                        LOG_INFO("T5", "sequence done");
                        stop();
                        drawDone();
                        return;
                    }
                    phase_pos = 0;
                    step_started_ms = now;
                    last_draw_ms = 0;
                    drawStep();
                }
                // 喂一帧
                fillBuffer(kSteps[step_idx]);
                size_t written = 0;
                i2s_write(I2S_NUM_1, stereo_buf,
                          kBufSamples * 2 * sizeof(int16_t),
                          &written, pdMS_TO_TICKS(50));
                // 播放中周期重画进度（节流 200ms）
                if (now - last_draw_ms >= 200)
                {
                    last_draw_ms = now;
                    drawStep();
                }
            }

            void exit() override
            {
                LOG_INFO("T5", "exit");
                stop();
                if (stereo_buf != nullptr)
                {
                    free(stereo_buf);
                    stereo_buf = nullptr;
                }
                gfx = nullptr;
            }

            void start()
            {
                if (stereo_buf == nullptr)
                {
                    LOG_ERROR("T5", "no buf");
                    return;
                }
                /* 保险：卸载可能残留的 I2S0 驱动（如 T4 未正常退出），
                 * 确保喇叭从干净状态开始 */
                i2s_driver_uninstall(I2S_NUM_0);
                if (!initI2S())
                {
                    LOG_ERROR("T5", "i2s init failed");
                    return;
                }
                running = true;
                step_idx = 0;
                phase_pos = 0;
                step_started_ms = millis();
                drawHeader("playing");
                drawStep();
            }

            void stop()
            {
                running = false;
                if (inited)
                {
                    i2s_driver_uninstall(I2S_NUM_1);
                    inited = false;
                }
            }

            bool initI2S()
            {
                i2s_config_t cfg = {};
                cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
                cfg.sample_rate = kSampleRate;
                cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
                cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
                cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
                cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
                cfg.dma_buf_count = 8;
                cfg.dma_buf_len = kBufSamples;
                cfg.use_apll = false;
                cfg.tx_desc_auto_clear = true;

                if (i2s_driver_install(I2S_NUM_1, &cfg, 0, nullptr) != ESP_OK)
                    return false;

                i2s_pin_config_t pins = {};
                /* 注意：{} 会把 mck_io_num 置 0（GPIO0），必须显式设为 NO_CHANGE，
                 * 否则驱动会把 MCLK 路由到 GPIO0（与生产 Audio::setPinout 行为不一致） */
                pins.mck_io_num = I2S_PIN_NO_CHANGE;
                pins.bck_io_num = kPinI2sBclkSpeaker;
                pins.ws_io_num = kPinI2sLrclkSpeaker;
                pins.data_out_num = kPinI2sDataSpeaker;
                pins.data_in_num = I2S_PIN_NO_CHANGE;
                if (i2s_set_pin(I2S_NUM_1, &pins) != ESP_OK)
                {
                    i2s_driver_uninstall(I2S_NUM_1);
                    return false;
                }
                i2s_zero_dma_buffer(I2S_NUM_1);
                inited = true;
                return true;
            }

            void fillBuffer(const ToneStep &s)
            {
                if (stereo_buf == nullptr)
                    return;
                double amp = 32767.0 * (s.amplitude_pct / 100.0);
                // 防爆音：每帧开头 8 个样本做 0..1 ramp-up，结尾 8 个 ramp-down
                for (uint16_t i = 0; i < kBufSamples; ++i)
                {
                    double t = (double)(phase_pos + i) / (double)kSampleRate;
                    double v = sin(2.0 * M_PI * s.freq * t) * amp;
                    int16_t sample = (int16_t)v;
                    stereo_buf[i * 2] = sample;
                    stereo_buf[i * 2 + 1] = sample;
                }
                phase_pos += kBufSamples;
            }

            void drawIdle()
            {
                if (gfx == nullptr) return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T5 Speaker", "SW=start");
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_FG);
                gfx->setCursor(8, 22);
                gfx->print("MAX98357 I2S1 TX");
                gfx->setCursor(8, 36);
                gfx->printf("BCLK=%d LRCLK=%d DOUT=%d",
                            kPinI2sBclkSpeaker, kPinI2sLrclkSpeaker,
                            kPinI2sDataSpeaker);
                gfx->setCursor(8, 54);
                gfx->print("Sequence:");
                gfx->setCursor(8, 66);
                gfx->print("440Hz 1k 2k 4k");
                gfx->setCursor(8, 78);
                gfx->print("1k @ 25/50/75/100%");
                gfx->setCursor(8, 100);
                gfx->setTextColor(COLOR_DIM);
                gfx->print("SW during run = skip step");
                gfx->setCursor(8, kScreenH - 12);
                gfx->setTextColor(COLOR_DIM);
                gfx->print("SW dbl = back menu");
            }

            void drawHeader(const char *label)
            {
                if (gfx == nullptr) return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T5 Speaker", label);
            }

            void drawStep()
            {
                if (gfx == nullptr) return;
                const ToneStep &s = kSteps[step_idx];
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_DIM);
                gfx->setCursor(8, 22);
                gfx->printf("Step %u/%u", step_idx + 1, kStepCount);
                gfx->setTextSize(3);
                gfx->setTextColor(COLOR_ACCENT);
                gfx->setCursor(8, 36);
                gfx->print(s.label);
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_FG);
                gfx->setCursor(8, 78);
                gfx->printf("amp=%u%% dur=%ums",
                            s.amplitude_pct, s.duration_ms);
                // 进度条
                uint32_t age = millis() - step_started_ms;
                uint16_t prog = (age >= s.duration_ms)
                                    ? 1000
                                    : (uint16_t)(age * 1000 / s.duration_ms);
                drawLevelBar(gfx, 8, 96, kScreenW - 16, 14, prog, COLOR_OK, COLOR_DIM);
            }

            void drawDone()
            {
                if (gfx == nullptr) return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T5 Speaker", "done");
                gfx->setTextSize(2);
                gfx->setTextColor(COLOR_OK);
                gfx->setCursor(8, 30);
                gfx->print("Sequence done.");
                gfx->setTextSize(1);
                gfx->setTextColor(COLOR_DIM);
                gfx->setCursor(8, 70);
                gfx->print("SW again -> replay");
                gfx->setCursor(8, kScreenH - 12);
                gfx->setTextColor(COLOR_FG);
                gfx->print("SW dbl = back menu");
            }
        };

        TestBase *createTestSpeaker()
        {
            return new TestSpeaker();
        }

    } // namespace hwtest
} // namespace ekeys