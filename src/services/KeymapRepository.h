/*
 * KeymapRepository.h
 *
 * keymap{N}.ini 读写（FEATURE_DOC §3.2、§3.3）。
 * 只做文件层：Profile 路径由 Configuration::getProfileConfigPath() 提供。
 */

#ifndef EKEYS_SERVICES_KEYMAP_REPOSITORY_H
#define EKEYS_SERVICES_KEYMAP_REPOSITORY_H

#include <array>

#include "input/MatrixScanner.h" // kMatrixKeyCount
#include "utils/keymap_types.h"

namespace ekeys
{

    class KeymapRepository
    {
    public:
        using KeymapArray = std::array<KeyMapping, kMatrixKeyCount + 1>; // 下标 1~11

        /*
         * 读取 profile 对应的 keymap{N}.ini。
         * 文件不存在或所有段均被显式清空时返回 false，调用方回退默认映射；
         * 文件内缺失的 [keyN] 段（该键从未配置过）由本函数补默认 a~k。
         */
        bool loadProfile(const char *path, KeymapArray &out);

        /*
         * 批量保存键映射到 profile 文件：单次 load + N 次内存修改 + 单次落盘，
         * 避免 0x06 多键同步时逐键读/写 SPIFFS。
         * keyMask 为位掩码，bit i（1~kMatrixKeyCount）置位表示保存 mappings[i]；
         * 掩码为 0 或含非法位时返回 false。
         */
        bool saveKeys(const char *path, const KeymapArray &mappings, uint16_t keyMask);

    private:
        static void splitPlus(const char *value,
                              std::array<String, kKeyMappingNormalCount> &out);
        static void splitPlus(const char *value,
                              std::array<String, kKeyMappingMacrosCount> &out);
    };

} // namespace ekeys

#endif // EKEYS_SERVICES_KEYMAP_REPOSITORY_H
