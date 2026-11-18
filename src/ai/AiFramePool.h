/*
 * AiFramePool.h
 *
 * AI Mode 帧缓冲池（docs/13 §6.1）：PSRAM 三槽环形。
 *   - Writing（解码/分片直写）与 Displaying（LVGL 逐片读取）永不重合；
 *   - 第三槽做"已解码待上屏"排队；
 *   - 状态迁移跨核（AiTask/MainTask 写，DisplayTask 读），用 portMUX
 *     临界区保护状态字段；像素内容本身不加锁（槽独占）。
 *
 * 分配时机：首次进入 AI 页时一次性分配，退出页面不释放（避免反复
 * 350KB PSRAM 分配导致碎片）。
 */
#ifndef EKEYS_AI_AI_FRAME_POOL_H
#define EKEYS_AI_AI_FRAME_POOL_H

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include <stdint.h>

#include "AiConfig.h"
#include "lvgl.h"

namespace ekeys
{
    namespace ai
    {

        class AiFramePool
        {
        public:
            static AiFramePool &instance();

            AiFramePool(const AiFramePool &) = delete;
            AiFramePool &operator=(const AiFramePool &) = delete;

            /* 首次调用分配 3 槽 PSRAM + 初始化 dsc；失败返回 false（页面进 ERROR） */
            bool ensureAllocated();
            bool allocated() const { return allocated_; }

            /* 空闲槽（Free）→ Writing，返回槽号；-1 = 三槽均忙（丢帧） */
            int acquireWriteSlot();

            /* Writing → Queued，投递 AiFrame 消息给 DisplayTask；
             * 队列满 → 槽回 Free（防泄漏）并返回 false */
            bool commitWriteSlot(int slot);

            /* DisplayTask：Queued → Displaying，并释放上一个 Displaying 槽 */
            void presentSlot(int slot);

            /* 解码失败/帧中止：Writing 直接回 Free（防槽泄漏卡死 FramePool） */
            void abandonWriteSlot(int slot);

            /* DisplayTask（测试图专用）：Writing 直接 → Displaying */
            void adoptDisplaying(int slot);

            /* 释放 Displaying 槽（换源后调用 / 离开 AI 页回收） */
            void releaseDisplaySlot(int slot);

            /* 离开 AI 页：回收 Displaying / Queued 槽（像素不释放，槽态归 Free） */
            void resetForIdle();

            const lv_img_dsc_t *dsc(int slot) const { return &dscs_[slot]; }
            uint8_t *px(int slot) const { return slots_[slot].px; }
            int displayingSlot() const { return displaying_; }
            int freeSlotCount() const;

        private:
            AiFramePool();

            enum SlotState : uint8_t
            {
                kFree = 0,
                kWriting = 1,
                kQueued = 2,
                kDisplaying = 3,
            };

            struct Slot
            {
                uint8_t *px{nullptr};
                uint8_t state{kFree};
                uint32_t seq{0};
            };

            void transition(int slot, uint8_t from, uint8_t to);

            /* const 查询接口（freeSlotCount）也需进临界区，故声明为 mutable */
            mutable portMUX_TYPE lock_;
            Slot slots_[kAiSlotCount];
            lv_img_dsc_t dscs_[kAiSlotCount];
            int displaying_{-1};
            bool allocated_{false};
        };

    } // namespace ai
} // namespace ekeys

#endif // EKEYS_AI_AI_FRAME_POOL_H
