/*
 * TestMic.cpp
 *
 * T4 麦克风测试实现。
 *
 * 自动循环流程（尽量少按键，便于反复验证）：
 *   LiveVU   - 进入即开始：实时电平（大电平条 + 峰值保持线）
 *   Record   - SW 单击：录 3s 到 PSRAM（大倒计时数字）
 *   Playback - 录完自动回放（进度条）；SW 单击可跳过
 *   播完自动回到 LiveVU，无需手动重启
 *
 * 录音（Mic，I2S0，SCK=IO13 专用线）与回放（speaker，I2S1，BCLK=IO10）
 * 硬件上互不冲突；切换阶段仍串行启停 I2S，保持时序清晰。
 */

#include "TestMic.h"

#include <Arduino_GFX_Library.h>
#include <driver/i2s.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "HwTestMenu.h"
#include "HwTestUI.h"
#include "audio/Mic.h"
#include "display/DisplayDriver.h"
#include "hardware/PinMap.h"
#include "logging/LogManager.h"

namespace ekeys
{
    namespace hwtest
    {

        enum class MicPhase : uint8_t
        {
            LiveVU = 0,
            Record,
            Playback,
            Error, // Mic 初始化失败，SW 重试
        };

        // 3 秒 × 16kHz = 48000 样本
        constexpr uint32_t kRecordSamples = 48000;
        constexpr uint32_t kRecordMs = 3000;

        // I2S1 (speaker) 16bit stereo
        constexpr uint32_t kSpeakerSampleRate = 16000;
        // 回放每次写入的单声道样本数（512 × 2ch × 2B = 2KB）
        constexpr size_t kPlayChunkSamples = 512;

        // 电平条几何（428×142，标题栏占 y 0..13）
        constexpr int16_t kBarX = 8;
        constexpr int16_t kBarW = kScreenW - 16;
        constexpr int16_t kRmsBarY = 34;
        constexpr int16_t kPeakBarY = 64;
        constexpr int16_t kBarH = 22;

        struct TestMic : public TestBase
        {
            Arduino_GFX *gfx = nullptr;
            MicPhase phase = MicPhase::LiveVU;
            uint32_t phase_entered_ms = 0;
            uint32_t last_redraw_ms = 0;
            uint32_t last_progress_ms = 0;

            int16_t *pcm = nullptr;
            size_t recorded = 0;

            // live VU
            int16_t read_buf[Mic::kDmaSamples];
            uint16_t live_rms_milli = 0; // 0~1000
            uint16_t live_peak_milli = 0;
            uint16_t peak_hold_milli = 0; // 峰值保持（缓降）
            uint32_t peak_hold_ms = 0;

            // speaker i2s state
            bool speaker_inited = false;

            // playback streaming state
            size_t play_pos_ = 0;
            int16_t *play_chunk = nullptr;

            const char *title() const override { return "T4 Mic"; }

            void enter() override
            {
                gfx = DisplayDriver::instance().gfx();
                if (pcm == nullptr)
                {
                    pcm = (int16_t *)ps_malloc(kRecordSamples * sizeof(int16_t));
                    LOG_INFO("T4", "pcm buf %u bytes %s",
                             (unsigned)(kRecordSamples * sizeof(int16_t)),
                             pcm ? "ok" : "NULL");
                }
                /* 保险：卸载可能残留的 I2S1 驱动，确保回放从干净状态开始 */
                i2s_driver_uninstall(I2S_NUM_1);
                speaker_inited = false;
                startLiveVU();
                LOG_INFO("T4", "enter");
            }

            void loop(uint8_t key) override
            {
                uint32_t now = millis();
                if (key == ROT_ENTER)
                {
                    if (phase == MicPhase::LiveVU || phase == MicPhase::Error)
                    {
                        startRecord();
                    }
                    else if (phase == MicPhase::Playback)
                    {
                        finishPlayback(); // 跳过回放
                    }
                    return;
                }

                if (phase == MicPhase::LiveVU)
                {
                    runLiveVU(now);
                    if (now - last_redraw_ms > 80)
                    {
                        last_redraw_ms = now;
                        drawVUNumbers();
                        drawVUBars();
                    }
                }
                else if (phase == MicPhase::Record)
                {
                    runRecord(now);
                }
                else if (phase == MicPhase::Playback)
                {
                    runPlayback(now);
                }
            }

