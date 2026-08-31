/*
 * parseConfigSetCommand.h
 *
 * CMD_CONFIG_SET 字段解析与原子写入（FEATURE_DOC §6；阶段 04 任务 4.5）。
 *
 * - 所有 DeviceSettings 修改经 Configuration::mutateSettings()（锁内原子完成）
 * - 变更键在 mutator 返回后逐键经 Configuration::saveSetting() 持久化
 * - 变更字段记录新旧值日志
 * - 副作用（work_mode 重建键盘 / WiFi 调度等）通过 ConfigSetResult 交由上层
 *   cmd_config handler 执行，本文件不依赖任务与协议模块
 */

#ifndef EKEYS_CONFIG_PARSE_CONFIG_SET_COMMAND_H
#define EKEYS_CONFIG_PARSE_CONFIG_SET_COMMAND_H

#include <ArduinoJson.h>
#include <stdint.h>

namespace ekeys {

struct ConfigSetResult {
    bool any_changed = false;
    bool work_mode_changed = false;
    uint8_t work_mode = 0;      // 新 work_mode（0~2）
    bool profile_changed = false;
    bool wifi_changed = false;  // wifi_switch / ssid / password / connect_host
};

/*
 * 解析 data["config"] 对象并原子写入。
 * cfg 为空对象时返回 false。未知字段忽略并 LOG_WARNING。
 */
bool parseConfigSetCommand(JsonObject cfg, ConfigSetResult &result);

}  // namespace ekeys

#endif  // EKEYS_CONFIG_PARSE_CONFIG_SET_COMMAND_H
