/*
 * KeyResolver.h
 *
 * 阶段 01 持有 11 个应用键的硬编码默认 KeyMapping；
 * 阶段 03 改为从 SPIFFS 加载（见 03.md）。
 */

#ifndef EKEYS_KEYMAP_KEY_RESOLVER_H
#define EKEYS_KEYMAP_KEY_RESOLVER_H

#include <array>

#include "output/IKeyboard.h"
#include "utils/keymap_types.h"

namespace ekeys {

class KeyResolver {
public:
    KeyResolver();

    void begin();
    void end();

    /*
     * 应用键 ID（1~11）→ KeyMapping。
     * 越界返回 valid=false 的占位。
     */
    const KeyMapping &get(uint8_t keyId) const;

    /*
     * 把按键按当前映射注入到 IKeyboard。
     * MainTask 在每个 5ms tick 中调用。
     */
    void press(uint8_t keyId, IKeyboard &keyboard);
    void release(uint8_t keyId, IKeyboard &keyboard);

    /*
     * 强制释放该键可能已注入的 HID 状态。
     */
    void releaseAllForKey(uint8_t keyId, IKeyboard &keyboard);

private:
    void loadDefaults();

    std::array<KeyMapping, kMatrixKeyCount + 1> map_;  // 下标 1~11
};

}  // namespace ekeys

#endif  // EKEYS_KEYMAP_KEY_RESOLVER_H
