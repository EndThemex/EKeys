/*
 * ForegroundKeyQueue.cpp
 *
 * 见 ForegroundKeyQueue.h。
 */

#include "input/ForegroundKeyQueue.h"

namespace ekeys
{
    ForegroundKeyQueue &ForegroundKeyQueue::instance()
    {
        static ForegroundKeyQueue inst;
        return inst;
    }

    bool ForegroundKeyQueue::push(const ForegroundKeyEvent &ev)
    {
        bool ok = false;
        taskENTER_CRITICAL(&lock_);
        const uint32_t next = (head_ + 1) % kCapacity;
        if (next != tail_)
        {
            buf_[head_] = ev;
            head_ = next;
            ok = true;
        }
        else
        {
            ++dropped_;
        }
        taskEXIT_CRITICAL(&lock_);
        return ok;
    }

    bool ForegroundKeyQueue::pop(ForegroundKeyEvent &out)
    {
        bool ok = false;
        taskENTER_CRITICAL(&lock_);
        if (tail_ != head_)
        {
            out = buf_[tail_];
            tail_ = (tail_ + 1) % kCapacity;
            ok = true;
        }
        taskEXIT_CRITICAL(&lock_);
        return ok;
    }

    void ForegroundKeyQueue::clear()
    {
        taskENTER_CRITICAL(&lock_);
        head_ = 0;
        tail_ = 0;
        dropped_ = 0;
        taskEXIT_CRITICAL(&lock_);
    }

    uint32_t ForegroundKeyQueue::droppedCount() const
    {
        uint32_t value = 0;
        taskENTER_CRITICAL(&lock_);
        value = dropped_;
        taskEXIT_CRITICAL(&lock_);
        return value;
    }

} // namespace ekeys
