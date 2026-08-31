/*
 * KeyResolver.cpp
 *
 * 默认映射（FEATURE_DOC §3.1）：
 *
 *   Key ID 1~11 → "a" "b" "c" "d" "e" "f" "g" "h" "i" "j" "k"
 *
 * 阶段 03：begin() 优先从 Configuration 加载当前 Profile 的
 * keymap{N}.ini；文件缺失 / 无有效映射时回退上述默认值。
 */

#include "KeyResolver.h"

#include "KeyNameTable.h"
#include "config/Configuration.h"
#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {
        constexpr const char *kDefaultMapping[kMatrixKeyCount + 1] = {
            "",
            "a", "b", "c", "d", "e",
            "f", "g", "h", "i", "j",
            "k"};
    } // namespace

    KeyResolver::KeyResolver(Configuration &config)
        : config_(config)
    {
        for (uint8_t i = 0; i <= kMatrixKeyCount; ++i)
        {
            map_[i].valid = false;
        }
    }

    void KeyResolver::begin()
    {
        /*
         * 优先加载 SPIFFS 中的 keymap{N}.ini；
         * 首次上电无文件时回退默认映射（a~k）。
         */
        if (config_.loadActiveProfileKeyMapping(map_))
        {
            LOG_INFO("KEY_RES", "loaded keymap profile %u from SPIFFS",
                     static_cast<unsigned>(config_.activeProfile()));
        }
        else
        {
            loadDefaults();
            LOG_WARNING("KEY_RES", "keymap file missing, using defaults");
        }
    }

    void KeyResolver::end()
    {
        for (uint8_t i = 0; i <= kMatrixKeyCount; ++i)
        {
            map_[i].valid = false;
        }
    }

    void KeyResolver::loadDefaults()
    {
        for (uint8_t i = 1; i <= kMatrixKeyCount; ++i)
        {
            KeyMapping &m = map_[i];
            m.function_key = kDefaultMapping[i];
            for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n)
            {
                m.normal_key[n] = (n == 0) ? String(kDefaultMapping[i]) : String();
            }
            for (uint8_t n = 0; n < kKeyMappingMacrosCount; ++n)
            {
                m.macros_key[n] = String();
            }
            m.valid = true;
        }
    }

    const KeyMapping &KeyResolver::get(uint8_t keyId) const
    {
        return map_[keyId <= kMatrixKeyCount ? keyId : 0];
    }

    void KeyResolver::press(uint8_t keyId, IKeyboard &keyboard)
    {
        if (keyId < 1 || keyId > kMatrixKeyCount)
        {
            return;
        }
        const KeyMapping &m = map_[keyId];
        if (!m.valid)
        {
            return;
        }

        if (m.function_key.length() > 0)
        {
            ResolvedKey r = resolveKeyWithModifier(m.function_key);
            if (r.keycode)
            {
                keyboard.press(r.keycode, r.modifier);
            }
            return;
        }
        for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n)
        {
            if (m.normal_key[n].length() == 0)
            {
                break;
            }
            ResolvedKey r = resolveKeyWithModifier(m.normal_key[n]);
            if (r.keycode)
            {
                keyboard.press(r.keycode, r.modifier);
            }
        }

        notifyLedEdge(keyId, true);
    }

    void KeyResolver::release(uint8_t keyId, IKeyboard &keyboard)
    {
        if (keyId < 1 || keyId > kMatrixKeyCount)
        {
            return;
        }
        const KeyMapping &m = map_[keyId];
        if (!m.valid)
        {
            return;
        }

        if (m.function_key.length() > 0)
        {
            ResolvedKey r = resolveKeyWithModifier(m.function_key);
            if (r.keycode)
            {
                keyboard.release(r.keycode);
            }
            return;
        }
        for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n)
        {
            if (m.normal_key[n].length() == 0)
            {
                break;
            }
            ResolvedKey r = resolveKeyWithModifier(m.normal_key[n]);
            if (r.keycode)
            {
                keyboard.release(r.keycode);
            }
        }

        notifyLedEdge(keyId, false);
    }

    void KeyResolver::releaseAllForKey(uint8_t keyId, IKeyboard &keyboard)
    {
        release(keyId, keyboard);
    }

    void KeyResolver::notifyLedEdge(uint8_t keyId, bool pressed)
    {
        /*
         * 占位：阶段 06 RGB 接入后在此回写点击高亮 / 呼吸灯状态
         * （FEATURE_DOC §9 RGB_CLICK_MODE）。
         */
        (void)keyId;
        (void)pressed;
    }

} // namespace ekeys
