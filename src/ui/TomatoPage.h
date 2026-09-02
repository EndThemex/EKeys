#pragma once
#include "ui/Page.h"
#include "rgb/RGBLightControl.h"
#include <lvgl.h>

namespace ekeys
{

    /* 番茄钟页
     *
     * 状态机：
     *   IDLE   -> 旋钮调整分钟数（步长 1min，1..60），单击 / KEY2 开始
     *   RUN    -> 倒计时；单击 / KEY2 暂停；旋钮 +/- 调整剩余时间
     *   DONE   -> 计时结束；单击 / KEY2 重置回 IDLE；RGB 强制红色呼吸
     *
     * UI：
     *   第 1 行: 标题 / 状态
     *   第 2 行: 大字倒计时 mm:ss
     *   第 3 行: 提示
     */
    class TomatoPage : public Page
    {
    public:
        TomatoPage(RGBLightControl &rgb);

        /* Page API */
        void onEnter() override;
        void onExit() override;
        void onEncoder(int8_t delta) override;
        void onConfirm() override;

        /* PageKind：S 流程状态机 —— KEY3..KEY9 直接进入第 idx 个状态/动作。
         * 状态机本身只有 4 个态（Idle/Run/Pause/Done），但"动作"视角下：
         *   idx=0 → reset    (KEY3)
         *   idx=1 → start    (KEY4)
         *   idx=2 → pause    (KEY5)
         *   idx=3 → resume   (KEY6)
         *   idx>=4           → 越界返回 false，基类丢弃。
         * 选择"动作"而非"状态"作为直选语义，是因为状态 DONE 没有"进入"价值，
         * 而 reset/start/pause/resume 是用户真正想"一键触发"的四个动作。 */
        PageKind kind() const override { return PageKind::State; }
        bool selectState(uint8_t idx) override;

        /* TomatoPage 用旋钮调时长 → 消费旋转，不发 BLE 方向键 */
        bool consumesEncoder() const override { return true; }

    private:
        enum class State : uint8_t
        {
            Idle,
            Run,
            Pause,
            Done
        };

        void buildUi() override;
        void refresh();

        /* 状态切换 */
        void startTimer();
        void pauseTimer();
        void resumeTimer();
        void resetTimer();
        void finishTimer();

        RGBLightControl &rgb_;
        State state_{State::Idle};

        uint32_t total_ms_{25 * 60 * 1000}; // 默认 25min
        uint32_t remain_ms_{0};
        uint32_t last_tick_ms_{0};

        lv_obj_t *status_label_{nullptr};
        lv_obj_t *time_label_{nullptr};
        lv_obj_t *hint_label_{nullptr};

    public:
        /* 由 main loop 调用，每帧推进一次 */
        void serviceTick();
    };

} // namespace ekeys