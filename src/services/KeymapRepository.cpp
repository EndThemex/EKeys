/*
 * KeymapRepository.cpp
 *
 * keymap{N}.ini 格式：
 *
 *   [key1]
 *   function_key=KEY_FUNCTION_ASR
 *   text=hello@example.com
 *   normal_key=a+b
 *   normal_key=Ctrl+c
 *
 * function_key 非空时优先使用；其次 text（文本注入，ASCII）；normal_key / macros_key 用 "+" 分隔。
 * 组合键写在 normal_key（"Ctrl"+"c" 拆两槽，修饰键槽经 KeyNameTable
 * 解析为 0xE0~0xE7）；macros_key 仅存储/协议透传，KeyResolver 未实现
 * 宏播放。
 *
 * FUN 组合层（combo1_* / combo2_*）：fun_key1 / fun_key2（config.ini
 * [system]）对应物理键按住时，其它键改为触发对应组合层（优先级同单击
 * function > text > normal）；FUN 键本身不产生 HID 输出。
 */

#include "KeymapRepository.h"

#include <string.h>

#include <SimpleIni.h>

#include "logging/LogManager.h"
#include "services/ConfigStore.h"

namespace ekeys
{

    namespace
    {

        constexpr const char *kKeySectionPrefix = "key";

        /*
         * 把 "a+b+c" 拆成数组；超过容量的项丢弃并告警。
         * 模板参数 N 为目标数组容量。
         */
        template <size_t N>
        void splitPlusImpl(const char *value, std::array<String, N> &out)
        {
            for (auto &s : out)
            {
                s = String();
            }
            if (value == nullptr || value[0] == '\0')
            {
                return;
            }

            const char *p = value;
            size_t idx = 0;
            while (p != nullptr && *p != '\0')
            {
                if (idx >= N)
                {
                    LOG_WARNING("KEYMAP", "too many entries, truncated");
                    break;
                }
                const char *sep = strchr(p, '+');
                size_t len = (sep != nullptr) ? static_cast<size_t>(sep - p)
                                              : strlen(p);
                out[idx++] = String(p).substring(0, len);
                p = (sep != nullptr) ? sep + 1 : nullptr;
            }
        }

    } // namespace

    void KeymapRepository::splitPlus(const char *value,
                                     std::array<String, kKeyMappingNormalCount> &out)
    {
        splitPlusImpl<kKeyMappingNormalCount>(value, out);
    }

    void KeymapRepository::splitPlus(const char *value,
                                     std::array<String, kKeyMappingMacrosCount> &out)
    {
        splitPlusImpl<kKeyMappingMacrosCount>(value, out);
    }

    bool KeymapRepository::loadProfile(const char *path, KeymapArray &out)
    {
        CSimpleIniA ini(true, false, false);
        if (!ConfigStore::loadGlobal(path, ini))
        {
            return false;
        }

        uint8_t valid_cnt = 0;
        for (uint8_t i = 1; i <= kMatrixKeyCount; ++i)
        {
            char section[12];
            snprintf(section, sizeof(section), "%s%u", kKeySectionPrefix,
                     static_cast<unsigned>(i));

            KeyMapping &m = out[i];
            const char *fk = ini.GetValue(section, "function_key", nullptr);
            const char *tx = ini.GetValue(section, "text", nullptr);
            const char *nk = ini.GetValue(section, "normal_key", nullptr);
            const char *mk = ini.GetValue(section, "macros_key", nullptr);

            if (fk == nullptr && tx == nullptr && nk == nullptr && mk == nullptr)
            {
                /*
                 * [keyN] 段/键缺失 = 该键从未配置过（用户显式清空时
                 * saveKeys 仍会写入空串键），回落默认 a~k，避免
                 * "只保存过部分键"的 ini 让其余键加载后静默失效。
                 */
                m = KeyMapping{};
                m.function_key = kDefaultKeyMapping[i];
                m.valid = true;
                ++valid_cnt;
                continue;
            }

            m.function_key = (fk != nullptr) ? fk : "";
            m.text_key = (tx != nullptr) ? tx : "";
            splitPlus((nk != nullptr) ? nk : "", m.normal_key);
            splitPlus((mk != nullptr) ? mk : "", m.macros_key);
            const char *c1f = ini.GetValue(section, "combo1_function_key", nullptr);
            const char *c1t = ini.GetValue(section, "combo1_text", nullptr);
            const char *c1n = ini.GetValue(section, "combo1_normal_key", nullptr);
            const char *c2f = ini.GetValue(section, "combo2_function_key", nullptr);
            const char *c2t = ini.GetValue(section, "combo2_text", nullptr);
            const char *c2n = ini.GetValue(section, "combo2_normal_key", nullptr);
            m.combo1_function_key = (c1f != nullptr) ? c1f : "";
            m.combo1_text_key = (c1t != nullptr) ? c1t : "";
            splitPlus((c1n != nullptr) ? c1n : "", m.combo1_normal_key);
            m.combo2_function_key = (c2f != nullptr) ? c2f : "";
            m.combo2_text_key = (c2t != nullptr) ? c2t : "";
            splitPlus((c2n != nullptr) ? c2n : "", m.combo2_normal_key);
            m.valid = (m.function_key.length() > 0) ||
                      (m.text_key.length() > 0) ||
                      (m.normal_key[0].length() > 0) ||
                      (m.macros_key[0].length() > 0) ||
                      (m.combo1_function_key.length() > 0) ||
                      (m.combo1_text_key.length() > 0) ||
                      (m.combo1_normal_key[0].length() > 0) ||
                      (m.combo2_function_key.length() > 0) ||
                      (m.combo2_text_key.length() > 0) ||
                      (m.combo2_normal_key[0].length() > 0);
            if (m.valid)
            {
                ++valid_cnt;
            }
        }

        if (valid_cnt == 0)
        {
            LOG_WARNING("KEYMAP", "%s has no valid mapping", path);
            return false;
        }
        return true;
    }

