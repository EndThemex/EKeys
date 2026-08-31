/*
 * KeyEventDispatcher.h
 *
 * 按键边沿分发（阶段 06 任务 6.12 / 6.16）：
 *
 *   - 命中语音触发键（voice_trigger_key 或 function_key=KEY_FUNCTION_ASR）
 *     → VoiceRecognizer startCapture / finishCapture（FEATURE_DOC §11.2）
 *   - 其余边沿 → ClickHighlight::onKeyEdge（RGB 点击高亮）
 *
 * 由 MainTask 在 5ms tick 的按键边沿处调用。
 */

#ifndef EKEYS_KEYMAP_KEY_EVENT_DISPATCHER_H
#define EKEYS_KEYMAP_KEY_EVENT_DISPATCHER_H

#include <stdint.h>

namespace ekeys
{

    class KeyResolver;

    class KeyEventDispatcher
    {
    public:
        KeyEventDispatcher() = delete;

        /* MainTask::begin() 注入（读映射判断 function_key 触发） */
        static void init(const KeyResolver *resolver);

        static void onKeyEdge(uint8_t key_id, bool pressed);
    };

} // namespace ekeys

#endif // EKEYS_KEYMAP_KEY_EVENT_DISPATCHER_H
