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
        // 默认表（a~k）收口在 utils/keymap_types.h 的 kDefaultKeyMapping，
        // 与协议 0x05 上报、KeymapRepository 补段共用。
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
        resetState();
    }

    void KeyResolver::resetState()
    {
        fun1_held_ = false;
        fun2_held_ = false;
        fire_layer_.fill(kLayerSingle);
    }

    uint8_t KeyResolver::funKey1() const
    {
        return config_.settings().fun_key1;
    }

    uint8_t KeyResolver::funKey2() const
    {
        return config_.settings().fun_key2;
    }

    uint8_t KeyResolver::activeFunLayer() const
    {
        if (fun1_held_)
        {
            return 1;
        }
        if (fun2_held_)
        {
            return 2;
        }
        return 0;
    }

    void KeyResolver::loadDefaults()
    {
        keymapFillDefaults(map_);
    }

    const KeyMapping &KeyResolver::get(uint8_t keyId) const
    {
        return map_[keyId <= kMatrixKeyCount ? keyId : 0];
    }

    bool KeyResolver::channelsEmpty(
        const String &fk, const String &tk,
        const std::array<String, kKeyMappingNormalCount> &nk)
    {
        return fk.isEmpty() && tk.isEmpty() && nk[0].isEmpty();
    }

    void KeyResolver::firePress(
        uint8_t keyId, const String &fk, const String &tk,
        const std::array<String, kKeyMappingNormalCount> &nk,
        IKeyboard &keyboard) const
    {
        if (fk.length() > 0)
        {
            ResolvedKey r = resolveKeyWithModifier(fk);
            if (r.keycode)
            {
                keyboard.press(r.keycode, r.modifier);
            }
            return;
        }
        /*
         * 文本注入通道：按键触发整串输出一次（ASCII），
         * release 无对应动作，仅回写 LED 边沿。
         */
        if (tk.length() > 0)
        {
            keyboard.type(tk);
            notifyLedEdge(keyId, true);
            return;
        }
        for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n)
        {
            if (nk[n].length() == 0)
            {
                break;
            }
            ResolvedKey r = resolveKeyWithModifier(nk[n]);
            if (r.keycode)
            {
                keyboard.press(r.keycode, r.modifier);
            }
        }

        notifyLedEdge(keyId, true);
    }

    void KeyResolver::fireRelease(
        uint8_t keyId, const String &fk, const String &tk,
        const std::array<String, kKeyMappingNormalCount> &nk,
        IKeyboard &keyboard) const
    {
        if (fk.length() > 0)
        {
            ResolvedKey r = resolveKeyWithModifier(fk);
            if (r.keycode)
            {
                keyboard.release(r.keycode);
            }
            return;
        }
        if (tk.length() > 0)
        {
            /* 文本在 press 时已整串输出完毕，release 无键可松 */
            notifyLedEdge(keyId, false);
            return;
        }
        for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n)
        {
            if (nk[n].length() == 0)
            {
                break;
            }
            ResolvedKey r = resolveKeyWithModifier(nk[n]);
            if (r.keycode)
            {
                keyboard.release(r.keycode);
            }
        }

        notifyLedEdge(keyId, false);
    }

    void KeyResolver::press(uint8_t keyId, IKeyboard &keyboard)
    {
        if (keyId < 1 || keyId > kMatrixKeyCount)
        {
            return;
        }

        /*
         * FUN 组合键：按住期间其它键改走组合层，FUN 键本身不产生 HID 输出。
         * press 幂等（MainTask 同 tick 预扫描会提前调用一次）。
         */
        const uint8_t fk1 = funKey1();
        const uint8_t fk2 = funKey2();
        if (fk1 != 0 && keyId == fk1)
        {
            fun1_held_ = true;
            return;
        }
        if (fk2 != 0 && keyId == fk2)
        {
            fun2_held_ = true;
            return;
        }

        const KeyMapping &m = map_[keyId];
        if (!m.valid)
        {
            return;
        }

        /* FUN1 组合层优先于 FUN2；组合层未配置时回落单击 */
        if (fun1_held_ && !channelsEmpty(m.combo1_function_key,
                                         m.combo1_text_key, m.combo1_normal_key))
        {
            firePress(keyId, m.combo1_function_key, m.combo1_text_key,
                      m.combo1_normal_key, keyboard);
            fire_layer_[keyId] = kLayerFun1;
            return;
        }
        if (fun2_held_ && !channelsEmpty(m.combo2_function_key,
                                         m.combo2_text_key, m.combo2_normal_key))
        {
            firePress(keyId, m.combo2_function_key, m.combo2_text_key,
                      m.combo2_normal_key, keyboard);
            fire_layer_[keyId] = kLayerFun2;
            return;
        }

        firePress(keyId, m.function_key, m.text_key, m.normal_key, keyboard);
        fire_layer_[keyId] = kLayerSingle;
    }

    void KeyResolver::release(uint8_t keyId, IKeyboard &keyboard)
    {
        if (keyId < 1 || keyId > kMatrixKeyCount)
        {
            return;
        }

        const uint8_t fk1 = funKey1();
        const uint8_t fk2 = funKey2();
        if (fk1 != 0 && keyId == fk1)
        {
            fun1_held_ = false;
            return;
        }
        if (fk2 != 0 && keyId == fk2)
        {
            fun2_held_ = false;
            return;
        }

        const KeyMapping &m = map_[keyId];
        if (!m.valid)
        {
            return;
        }

        /*
         * 按下时记录的触发层决定释放动作；组合触发后即使 FUN 键先松开，
         * 仍等该键松开时释放，保证 press/release 配对。
         */
        switch (fire_layer_[keyId])
        {
        case kLayerFun1:
            fireRelease(keyId, m.combo1_function_key, m.combo1_text_key,
                        m.combo1_normal_key, keyboard);
            break;
        case kLayerFun2:
            fireRelease(keyId, m.combo2_function_key, m.combo2_text_key,
                        m.combo2_normal_key, keyboard);
            break;
        default:
            fireRelease(keyId, m.function_key, m.text_key, m.normal_key,
                        keyboard);
            break;
        }
        fire_layer_[keyId] = kLayerSingle;
    }

    void KeyResolver::releaseAllForKey(uint8_t keyId, IKeyboard &keyboard)
    {
        release(keyId, keyboard);
    }

    void KeyResolver::notifyLedEdge(uint8_t keyId, bool pressed) const
    {
        /*
         * 占位：阶段 06 RGB 接入后在此回写点击高亮 / 呼吸灯状态
         * （FEATURE_DOC §9 RGB_CLICK_MODE）。
         */
        (void)keyId;
        (void)pressed;
    }

} // namespace ekeys
