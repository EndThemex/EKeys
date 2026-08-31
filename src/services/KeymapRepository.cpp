/*
 * KeymapRepository.cpp
 *
 * keymap{N}.ini 格式：
 *
 *   [key1]
 *   function_key=KEY_FUNCTION_ASR
 *   normal_key=a+b
 *   macros_key=Ctrl+c
 *
 * function_key 非空时优先使用；normal_key / macros_key 用 "+" 分隔。
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
            const char *fk = ini.GetValue(section, "function_key", "");
            const char *nk = ini.GetValue(section, "normal_key", "");
            const char *mk = ini.GetValue(section, "macros_key", "");

            m.function_key = fk;
            splitPlus(nk, m.normal_key);
            splitPlus(mk, m.macros_key);
            m.valid = (m.function_key.length() > 0) ||
                      (m.normal_key[0].length() > 0) ||
                      (m.macros_key[0].length() > 0);
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

    bool KeymapRepository::saveKey(const char *path, uint8_t keyId,
                                   const KeyMapping &mapping)
    {
        if (keyId < 1 || keyId > kMatrixKeyCount)
        {
            return false;
        }

        CSimpleIniA ini(true, false, false);
        if (ConfigStore::exists(path))
        {
            ConfigStore::loadGlobal(path, ini);
        }

        char section[12];
        snprintf(section, sizeof(section), "%s%u", kKeySectionPrefix,
                 static_cast<unsigned>(keyId));

        ini.SetValue(section, "function_key", mapping.function_key.c_str());

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

        return ConfigStore::saveGlobal(path, ini);
    }

} // namespace ekeys
