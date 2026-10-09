/*
 * HaEntities.h
 *
 * 11 键 binary_sensor 实体描述常量（docs/11 §2）。
 *
 * object_id / unique_id 固定为 key_N / ekeys-key-N，不随当前键映射内容变
 * （映射可变，实体身份必须稳定；HA 侧显示名可在 HA 里改）。
 * key 字段 = key_id（1~11），BinarySensorStateResponse 直接复用。
 */

#ifndef EKEYS_HA_HA_ENTITIES_H
#define EKEYS_HA_HA_ENTITIES_H

#include <stdint.h>

namespace ekeys
{
    namespace ha
    {

        constexpr uint8_t kHaEntityCount = 11;
        constexpr uint8_t kHaEntityKeyFirst = 1;
        constexpr uint8_t kHaEntityKeyLast = kHaEntityCount;

        struct HaEntityDesc
        {
            uint8_t key_id;
            const char *object_id;
            const char *name;
            const char *unique_id;
        };

        constexpr HaEntityDesc kHaEntities[kHaEntityCount] = {
            {1, "key_1", "Key 1", "ekeys-key-1"},
            {2, "key_2", "Key 2", "ekeys-key-2"},
            {3, "key_3", "Key 3", "ekeys-key-3"},
            {4, "key_4", "Key 4", "ekeys-key-4"},
            {5, "key_5", "Key 5", "ekeys-key-5"},
            {6, "key_6", "Key 6", "ekeys-key-6"},
            {7, "key_7", "Key 7", "ekeys-key-7"},
            {8, "key_8", "Key 8", "ekeys-key-8"},
            {9, "key_9", "Key 9", "ekeys-key-9"},
            {10, "key_10", "Key 10", "ekeys-key-10"},
            {11, "key_11", "Key 11", "ekeys-key-11"},
        };

    } // namespace ha
} // namespace ekeys

#endif // EKEYS_HA_HA_ENTITIES_H