            void exit() override
            {
                LOG_INFO("T4", "exit");
                cleanupAll();
                if (play_chunk != nullptr)
                {
                    free(play_chunk);
                    play_chunk = nullptr;
                }
                if (pcm != nullptr)
                {
                    free(pcm);
                    pcm = nullptr;
                }
                gfx = nullptr;
            }

            // ---- 阶段切换 ----

            void startLiveVU()
            {
                live_rms_milli = 0;
                live_peak_milli = 0;
                peak_hold_milli = 0;
                last_redraw_ms = 0;
                if (Mic::instance().begin())
                {
                    phase = MicPhase::LiveVU;
                    drawLiveVUSkeleton();
                }
                else
                {
                    phase = MicPhase::Error;
                    drawError();
                }
            }

            void startRecord()
            {
                if (pcm == nullptr)
                {
                    LOG_ERROR("T4", "no pcm buffer");
                    return;
                }
                if (phase == MicPhase::Error)
                {
                    // 从错误态重试：需要先初始化 Mic
                    if (!Mic::instance().begin())
                    {
                        LOG_ERROR("T4", "Mic::begin failed (retry)");
                        return;
                    }
                }
                phase = MicPhase::Record;
                recorded = 0;
                phase_entered_ms = millis();
                last_progress_ms = 0;
                LOG_INFO("T4", "record start");
                drawRecordFrame();
            }

            // 录满自动进入回放
            void finishRecord()
            {
                LOG_INFO("T4", "record done: %u samples", (unsigned)recorded);
                Mic::instance().end();
                play_pos_ = 0;
                if (!initSpeaker())
                {
                    LOG_ERROR("T4", "speaker i2s init failed, back to VU");
                    startLiveVU();
                    return;
                }
                phase = MicPhase::Playback;
                phase_entered_ms = millis();
                last_progress_ms = 0;
                LOG_INFO("T4", "playback start");
                drawPlaybackFrame();
            }

            // 播完（或 SW 跳过）自动回到 LiveVU
            void finishPlayback()
            {
                LOG_INFO("T4", "playback done");
                deinitSpeaker();
                startLiveVU();
            }

            // ---- 运行 ----

            void runLiveVU(uint32_t now)
            {
                size_t got = Mic::instance().Read(read_buf, Mic::kDmaSamples);
                if (got == 0)
                    return;
                uint32_t sum_sq = 0;
                int16_t peak = 0;
                for (size_t i = 0; i < got; ++i)
                {
                    int32_t s = read_buf[i];
                    sum_sq += (uint32_t)(s * s);
                    int16_t a = (s < 0) ? (int16_t)-s : (int16_t)s;
                    if (a > peak) peak = a;
                }
                // RMS
                double rms = sqrt((double)sum_sq / got);
                if (rms > 32767.0) rms = 32767.0;
                live_rms_milli = (uint16_t)(rms * 1000.0 / 32767.0);
                // Peak
                live_peak_milli = (uint16_t)((int32_t)peak * 1000 / 32767);
                // 峰值保持：立即跟随上升，1s 后缓慢下降
                if (live_peak_milli >= peak_hold_milli)
                {
                    peak_hold_milli = live_peak_milli;
                    peak_hold_ms = now;
                }
                else if (now - peak_hold_ms > 1000)
                {
                    peak_hold_milli =
                        (peak_hold_milli > 12) ? (uint16_t)(peak_hold_milli - 12) : 0;
                }
            }

