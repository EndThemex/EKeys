#include "ui/MicPage.h"
#include "ui/Pages.h"
#include <Arduino.h>
#include <math.h>

/* 与 main.cpp 中的 serialPrintf() 对齐：在本文件里临时声明一个本地版本，
 * 这样不需要把 main.cpp 内部的全局 helper 暴露到头文件里。
 * MicPage 的调试日志只是辅助诊断，单条短、无锁即可。 */
#include <stdarg.h>
#include <stdio.h>
static void micSerialPrintf(const char *fmt, ...)
{
    char buf[160];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0)
        return;
    if (n >= (int)sizeof(buf))
        n = sizeof(buf) - 1;
    Serial.write((const uint8_t *)buf, (size_t)n);
}
#define MIC_SERIAL_PRINTF(...) micSerialPrintf(__VA_ARGS__)

namespace ekeys
{

    /* 屏幕高 142：诊断区 y=0..60，柱状区 y=68..124，提示 y=128..142。
     * 柱状高度 = 56 px，宽度 = (总宽 - 边距*2 - 间隙*(N-1)) / N。
     * 边距 = 8，左右合计 16；总间隙 = 4*15 = 60；段宽 = (428-16-60)/16 ≈ 22。 */
    static constexpr int16_t SPECTRUM_X0 = 8;
    static constexpr int16_t SPECTRUM_Y0 = 68;
    static constexpr int16_t SPECTRUM_H = 56;
    static constexpr int16_t BAR_W = 22;
    static constexpr int16_t BAR_GAP = 4;
    /* 柱状条最小可见高度 (px)。低于此值的能量按此值出，避免空画后看着像"没反应"。 */
    static constexpr int16_t BAR_MIN_H = 4;

    /* 把单段能量近似映射到 0..SPECTRUM_H 的高度。
     * 实测安静环境单段 raw 几十到几百，大声 1k~5k，按 1500 做参考上限，
     * 既保证安静环境也能看到跳动，也不会让大声瞬间全部顶满。 */
    static constexpr float BAR_SCALE = (float)SPECTRUM_H / 1500.0f;

    MicPage::MicPage()
        : Page(/*id=*/PAGE_MIC, "Mic", lv_color_hex(0x66FFCC)),
          analyzer_(MIC_SAMPLE_RATE)
    {
    }

    MicPage::~MicPage()
    {
        /* I2S 资源通过 mic_ 析构自动释放；这里再补一刀保险 */
        mic_.end();
    }

