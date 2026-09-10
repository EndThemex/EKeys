/*
 * KeyResolver.h
 *
 * 阶段 01 持有 11 个应用键的硬编码默认 KeyMapping；
 * 阶段 03 改为从 Configuration（SPIFFS keymap{N}.ini）加载，
 * 加载失败时回退默认映射。
 */

#ifndef EKEYS_KEYMAP_KEY_RESOLVER_H
#define EKEYS_KEYMAP_KEY_RESOLVER_H

#include <array>

#include "input/MatrixScanner.h" // kMatrixKeyCount
#include "output/IKeyboard.h"
#include "utils/keymap_types.h"

namespace ekeys
{

    class Configuration;

    class KeyResolver
    {
    public:
        explicit KeyResolver(Configuration &config);

        void begin();
        void end();

        /*
         * 应用键 ID（1~11）→ KeyMapping。
         * 越界返回 valid=false 的占位。
         */
        const KeyMapping &get(uint8_t keyId) const;

        /* FUN 组合键配置（0=未配置），读 Configuration settings() */
        uint8_t funKey1() const;
        uint8_t funKey2() const;

        /*
         * 当前激活的组合层：0=无（单击）、1=FUN1 按住、2=FUN2 按住
         * （两键同按 FUN1 优先）。供 MainTask 驱动键映射二级页
         * 切换显示组合层摘要。
         */
        uint8_t activeFunLayer() const;

        /*
         * 把按键按当前映射注入到 IKeyboard。
         * MainTask 在每个 5ms tick 中调用；FUN 键的 press 幂等，
         * 支持同 tick 预扫描先置位 FUN 状态再处理其余键。
         */
        void press(uint8_t keyId, IKeyboard &keyboard);
        void release(uint8_t keyId, IKeyboard &keyboard);

        /*
         * 强制释放该键可能已注入的 HID 状态。
         */
        void releaseAllForKey(uint8_t keyId, IKeyboard &keyboard);

        /*
         * 重置运行时状态（FUN held 标志、触发层记录）。
         * reloadKeymap / 配置变更时调用，避免状态卡住。
         */
        void resetState();

    private:
        void loadDefaults();

        /* 触发层编号：fire_layer_ / fireChannels 的取值 */
        static constexpr uint8_t kLayerSingle = 0;
        static constexpr uint8_t kLayerFun1 = 1;
        static constexpr uint8_t kLayerFun2 = 2;

        /*
         * 按通道组（function > text > normal）注入/释放 HID，
         * 保留原有单击路径的 LED 回调行为；单击与两个组合层共用。
         */
        void firePress(uint8_t keyId, const String &fk, const String &tk,
                       const std::array<String, kKeyMappingNormalCount> &nk,
                       IKeyboard &keyboard) const;
        void fireRelease(uint8_t keyId, const String &fk, const String &tk,
                         const std::array<String, kKeyMappingNormalCount> &nk,
                         IKeyboard &keyboard) const;

        /* 组合层三通道是否全空 */
        static bool channelsEmpty(const String &fk, const String &tk,
                                  const std::array<String, kKeyMappingNormalCount> &nk);

        /*
         * 按键边沿结束后的 LED 回写（阶段 06 RGB 接入后实现，当前占位）。
         */
        void notifyLedEdge(uint8_t keyId, bool pressed) const;

        Configuration &config_;                               // 不持有所有权
        std::array<KeyMapping, kMatrixKeyCount + 1> map_;     // 下标 1~11
        bool fun1_held_;                                      // FUN 键 1 按住
        bool fun2_held_;                                      // FUN 键 2 按住
        std::array<uint8_t, kMatrixKeyCount + 1> fire_layer_; // 该键本次按下走的层
    };

} // namespace ekeys

#endif // EKEYS_KEYMAP_KEY_RESOLVER_H
