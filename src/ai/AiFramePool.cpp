/*
 * AiFramePool.cpp
 *
 * 见 AiFramePool.h。像素缓冲与 lv_img_dsc_t 生命周期说明：
 *   - px：heap_caps_malloc(MALLOC_CAP_SPIRAM)，首次进入 AI 页分配，常驻；
 *   - dsc：LVGL 不管理其内存（data 指针直指槽缓冲），槽常驻即常驻。
 */
#include "AiFramePool.h"

#include <esp_heap_caps.h>
#include <string.h>

#include "message_types.h"
#include "tasks/DisplayTask.h"
#include "logging/LogManager.h"

namespace ekeys
{
    namespace ai
    {

        namespace
        {
            constexpr uint16_t kAiImgW = 428;
            constexpr uint16_t kAiImgH = 142;
            constexpr uint32_t kAiFrameBytes =
                static_cast<uint32_t>(kAiImgW) * kAiImgH * 2;
        } // namespace

        AiFramePool &AiFramePool::instance()
        {
            static AiFramePool inst;
            return inst;
        }

        AiFramePool::AiFramePool()
            : lock_(portMUX_INITIALIZER_UNLOCKED)
        {
            memset(slots_, 0, sizeof(slots_));
            memset(dscs_, 0, sizeof(dscs_));
        }

        bool AiFramePool::ensureAllocated()
        {
            if (allocated_)
            {
                return true;
            }

            for (int i = 0; i < kAiSlotCount; ++i)
            {
                slots_[i].px = static_cast<uint8_t *>(
                    heap_caps_malloc(kAiFrameBytes, MALLOC_CAP_SPIRAM));
                if (slots_[i].px == nullptr)
                {
                    LOG_ERROR("AI", "frame slot %d alloc failed (%u bytes PSRAM)",
                              i, static_cast<unsigned>(kAiFrameBytes));
                    /* 回收已分配槽，保持未分配状态可重试 */
                    for (int j = 0; j < i; ++j)
                    {
                        heap_caps_free(slots_[j].px);
                        slots_[j].px = nullptr;
                    }
                    return false;
                }

                lv_img_dsc_t &d = dscs_[i];
                d.header.always_zero = 0;
                d.header.w = kAiImgW;
                d.header.h = kAiImgH;
                d.header.cf = LV_IMG_CF_TRUE_COLOR;
                d.data = slots_[i].px;
                d.data_size = kAiFrameBytes;
            }

            allocated_ = true;
            LOG_INFO("AI", "frame pool: %u slots x %u bytes PSRAM",
                     static_cast<unsigned>(kAiSlotCount),
                     static_cast<unsigned>(kAiFrameBytes));
            return true;
        }

        int AiFramePool::acquireWriteSlot()
        {
            int found = -1;
            portENTER_CRITICAL(&lock_);
            for (int i = 0; i < kAiSlotCount; ++i)
            {
                if (slots_[i].state == kFree)
                {
                    slots_[i].state = kWriting;
                    found = i;
                    break;
                }
            }
            portEXIT_CRITICAL(&lock_);
            return found;
        }

        void AiFramePool::transition(int slot, uint8_t from, uint8_t to)
        {
            portENTER_CRITICAL(&lock_);
            if (slots_[slot].state == from)
            {
                slots_[slot].state = to;
            }
            portEXIT_CRITICAL(&lock_);
        }

        bool AiFramePool::commitWriteSlot(int slot)
        {
            transition(slot, kWriting, kQueued);

            /* 投递 AiFrame 消息（不搬像素，只带槽号）。队列满 → 槽回 Free
             * 防止 FramePool 卡死（docs/13 §4.2）。 */
            DisplayMessage msg;
            msg.type = DisplayMessageType::AiFrame;
            msg.ai_slot = static_cast<uint8_t>(slot);
            if (!DisplayTask::instance().post(msg, 0))
            {
                transition(slot, kQueued, kFree);
                return false;
            }
            return true;
        }

        void AiFramePool::abandonWriteSlot(int slot)
        {
            transition(slot, kWriting, kFree);
        }

        void AiFramePool::presentSlot(int slot)
        {
            bool ok = false;
            portENTER_CRITICAL(&lock_);
            if (slots_[slot].state == kQueued)
            {
                slots_[slot].state = kDisplaying;
                ok = true;
            }
            portEXIT_CRITICAL(&lock_);
            if (!ok)
            {
                return;
            }

            int prev = -1;
            portENTER_CRITICAL(&lock_);
            if (displaying_ >= 0 && displaying_ != slot)
            {
                prev = displaying_;
            }
            displaying_ = slot;
            portEXIT_CRITICAL(&lock_);
            if (prev >= 0)
            {
                transition(prev, kDisplaying, kFree);
            }
        }

        void AiFramePool::adoptDisplaying(int slot)
        {
            transition(slot, kWriting, kDisplaying);
            portENTER_CRITICAL(&lock_);
            displaying_ = slot;
            portEXIT_CRITICAL(&lock_);
        }

        void AiFramePool::releaseDisplaySlot(int slot)
        {
            transition(slot, kDisplaying, kFree);
            portENTER_CRITICAL(&lock_);
            if (displaying_ == slot)
            {
                displaying_ = -1;
            }
            portEXIT_CRITICAL(&lock_);
        }

        void AiFramePool::resetForIdle()
        {
            portENTER_CRITICAL(&lock_);
            for (int i = 0; i < kAiSlotCount; ++i)
            {
                if (slots_[i].state == kQueued || slots_[i].state == kDisplaying)
                {
                    slots_[i].state = kFree;
                }
            }
            displaying_ = -1;
            portEXIT_CRITICAL(&lock_);
        }

        int AiFramePool::freeSlotCount() const
        {
            int n = 0;
            portENTER_CRITICAL(&lock_);
            for (int i = 0; i < kAiSlotCount; ++i)
            {
                if (slots_[i].state == kFree)
                {
                    ++n;
                }
            }
            portEXIT_CRITICAL(&lock_);
            return n;
        }

    } // namespace ai
} // namespace ekeys