            void runRecord(uint32_t now)
            {
                if (pcm == nullptr)
                    return;
                if (now - phase_entered_ms > kRecordMs)
                {
                    finishRecord();
                    return;
                }
                // 每帧最多读 512 个
                size_t want = Mic::kDmaSamples;
                if (recorded + want > kRecordSamples)
                    want = kRecordSamples - recorded;
                if (want == 0)
                    return;
                size_t got = Mic::instance().Read(pcm + recorded, want);
                recorded += got;
                // 倒计时/进度（节流 100ms，避免 SPI 刷屏挤占录音）
                if (now - last_progress_ms >= 100)
                {
                    last_progress_ms = now;
                    drawRecordFrame();
                }
            }

            void runPlayback(uint32_t now)
            {
                if (play_chunk == nullptr)
                {
                    play_chunk = (int16_t *)malloc(kPlayChunkSamples * 2 * sizeof(int16_t));
                    if (play_chunk == nullptr)
                    {
                        LOG_ERROR("T4", "play chunk alloc fail");
                        finishPlayback();
                        return;
                    }
                }
                if (play_pos_ >= recorded)
                {
                    finishPlayback();
                    return;
                }
                size_t n = recorded - play_pos_;
                if (n > kPlayChunkSamples)
                    n = kPlayChunkSamples;
                // 单声道 → 立体声复制
                for (size_t i = 0; i < n; ++i)
                {
                    int16_t s = pcm[play_pos_ + i];
                    play_chunk[i * 2] = s;
                    play_chunk[i * 2 + 1] = s;
                }
                size_t written = 0;
                if (i2s_write(I2S_NUM_1, play_chunk, n * 2 * sizeof(int16_t),
                              &written, pdMS_TO_TICKS(200)) == ESP_OK)
                {
                    // written 为字节；每个单声道样本占 4 字节（2ch×16bit）
                    play_pos_ += written / (2 * sizeof(int16_t));
                }
                if (now - last_progress_ms >= 100)
                {
                    last_progress_ms = now;
                    drawPlaybackFrame();
                }
            }

            // ---- speaker I2S ----

            bool initSpeaker()
            {
                i2s_config_t cfg = {};
                cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
                cfg.sample_rate = kSpeakerSampleRate;
                cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
                cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
                cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
                cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
                cfg.dma_buf_count = 8;
                cfg.dma_buf_len = 1024;
                cfg.use_apll = false;
                cfg.tx_desc_auto_clear = true;

                if (i2s_driver_install(I2S_NUM_1, &cfg, 0, nullptr) != ESP_OK)
                    return false;

                i2s_pin_config_t pins = {};
                /* 注意：{} 会把 mck_io_num 置 0（GPIO0），必须显式设为 NO_CHANGE */
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
                speaker_inited = true;
                return true;
            }

            void deinitSpeaker()
            {
                if (!speaker_inited)
                    return;
                i2s_driver_uninstall(I2S_NUM_1);
                speaker_inited = false;
            }

            void cleanupAll()
            {
                Mic::instance().end();
                deinitSpeaker();
            }

            // ---- UI ----

            void drawLiveVUSkeleton()
            {
                if (gfx == nullptr) return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T4 Mic", "Live VU");
                drawVUNumbers();
                drawVUBars();
                drawText(gfx, 8, 96, "SW = record 3s (auto playback)", COLOR_DIM, 1);
                drawText(gfx, 8, kScreenH - 12, "SW dbl = back menu", COLOR_DIM, 1);
            }

            // 数值行（先擦后画，避免残影）
            void drawVUNumbers()
            {
                if (gfx == nullptr) return;
                gfx->fillRect(8, 20, kScreenW - 16, 11, COLOR_BG);
                float dbfs = -100.0f;
                if (live_rms_milli > 0)
                {
                    dbfs = 20.0f * log10f((float)live_rms_milli / 1000.0f);
                }
                drawFmt(gfx, 8, 20, COLOR_DIM, 1,
                        "RMS %3u%%   PEAK %3u%%   RMS %6.1f dBFS",
                        (unsigned)(live_rms_milli / 10),
                        (unsigned)(live_peak_milli / 10), dbfs);
            }

