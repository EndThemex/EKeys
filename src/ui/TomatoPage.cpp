#include "ui/TomatoPage.h"
#include "ui/Pages.h"
#include <FastLED.h>

namespace ekeys
{

    TomatoPage::TomatoPage(RGBLightControl &rgb)
        : Page(/*id=*/PAGE_TOMATO, "Pomodoro", lv_color_hex(0xFFCC00)), rgb_(rgb) {}

    void TomatoPage::buildUi()
    {
        lv_obj_t *root_obj = root();

        lv_obj_t *title = lv_label_create(root_obj);
        lv_label_set_text(title, "Pomodoro");
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

        lv_obj_t *line = lv_obj_create(root_obj);
        lv_obj_set_size(line, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 28);

        /* 状态行：IDLE / RUN / PAUSE / DONE */
        lv_obj_t *st = lv_label_create(root_obj);
        lv_label_set_text(st, "State: --");
        lv_obj_set_style_text_color(st, lv_color_hex(0x00FFCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(st, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(st, LV_ALIGN_TOP_LEFT, 8, 36);
        status_label_ = st;

        /* 大字倒计时：mm:ss */
        lv_obj_t *t = lv_label_create(root_obj);
        lv_label_set_text(t, "25:00");
        lv_obj_set_style_text_color(t, lv_color_hex(0xFFCC00), LV_PART_MAIN);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 60);
        time_label_ = t;

        /* 提示行 */
        lv_obj_t *h = lv_label_create(root_obj);
        lv_label_set_text(h, "KEY1 back");
        lv_obj_set_style_text_color(h, lv_color_hex(0x808080), LV_PART_MAIN);
        lv_obj_set_style_text_font(h, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(h, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
        hint_label_ = h;
    }

    void TomatoPage::onEnter()
    {
        /* 重置 UI 到 IDLE */
        last_tick_ms_ = millis();
        refresh();
    }

    void TomatoPage::onExit()
    {
        /* 退出时清掉 RGB 上的"Pomodoro 红"恢复默认灯效。
         * 简单做法：直接把 RGB 关掉（用户可以再手动开）。
         * 这里不切换 currentEffect_，避免改变菜单里的灯效设置。 */
        rgb_.setEnabled(false);
    }

    void TomatoPage::serviceTick()
    {
        if (state_ != State::Run)
            return;
        const uint32_t now = millis();
        const uint32_t delta = now - last_tick_ms_;
        last_tick_ms_ = now;
        if (delta == 0)
            return;
        if (remain_ms_ > delta)
        {
            remain_ms_ -= delta;
        }
        else
        {
            remain_ms_ = 0;
            finishTimer();
        }
        refresh();
    }

    void TomatoPage::onEncoder(int8_t delta)
    {
        if (state_ == State::Idle)
        {
            /* 步长 1 分钟，最小 1，最大 99 */
            int32_t m = (int32_t)(total_ms_ / 60000) + delta;
            if (m < 1)
                m = 1;
            if (m > 99)
                m = 99;
            total_ms_ = (uint32_t)m * 60000;
            remain_ms_ = total_ms_;
        }
        else if (state_ == State::Run || state_ == State::Pause)
        {
            /* 运行时调整 = 增加/减少 1 分钟 */
            int32_t new_total = (int32_t)(total_ms_ / 60000) + delta;
            if (new_total < 1)
                new_total = 1;
            if (new_total > 99)
                new_total = 99;
            total_ms_ = (uint32_t)new_total * 60000;
            if (state_ == State::Pause)
            {
                remain_ms_ = total_ms_;
            }
            else
            {
                /* Run 中调整：保持当前剩余相对变化？
                 * 这里简化为：重新设置剩余 = 新 total（避免越界）。 */
                remain_ms_ = total_ms_;
            }
        }
        else if (state_ == State::Done)
        {
            /* 已结束：旋转无操作 */
        }
        refresh();
    }

    void TomatoPage::onEncoderClick()
    {
        /* 单击：IDLE/RUN/PAUSE/DONE 各自的"切换"动作 */
        switch (state_)
        {
        case State::Idle:
            startTimer();
            break;
        case State::Run:
            pauseTimer();
            break;
        case State::Pause:
            resumeTimer();
            break;
        case State::Done:
            resetTimer();
            break;
        }
    }

    void TomatoPage::onConfirm()
    {
        /* KEY2 = 与单击同义 */
        onEncoderClick();
    }

    void TomatoPage::startTimer()
    {
        state_ = State::Run;
        if (remain_ms_ == 0)
            remain_ms_ = total_ms_;
        last_tick_ms_ = millis();

        /* 视觉反馈：番茄钟运行 = RGB 切到 Pulse + enable */
        rgb_.setEnabled(true);
        /* Pulse 已经是白色呼吸；想换色可在这里 setEffect(StaticRed) 或增加枚举。
         * 暂时保持既有灯效不变，只确保 enable。 */
    }

    void TomatoPage::pauseTimer()
    {
        state_ = State::Pause;
    }

    void TomatoPage::resumeTimer()
    {
        state_ = State::Run;
        last_tick_ms_ = millis();
    }

    void TomatoPage::resetTimer()
    {
        state_ = State::Idle;
        remain_ms_ = total_ms_;
        rgb_.setEnabled(false);
    }

    void TomatoPage::finishTimer()
    {
        state_ = State::Done;
        remain_ms_ = 0;
        /* 番茄钟结束：强制红色提醒（写死一帧红灯，不动 currentEffect_）。 */
        CRGB *buf = rgb_.leds();
        for (int i = 0; i < 9; ++i)
            buf[i] = CRGB::Red;
        rgb_.setBrightnessLevel(rgb_.brightness());
        rgb_.show();
    }

    void TomatoPage::refresh()
    {
        /* 状态行 */
        static const char *stateNames[] = {"IDLE", "RUN", "PAUSE", "DONE"};
        if (status_label_ != nullptr)
        {
            lv_label_set_text_fmt(status_label_, "State: %s", stateNames[(uint8_t)state_]);
        }
        /* mm:ss */
        uint32_t total_seconds = (state_ == State::Done)   ? 0
                                 : (state_ == State::Idle) ? total_ms_ / 1000
                                                           : remain_ms_ / 1000;
        uint8_t mm = (uint8_t)(total_seconds / 60);
        uint8_t ss = (uint8_t)(total_seconds % 60);
        if (time_label_ != nullptr)
        {
            lv_label_set_text_fmt(time_label_, "%02u:%02u", mm, ss);
            /* 颜色随状态变 */
            lv_color_t col = lv_color_hex(0xFFCC00);
            if (state_ == State::Run)
                col = lv_color_hex(0x00FF66);
            if (state_ == State::Pause)
                col = lv_color_hex(0xFFAA00);
            if (state_ == State::Done)
                col = lv_color_hex(0xFF3333);
            lv_obj_set_style_text_color(time_label_, col, LV_PART_MAIN);
        }
        if (hint_label_ != nullptr)
        {
            const char *h = "KEY1 back";
            if (state_ == State::Idle)
                h = "click=start";
            if (state_ == State::Run)
                h = "click=pause";
            if (state_ == State::Pause)
                h = "click=resume";
            if (state_ == State::Done)
                h = "click=reset";
            lv_label_set_text(hint_label_, h);
        }
    }

} // namespace ekeys