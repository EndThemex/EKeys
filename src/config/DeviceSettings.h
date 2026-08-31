/*
 * DeviceSettings.h
 *
 * 全局设置 POD（FEATURE_DOC §6），字段与桌面 App CMD_CONFIG_SET 一致。
 * 阶段 03：先全部 = 0 占位，实际生效随阶段 04~06 逐步接入。
 */

#ifndef EKEYS_CONFIG_DEVICE_SETTINGS_H
#define EKEYS_CONFIG_DEVICE_SETTINGS_H

#include <stdint.h>

namespace ekeys {

/*
 * 字符串字段容量：
 *   SSID ≤ 32 字节（IEEE 802.11），密码 ≤ 64 字节（PSK），
 *   百度 ASR 凭证各预留 64 字节。
 */
struct DeviceSettings {
    /* WiFi / 主机连接 */
    uint8_t wifi_switch;
    uint8_t connect_host;
    char    wifi_ssid[33];
    char    wifi_password[65];

    /* 工作模式（0=USB 1=BLE 2=2.4G） */
    uint8_t work_mode;

    /* RGB */
    uint8_t rgb_mode;
    uint8_t rgb_single_colar;
    uint8_t rgb_click_mode;
    uint8_t rgb_brightness;

    /* 屏幕 */
    uint8_t tft_theme;
    uint8_t tft_brightness;

    /* 音频 */
    uint8_t device_volume;
    uint8_t audio_enable;
    uint8_t power_mode;

    /* 语音 */
    uint8_t  voice_enable;
    uint8_t  voice_trigger_key;
    uint16_t voice_max_record_ms;
    uint8_t  voice_auto_enter;
    uint16_t voice_dev_pid;
    char     voice_cuid[33];
    char     voice_baidu_api_key[65];
    char     voice_baidu_secret_key[65];

    /* PC 状态位掩码 */
    uint32_t pc_status_mask;

    /* 当前激活键映射 Profile（0~7） */
    uint8_t active_keymap_profile;

    /* 阶段 07：设备元数据（0x02/0x04 写入，system 节持久化） */
    uint32_t config_version;
    char     device_name[33];
    char     serial_number[33];
};

}  // namespace ekeys

#endif  // EKEYS_CONFIG_DEVICE_SETTINGS_H