            void drawVUBars()
            {
                if (gfx == nullptr) return;
                drawLevelBar(gfx, kBarX, kRmsBarY, kBarW, kBarH,
                             live_rms_milli, COLOR_OK, COLOR_DIM);
                drawLevelBar(gfx, kBarX, kPeakBarY, kBarW, kBarH,
                             live_peak_milli, COLOR_WARN, COLOR_DIM);
                // 峰值保持线（画在 Peak 条上）
                if (peak_hold_milli > 0)
                {
                    int16_t hx = kBarX + (int16_t)((int32_t)kBarW * peak_hold_milli / 1000);
                    if (hx > kBarX + kBarW - 2) hx = kBarX + kBarW - 2;
                    gfx->fillRect(hx, kPeakBarY, 2, kBarH, COLOR_FG);
                }
            }

            // 录音帧：大倒计时 + 进度（先擦标题栏以下区域）
            void drawRecordFrame()
            {
                if (gfx == nullptr) return;
                gfx->fillRect(0, 16, kScreenW, kScreenH - 16, COLOR_BG);
                drawTitle(gfx, "T4 Mic", "Recording");
                uint32_t elapsed = millis() - phase_entered_ms;
                uint32_t remain_ms = (elapsed >= kRecordMs) ? 0 : kRecordMs - elapsed;
                uint8_t remain_s = (uint8_t)((remain_ms + 999) / 1000);
                gfx->setTextSize(5);
                gfx->setTextColor(COLOR_WARN);
                char b[2] = {(char)('0' + remain_s), 0};
                gfx->setCursor((kScreenW - 25) / 2, 24);
                gfx->print(b);
                gfx->setTextSize(1);
                uint32_t pct = (uint32_t)(elapsed * 100 / kRecordMs);
                if (pct > 100) pct = 100;
                drawFmt(gfx, 8, 80, COLOR_DIM, 1, "%u / %u samples (%lu%%)",
                        (unsigned)recorded, (unsigned)kRecordSamples,
                        (unsigned long)pct);
                drawLevelBar(gfx, 8, 94, kScreenW - 16, 14,
                             (uint16_t)(pct * 10), COLOR_WARN, COLOR_DIM);
                drawText(gfx, 8, kScreenH - 12, "recording...", COLOR_DIM, 1);
            }

            // 回放帧：进度条 + 百分比
            void drawPlaybackFrame()
            {
                if (gfx == nullptr) return;
                gfx->fillRect(0, 16, kScreenW, kScreenH - 16, COLOR_BG);
                drawTitle(gfx, "T4 Mic", "Playback");
                uint32_t total = (recorded > 0) ? recorded : 1;
                uint32_t pct = (uint32_t)(play_pos_ * 100 / total);
                if (pct > 100) pct = 100;
                drawText(gfx, 8, 24, "Playing back recording...", COLOR_FG, 1);
                drawLevelBar(gfx, 8, 44, kScreenW - 16, 16,
                             (uint16_t)(pct * 10), COLOR_OK, COLOR_DIM);
                drawFmt(gfx, 8, 72, COLOR_OK, 2, "%lu%%", (unsigned long)pct);
                drawText(gfx, 8, kScreenH - 12, "SW = skip", COLOR_DIM, 1);
            }

            void drawError()
            {
                if (gfx == nullptr) return;
                gfx->fillScreen(COLOR_BG);
                drawTitle(gfx, "T4 Mic", "error");
                drawText(gfx, 8, 30, "Mic init failed!", COLOR_WARN, 2);
                drawText(gfx, 8, 60, "Check ICS43434 wiring:", COLOR_DIM, 1);
                drawFmt(gfx, 8, 74, COLOR_DIM, 1, "SCK=%u WS=%u SD=%u",
                        kPinI2sMicSck, kPinI2sMicWs, kPinI2sMicSd);
                drawText(gfx, 8, 96, "SW = retry", COLOR_FG, 1);
                drawText(gfx, 8, kScreenH - 12, "SW dbl = back menu", COLOR_DIM, 1);
            }
        };

        TestBase *createTestMic()
        {
            return new TestMic();
        }

    } // namespace hwtest
} // namespace ekeys