    bool KeymapRepository::saveKeys(const char *path, const KeymapArray &mappings,
                                    uint16_t keyMask)
    {
        constexpr uint16_t kValidMask =
            static_cast<uint16_t>((1u << (kMatrixKeyCount + 1)) - 1) & ~1u;
        if (keyMask == 0 || (keyMask & ~kValidMask) != 0)
        {
            return false;
        }

        CSimpleIniA ini(true, false, false);
        if (ConfigStore::exists(path))
        {
            ConfigStore::loadGlobal(path, ini);
        }

        for (uint8_t i = 1; i <= kMatrixKeyCount; ++i)
        {
            if ((keyMask & (1u << i)) == 0)
            {
                continue;
            }
            const KeyMapping &mapping = mappings[i];

            char section[12];
            snprintf(section, sizeof(section), "%s%u", kKeySectionPrefix,
                     static_cast<unsigned>(i));

            /* 空串键也写入：loadProfile 依赖"键存在但为空"区分显式清空 */
            ini.SetValue(section, "function_key", mapping.function_key.c_str());
            ini.SetValue(section, "text", mapping.text_key.c_str());

            String nk;
            for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n)
            {
                if (mapping.normal_key[n].length() == 0)
                {
                    break;
                }
                if (n > 0)
                {
                    nk += '+';
                }
                nk += mapping.normal_key[n];
            }
            ini.SetValue(section, "normal_key", nk.c_str());

            String mk;
            for (uint8_t n = 0; n < kKeyMappingMacrosCount; ++n)
            {
                if (mapping.macros_key[n].length() == 0)
                {
                    break;
                }
                if (n > 0)
                {
                    mk += '+';
                }
                mk += mapping.macros_key[n];
            }
            ini.SetValue(section, "macros_key", mk.c_str());

            /* FUN 组合层（combo1/combo2）："+" 分隔，空串也写入保持显式清空语义 */
            ini.SetValue(section, "combo1_function_key",
                         mapping.combo1_function_key.c_str());
            ini.SetValue(section, "combo1_text",
                         mapping.combo1_text_key.c_str());
            String c1n;
            for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n)
            {
                if (mapping.combo1_normal_key[n].length() == 0)
                {
                    break;
                }
                if (n > 0)
                {
                    c1n += '+';
                }
                c1n += mapping.combo1_normal_key[n];
            }
            ini.SetValue(section, "combo1_normal_key", c1n.c_str());

            ini.SetValue(section, "combo2_function_key",
                         mapping.combo2_function_key.c_str());
            ini.SetValue(section, "combo2_text",
                         mapping.combo2_text_key.c_str());
            String c2n;
            for (uint8_t n = 0; n < kKeyMappingNormalCount; ++n)
            {
                if (mapping.combo2_normal_key[n].length() == 0)
                {
                    break;
                }
                if (n > 0)
                {
                    c2n += '+';
                }
                c2n += mapping.combo2_normal_key[n];
            }
            ini.SetValue(section, "combo2_normal_key", c2n.c_str());
        }

        return ConfigStore::saveGlobal(path, ini);
    }

} // namespace ekeys
