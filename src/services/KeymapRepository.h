/*
 * KeymapRepository.h
 *
 * keymap{N}.ini 读写（FEATURE_DOC §3.2、§3.3）。
 * 只做文件层：Profile 路径由 Configuration::getProfileConfigPath() 提供。
 */

#ifndef EKEYS_SERVICES_KEYMAP_REPOSITORY_H
#define EKEYS_SERVICES_KEYMAP_REPOSITORY_H

#include <array>

#include "input/MatrixScanner.h"  // kMatrixKeyCount
#include "utils/keymap_types.h"

namespace ekeys {

class KeymapRepository {
public:
    using KeymapArray = std::array<KeyMapping, kMatrixKeyCount + 1>;  // 下标 1~11

    /*
     * 读取 profile 对应的 keymap{N}.ini。
     * 文件不存在或为空映射时返回 false，调用方回退默认映射。
     */
    bool loadProfile(const char *path, KeymapArray &out);

    /*
     * 保存单个键的映射到 profile 文件。
     */
    bool saveKey(const char *path, uint8_t keyId, const KeyMapping &mapping);

private:
    static void splitPlus(const char *value,
                          std::array<String, kKeyMappingNormalCount> &out);
    static void splitPlus(const char *value,
                          std::array<String, kKeyMappingMacrosCount> &out);
};

}  // namespace ekeys

#endif  // EKEYS_SERVICES_KEYMAP_REPOSITORY_H
