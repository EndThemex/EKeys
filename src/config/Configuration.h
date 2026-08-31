/*
 * Configuration.h
 *
 * 配置层顶层单例（FEATURE_DOC §3.2、§3.3、§6；ARCHITECTURE §3.7）。
 *
 * - 持有 DeviceSettings 与 FreeRTOS 互斥量 mutex_
 * - 全局设置走 /config.ini；键映射走 keymap{N}.ini（委托 KeymapRepository）
 * - 所有公开接口内部加锁，调用方无需自行同步
 */

#ifndef EKEYS_CONFIG_CONFIGURATION_H
#define EKEYS_CONFIG_CONFIGURATION_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <functional>

#include "config/DeviceSettings.h"
#include "input/MatrixScanner.h" // kMatrixKeyCount
#include "utils/keymap_types.h"

namespace ekeys
{

    class KeymapRepository;

    class Configuration
    {
    public:
        static constexpr uint8_t CONFIG_PROFILE_COUNT = 8;

        using KeymapArray = std::array<KeyMapping, kMatrixKeyCount + 1>; // 下标 1~11

        static Configuration &instance();

        Configuration(const Configuration &) = delete;
        Configuration &operator=(const Configuration &) = delete;

        /*
         * 注入 KeymapRepository（AppContext::init() 中调用一次）。
         */
        void setRepository(KeymapRepository *repo) { repo_ = repo; }

        /*
         * 读取 /config.ini；文件不存在时使用默认值并输出
         * LOG_INFO("Using default config")。
         */
        void load();

        /*
         * 更新单个设置项：先写内存 settings_，再持久化到 /config.ini。
         */
        bool saveSetting(const char *key, const char *value);
        bool saveSetting(const char *key, int value);

        /*
         * 加载当前激活 Profile 的键映射；无文件返回 false。
         */
        bool loadActiveProfileKeyMapping(KeymapArray &out);

        /*
         * 保存当前 Profile 单键映射到 keymap{N}.ini。
         */
        bool saveKeyMapping(uint8_t keyId, const KeyMapping &mapping);

        /*
         * 切换激活 Profile：更新内存 + 持久化 active_keymap_profile。
         * 注意：调用后需重新 loadActiveProfileKeyMapping() 刷新 KeyResolver。
         */
        bool switchActiveProfile(uint8_t idx);

        const DeviceSettings &settings() const { return settings_; }
        uint8_t activeProfile() const { return settings_.active_keymap_profile; }

        /*
         * 加锁复制当前设置快照（协议层读取统一入口，FEATURE_DOC §6）。
         */
        void snapshot(DeviceSettings &out);

        /*
         * 加锁执行 mutator 修改内存 settings_（阶段 04 任务 4.5 原子写入）。
         * 持久化由调用方在返回后经 saveSetting() 逐键完成（其内部自行加锁，
         * 不能在 mutator 内调用）。
         */
        using SettingsMutator = std::function<void(DeviceSettings &)>;
        bool mutateSettings(const SettingsMutator &mutator);

        /* Profile 路径与显示名（FEATURE_DOC §3.3），idx 0~7 */
        const char *getProfileConfigPath(uint8_t idx) const;
        const char *getProfileDisplayName(uint8_t idx) const;
        const char *getProfileIconPath(uint8_t idx) const;

        /* 供后续阶段（ConfigStore 直连场景）共享互斥量 */
        void lock();
        void unlock();

    private:
        Configuration();
        ~Configuration();

        void loadGlobalSettings_locked();

        DeviceSettings settings_;
        SemaphoreHandle_t mutex_;
        KeymapRepository *repo_; // 不持有所有权

        /* 预生成的 Profile 路径 / 名称（避免运行时格式化） */
        char config_paths_[CONFIG_PROFILE_COUNT][16];
        char icon_paths_[CONFIG_PROFILE_COUNT][16];
    };

} // namespace ekeys

#endif // EKEYS_CONFIG_CONFIGURATION_H
