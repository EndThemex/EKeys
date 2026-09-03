#pragma once
#include "ui/Page.h"
#include "ui/Pages.h" /* PAGE_BACKLIGHT 枚举值 */
#include <lvgl.h>

namespace ekeys
{

    /* 背光调节页
     *
     * 交互：
     *   - 旋钮旋转：在 MIN_PCT (25%) ~ MAX_PCT (100%) 之间循环调整亮度
     *     每步 ±5%（StepSize），共 (100-25)/5+1 = 16 档
     *   - KEY1：退出回菜单
     *   - KEY2 / 旋钮单击：仅作为"确认"，不做任何动作（防过度调暗与"调暗到看不见"对抗）
     *   - KEY3..KEY9：直选第 idx 个档位（idx=0 → MIN_PCT，idx=15 → MAX_PCT），越界无操作
     *
     * 防过度调暗设计：
     *   - 亮度下限固定在 MIN_PCT（=25%），低于此值屏幕画面发黑不可读
     *   - UI 上把 ≤25% 的档位标记为"DIM"，提示用户这是有效下限
     *   - 进入页面时立即把当前亮度同步显示
     */
    class BacklightPage : public Page
    {
    public:
        BacklightPage();

        /* Page API：旋钮调亮度（消费旋转，不发 BLE 方向键） */
        void onEnter() override;
        void onEncoder(int8_t delta) override;
        void onConfirm() override {}

        /* PageKind：State 流程状态机风格 —— KEY3..KEY9 直接选第 idx 档。
         * idx ∈ [0, 15] 有效；越界无操作。 */
        PageKind kind() const override { return PageKind::State; }
        bool selectState(uint8_t idx) override;

        /* 消费旋转：调亮度不发 BLE 方向键 */
        bool consumesEncoder() const override { return true; }

    private:
        void buildUi() override;
        /* 重新绘制当前亮度对应的 UI 状态：
         *   - 数值 label (XX%)
         *   - 进度条 fill width
         *   - "DIM" 警告标记（亮度 ≤ MIN_PCT 时显示）
         */
        void refresh();

        /* 档位数：(MAX_PCT - MIN_PCT) / STEP_PCT + 1 */
        static constexpr uint8_t MIN_PCT = 25;
        static constexpr uint8_t MAX_PCT = 100;
        static constexpr uint8_t STEP_PCT = 5;
        static constexpr uint8_t LEVEL_COUNT = (MAX_PCT - MIN_PCT) / STEP_PCT + 1; /* 16 */

        /* 把 idx (0..15) 转换为实际百分比（=MIN_PCT + idx*STEP_PCT） */
        static uint8_t idxToPct(uint8_t idx)
        {
            return (uint8_t)(MIN_PCT + idx * STEP_PCT);
        }

        /* 把全局亮度百分比反向映射回档位 idx（用于 onEnter 同步） */
        static uint8_t pctToLevel(uint8_t pct)
        {
            if (pct < idxToPct(0))
                pct = idxToPct(0);
            uint8_t idx = (uint8_t)((pct - idxToPct(0)) / STEP_PCT);
            if (idx >= LEVEL_COUNT)
                idx = LEVEL_COUNT - 1;
            return idx;
        }

        /* 当前档位索引 0..LEVEL_COUNT-1 */
        uint8_t level_{15}; /* 默认 100% (idx=15) — 保持项目原有"最亮"语义 */

        /* UI 元素 */
        lv_obj_t *value_label_{nullptr}; /* 数值文字 "XX%" */
        lv_obj_t *bar_fill_{nullptr};    /* 进度条填充 */
        lv_obj_t *dim_warn_{nullptr};    /* "DIM" 警告文字（≤MIN_PCT 时显示） */
    };

} // namespace ekeys