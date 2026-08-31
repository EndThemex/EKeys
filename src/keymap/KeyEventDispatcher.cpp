/*
 * KeyEventDispatcher.cpp
 *
 * 见 KeyEventDispatcher.h。
 */

#include "KeyEventDispatcher.h"

#include <string.h>

#include "config/Configuration.h"
#include "keymap/KeyResolver.h"
#include "rgb/ClickHighlight.h"
#include "voice/VoiceRecognizer.h"

namespace ekeys
{

    namespace
    {

        constexpr const char *kAsrFunctionKey = "KEY_FUNCTION_ASR";

        const KeyResolver *g_resolver = nullptr;

        bool isVoiceTriggerKey(uint8_t key_id)
        {
            DeviceSettings snap;
            Configuration::instance().snapshot(snap);
            if (snap.voice_trigger_key != 0 && key_id == snap.voice_trigger_key)
            {
                return true;
            }
            /* function_key 命中 KEY_FUNCTION_ASR 同样触发（FEATURE_DOC §11.2） */
            if (g_resolver != nullptr)
            {
                return g_resolver->get(key_id).function_key == kAsrFunctionKey;
            }
            return false;
        }

    } // namespace

    void KeyEventDispatcher::init(const KeyResolver *resolver)
    {
        g_resolver = resolver;
    }

    void KeyEventDispatcher::onKeyEdge(uint8_t key_id, bool pressed)
    {
        if (isVoiceTriggerKey(key_id))
        {
            if (pressed)
            {
                VoiceRecognizer::instance().startCapture();
            }
            else
            {
                VoiceRecognizer::instance().finishCapture();
            }
            return;
        }

        ClickHighlight::onKeyEdge(key_id, pressed);
    }

} // namespace ekeys