    void MicPage::buildUi()
    {
        lv_obj_t *root_obj = root();

        lv_obj_set_style_bg_opa(root_obj, LV_OPA_TRANSP, LV_PART_MAIN);

        /* 标题 */
        title_label_ = lv_label_create(root_obj);
        lv_label_set_text(title_label_, "Mic Spectrum");
        lv_obj_set_style_text_color(title_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title_label_, LV_ALIGN_TOP_LEFT, 8, 4);

        /* 右上角实时 peak (dB 估值) */
        peak_label_ = lv_label_create(root_obj);
        lv_label_set_text(peak_label_, "--");
        lv_obj_set_style_text_color(peak_label_, lv_color_hex(0x66FFCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(peak_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(peak_label_, LV_ALIGN_TOP_RIGHT, -8, 8);

        /* 分割线 */
        lv_obj_t *line = lv_obj_create(root_obj);
        lv_obj_set_size(line, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 60);

        /* 频谱柱状条：从底往上画，所以锚定 BOTTOM。
         * 初始高度给一个最小可见值 BAR_MIN_H，确保即使还没读到有效样本，
         * 也能看到一排"底色柱"，方便排查"是不是 FFT 输出都是 0"。 */
        for (int i = 0; i < AUDIO_BANDS; ++i)
        {
            lv_obj_t *bar = lv_obj_create(root_obj);
            lv_obj_remove_style_all(bar);
            lv_obj_set_size(bar, BAR_W, BAR_MIN_H);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0x66FFCC), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
            const int16_t x = SPECTRUM_X0 + i * (BAR_W + BAR_GAP);
            const int16_t y = SPECTRUM_Y0 + SPECTRUM_H - BAR_MIN_H;
            lv_obj_set_pos(bar, x, y);
            bars_[i] = bar;
        }

        /* 状态行（Mic OK / FAIL + 引脚 + 采样率），y=28 */
        status_label_ = lv_label_create(root_obj);
        lv_label_set_text(status_label_, "Mic: init");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 8, 28);

        /* 诊断行：last / cnt/s / RMS dB，y=42 */
        diag_label_ = lv_label_create(root_obj);
        lv_label_set_text(diag_label_, "last:-- cnt/s:0  RMS:--");
        lv_obj_set_style_text_color(diag_label_, lv_color_hex(0x9CA3AF), LV_PART_MAIN);
        lv_obj_set_style_text_font(diag_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(diag_label_, LV_ALIGN_TOP_LEFT, 8, 42);

        /* 底部提示 —— 按 docs/10-input-mapping-rule.md §5 R 类型模板。 */
        hint_ = lv_label_create(root_obj);
        char hint_buf[80];
        buildHintLabel(kind(), hint_buf, sizeof(hint_buf));
        lv_label_set_text(hint_, hint_buf);
        lv_obj_set_style_text_color(hint_, lv_color_hex(0x808080), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint_, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    }

    void MicPage::teardownUi()
    {
        /* bars_ 数组随 root 一起被 lv_obj_clean，不需要单独删除 */
        bars_[0] = nullptr; /* 防误用 */
    }

    void MicPage::onEnter()
    {
        last_tick_ms_ = millis();
        window_start_ms_ = last_tick_ms_;
        samples_in_window_ = 0;
        last_rms_db_ = -90.0f;

        if (!mic_.begin())
        {
            mic_ok_ = false;
            drawStatusLine("Mic: FAIL", lv_color_hex(0xFF5555));
        }
        else
        {
            analyzer_.begin();
            mic_ok_ = true;
            last_ok_ms_ = millis();
            /* 在状态行上把硬件信息也写出来，便于一眼确认引脚配置 */
            lv_label_set_text_fmt(status_label_,
                                  "Mic: OK  BCLK=%d WS=%d DIN=%d %uHz",
                                  MIC_I2S_BCLK, MIC_I2S_WS, MIC_I2S_DATA,
                                  (unsigned)MIC_SAMPLE_RATE);
            lv_obj_set_style_text_color(status_label_,
                                        lv_color_hex(0x66FFCC), LV_PART_MAIN);
        }
        memset(smooth_, 0, sizeof(smooth_));
    }

    void MicPage::onExit()
    {
        mic_.end();
        mic_ok_ = false;
    }

    void MicPage::serviceTick()
    {
        if (!mic_ok_ || !mic_.isInitialized())
            return;

        /* 16 kHz / 512 样本 ≈ 32 ms；这里直接同步读取 + FFT，单帧 ~10~15 ms，
         * 还在 50 ms 帧预算内。后续若要更流畅可挪到 xTaskCreatePinnedToCore。 */
        int32_t buf[MIC_BUFFER_SAMPLES];
        size_t n = mic_.read(buf, MIC_BUFFER_SAMPLES);
        if (n == 0)
            return;

        /* 计算 RMS → dB（参考 24-bit 满量程 8388607 = 0 dBFS）。 */
        double acc = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            const double s = (double)buf[i];
            acc += s * s;
        }
        const double rms = sqrt(acc / (double)n);
        /* 8388607 = 2^23 - 1 是 24-bit 满量程；阈值 16 对应 -113 dBFS，留点余量。 */
        last_rms_db_ = (rms > 16.0) ? (float)(20.0 * log10(rms / 8388607.0)) : -90.0f;

        analyzer_.process(buf, n);
        last_ok_ms_ = millis();
        samples_in_window_ += (uint32_t)n;
        drawBars();
        refreshDiag();

        /* 调试日志：每 ~300ms 打一行，便于确认 I2S/FFT/UI 哪一环卡住。
         * 当你看到屏幕不动时，看 Serial 输出即可定位阶段。
         * 这里额外打印前 4 个 int32_t 原始样本：当 peak_raw=0 时，
         * 看 raw[0..3] 就能区分是"全是 0"（接线错/SD 没数据），
         * 还是"有数据但 FFT 算出来是 0"（FFT 算法/参数问题）。 */
        static uint32_t s_last_dbg_ms = 0;
        if (last_ok_ms_ - s_last_dbg_ms >= 300)
        {
            s_last_dbg_ms = last_ok_ms_;
            const float *b = analyzer_.getBands();
            float mx = 0.0f;
            for (int i = 0; i < AUDIO_BANDS; ++i)
                if (b[i] > mx)
                    mx = b[i];
            MIC_SERIAL_PRINTF(
                "[Mic] n=%u rms=%2.0fdB peak_raw=%.2f  raw[0..3]=%ld,%ld,%ld,%ld\n",
                (unsigned)n, (double)last_rms_db_, (double)mx,
                (long)buf[0], (long)buf[1], (long)buf[2], (long)buf[3]);
        }
    }

    void MicPage::drawBars()
    {
        const float *bands = analyzer_.getBands();
        float peak = 0.0f;

        for (int i = 0; i < AUDIO_BANDS; ++i)
        {
            float v = bands[i];
            if (v < 0.0f)
                v = 0.0f;

            /* 指数滑动平均 (attack 快，release 慢) */
            const float ATTACK = 0.6f;
            const float RELEASE = 0.15f;
            if (v > smooth_[i])
                smooth_[i] += (v - smooth_[i]) * ATTACK;
            else
                smooth_[i] += (v - smooth_[i]) * RELEASE;

            if (smooth_[i] > peak)
                peak = smooth_[i];

            int16_t h = (int16_t)(smooth_[i] * BAR_SCALE);
            if (h < BAR_MIN_H)
                h = BAR_MIN_H;
            if (h > SPECTRUM_H)
                h = SPECTRUM_H;

            if (bars_[i] != nullptr)
            {
                lv_obj_set_size(bars_[i], BAR_W, h);
                /* 底部对齐：y = 基线 - (h-1) */
                const int16_t y = SPECTRUM_Y0 + SPECTRUM_H - h;
                lv_obj_set_y(bars_[i], y);
            }
        }

        if (peak_label_ != nullptr)
        {
            /* 把 peak 粗略映射成 dB（参考 1.0 = 0dB），仅作"看着有反应"用 */
            const float db = (peak > 0.0001f) ? 20.0f * log10f(peak) : -90.0f;
            lv_label_set_text_fmt(peak_label_, "%2.0fdB", db);
        }
    }

    void MicPage::drawStatusLine(const char *text, lv_color_t color)
    {
        if (status_label_ == nullptr)
            return;
        lv_label_set_text(status_label_, text);
        lv_obj_set_style_text_color(status_label_, color, LV_PART_MAIN);
    }

    /* 刷新诊断文本：
     *   last  - 自上次成功读样本以来过去了多少 ms（>500ms 就黄，>1500ms 就红）
     *   cnt/s - 过去 1 秒滑动窗口内读到的样本数（理想值 ≈ 16000）
     *   RMS   - 上一帧的均方根幅度，按 32767 满量程换算成 dBFS
     */
    void MicPage::refreshDiag()
    {
        if (diag_label_ == nullptr)
            return;

        const uint32_t now = millis();
        const uint32_t since_last = now - last_ok_ms_;
        const uint32_t window_ms = now - window_start_ms_;

        if (window_ms >= 1000)
        {
            /* 1 秒滑窗：把窗口内累计的样本数归一化到 /s */
            samples_in_window_ = (samples_in_window_ * 1000U) / window_ms;
            window_start_ms_ = now;
        }
        (void)last_tick_ms_;

        /* 颜色按延迟给：正常灰、偏黄、变红 */
        lv_color_t c = lv_color_hex(0x9CA3AF);
        if (since_last > 1500)
            c = lv_color_hex(0xFF5555);
        else if (since_last > 500)
            c = lv_color_hex(0xFFAA00);

        lv_label_set_text_fmt(diag_label_,
                              "last:%4lums  cnt/s:%5u  RMS:%2.0fdB",
                              (unsigned long)since_last,
                              (unsigned)samples_in_window_,
                              (double)last_rms_db_);
        lv_obj_set_style_text_color(diag_label_, c, LV_PART_MAIN);
    }

} // namespace ekeys