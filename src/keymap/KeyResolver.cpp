/*
 * KeyResolver.cpp
 *
 * 默认映射（FEATURE_DOC §3.1）：
 *
 *   Key ID 1~11 → "a" "b" "c" "d" "e" "f" "g" "h" "i" "j" "k"
 *
 * 用于阶段 01 的最小 HID 自检。
 */

#include "KeyResolver.h"

#include "KeyNameTable.h"
#include "logging/LogManager.h"

namespace ekeys {

namespace {
constexpr const char *kDefaultMapping[kMatrixKeyCount + 1] = {
    "",
    "a", "b", "c", "d", "e",
    "f", "g", "h", "i", "j",
    "k"
};
}  // namespace

KeyResolver::KeyResolver()
{
    for (uint8_t i = 0; i <= kMatrixKeyCount; ++i) {
        map_[i].valid = false;
    }
}

void KeyResolver::begin()
{
    loadDefaults();
    LOG_INFO("KEY_RES", "loaded %d default mappings", kMatrixKeyCount);
}

void KeyResolver::end()
{
    for (uint8_t i = 0; i <= kMatrixKeyCount; ++i) {
        map_[i].valid = false;
    }
}

void KeyResolver::loadDefaults()
{
    for (uint8_t i = 1; i <= kMatrixKeyCount; ++i) {
        KeyMapping &m = map_[i];
        m.function_key = kDefaultMapping[i];
        for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n) {
            m.normal_key[n] = (n == 0) ? String(kDefaultMapping[i]) : String();
        }
        for (uint8_t n = 0; n < kKeyMappingMacrosCount; ++n) {
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
    if (keyId < 1 || keyId > kMatrixKeyCount) {
        return;
    }
    const KeyMapping &m = map_[keyId];
    if (!m.valid) {
        return;
    }

    if (m.function_key.length() > 0) {
        ResolvedKey r = resolveKeyWithModifier(m.function_key);
        if (r.keycode) {
            keyboard.press(r.keycode, r.modifier);
        }
        return;
    }
    for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n) {
        if (m.normal_key[n].length() == 0) {
            break;
        }
        ResolvedKey r = resolveKeyWithModifier(m.normal_key[n]);
        if (r.keycode) {
            keyboard.press(r.keycode, r.modifier);
        }
    }
}

void KeyResolver::release(uint8_t keyId, IKeyboard &keyboard)
{
    if (keyId < 1 || keyId > kMatrixKeyCount) {
        return;
    }
    const KeyMapping &m = map_[keyId];
    if (!m.valid) {
        return;
    }

    if (m.function_key.length() > 0) {
        ResolvedKey r = resolveKeyWithModifier(m.function_key);
        if (r.keycode) {
            keyboard.release(r.keycode);
        }
        return;
    }
    for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n) {
        if (m.normal_key[n].length() == 0) {
            break;
        }
        ResolvedKey r = resolveKeyWithModifier(m.normal_key[n]);
        if (r.keycode) {
            keyboard.release(r.keycode);
        }
    }
}

void KeyResolver::releaseAllForKey(uint8_t keyId, IKeyboard &keyboard)
{
    release(keyId, keyboard);
}

}  // namespace ekeys
