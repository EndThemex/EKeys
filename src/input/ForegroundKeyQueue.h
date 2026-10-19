/*
 * ForegroundKeyQueue.h
 *
 * 前台玩法按键边沿队列（docs/13-input-router-design.md §3.6）。
 *
 * 为什么不复用 DisplayMessage：
 *   - DisplayMessage 队列长度 10，单条约 1.2KB；
 *   - 游戏一次按下+释放就是 2 条，快速连按会丢边沿。
 *
 * 本队列 32 项 × 8B，SPSC：MainTask push、DisplayTask pop。
 * 满时丢弃新事件（dropped_ 计数），避免阻塞输入路径。
 */

#ifndef EKEYS_INPUT_FOREGROUND_KEY_QUEUE_H
#define EKEYS_INPUT_FOREGROUND_KEY_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace ekeys
{
    struct ForegroundKeyEvent
    {
        uint8_t key{0};
        bool pressed{false};
        uint32_t timestamp_ms{0};
    };

    class ForegroundKeyQueue
    {
    public:
        /* 环形缓冲有效容量 = kCapacity - 1（留一格判满） */
        static constexpr size_t kCapacity = 32;

        static ForegroundKeyQueue &instance();

        ForegroundKeyQueue(const ForegroundKeyQueue &) = delete;
        ForegroundKeyQueue &operator=(const ForegroundKeyQueue &) = delete;

        /* MainTask 上下文：写入一条边沿；满返回 false */
        bool push(const ForegroundKeyEvent &ev);

        /* DisplayTask 上下文：取出一条边沿；空返回 false */
        bool pop(ForegroundKeyEvent &out);

        void clear();

        /* 统计用：因队列满丢弃的事件数（近似值） */
        uint32_t droppedCount() const;

    private:
        ForegroundKeyQueue() = default;

        ForegroundKeyEvent buf_[kCapacity]{};
        volatile uint32_t head_{0};
        volatile uint32_t tail_{0};
        uint32_t dropped_{0};
        mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    };

} // namespace ekeys

#endif // EKEYS_INPUT_FOREGROUND_KEY_QUEUE_H
