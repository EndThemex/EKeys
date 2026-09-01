#pragma once
#include "ui/Page.h"
#include "mic/Mic.h"
#include "mic/AudioAnalyzer.h"
#include <lvgl.h>

namespace ekeys
{

    /* 麦克风频谱页：
     *   - 启动 I2S Mic；
     *   - 每帧从 I2S 读 512 个样本、做 FFT、把 16 段能量映射为柱状条高度；
     *   - 在 428x142 屏幕的底部区域绘制 16 条等宽竖条。
     *   - 屏幕上半部分展示麦克风检查信息：引脚 / 采样率 / 初始化结果 /
     *     最近一次读样本距今的延迟 / 1 秒内读到的样本数 / 估算 RMS dB。
     *
     * 屏幕 428x142 布局：
     *   y=4   标题 "Mic Spectrum"           + 右上角实时 peak 数值
     *   y=24  状态行 "Mic: OK BCLK=11 WS=17 DIN=18 16kHz"
     *   y=40  诊断行1 "last: 32ms  cnt/s: 16000  RMS: -42dB"
     *   y=56  分割线
     *   y=64..120  频谱柱状区（高 56 px，16 条等宽）
     *   底右    "KEY1 back"
     */
    class MicPage : public Page
    {
    public:
        MicPage();
        ~MicPage() override;

        void onEnter() override;
        void onExit() override;

        /* 每帧调用一次：读 mic → FFT → 刷新柱状条 + 诊断文本 */
        void serviceTick();

        /* 纯展示页：旋钮不消费（穿透到 BLE 方向键） */
        bool consumesEncoder() const override { return false; }

    private:
        void buildUi() override;
        void teardownUi() override;
        void drawBars();
        void drawStatusLine(const char *text, lv_color_t color);
        void refreshDiag();

        Mic mic_;
        AudioAnalyzer analyzer_;

        /* 16 段柱状条对象（每段 1 个 lv_obj rect，方便直接 set_height） */
        lv_obj_t *bars_[AUDIO_BANDS]{nullptr};
        lv_obj_t *title_label_{nullptr};
        lv_obj_t *peak_label_{nullptr};
        lv_obj_t *status_label_{nullptr};
        lv_obj_t *diag_label_{nullptr};
        lv_obj_t *hint_{nullptr};

        /* 平滑显示用的指数滑动平均缓存，避免柱状条剧烈跳动 */
        float smooth_[AUDIO_BANDS]{0.0f};

        /* 诊断信息 */
        uint32_t last_ok_ms_{0};
        uint32_t last_tick_ms_{0};
        uint32_t samples_in_window_{0};
        uint32_t window_start_ms_{0};
        float last_rms_db_{-90.0f};
        bool mic_ok_{false};
    };

} // namespace ekeys