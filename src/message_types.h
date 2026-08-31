/*
 * message_types.h
 *
 * MainTask → DisplayTask 消息定义（FEATURE_DOC §8.3）。
 *
 * 本期（阶段 02）仅声明 SETTING_UPDATE / TIME_UPDATE；
 * 后续阶段按需追加 KEY_INPUT / MODULE_STATUS / ASR_RECORDING_STATE /
 * PC_STATUS_UPDATE / HA_STATUS_UPDATE / MUSIC_PLAYER_UPDATE /
 * KEYMAP_PROFILE_UPDATE / ACTION_INPUT。
 */

#ifndef EKEYS_MESSAGE_TYPES_H
#define EKEYS_MESSAGE_TYPES_H

#include <stdint.h>

namespace ekeys
{

  enum class DisplayMessageType : uint8_t
  {
    SettingUpdate = 0,
    TimeUpdate = 1,
    /* 以下为预留，本阶段不投递 */
    ActionInput = 2,
    KeyInput = 3,
    ModuleStatus = 4,
    AsrRecording = 5,
    PcStatus = 6,
    HaStatus = 7,
    MusicPlayer = 8,
    KeymapProfile = 9,
  };

  /*
   * 时间载荷：单字符串（"HH:MM:SS"），避免每个组件各自管理 buffer。
   *
   * 阶段 02 之后会扩展其它 union 字段（SettingUpdateSettingsSnapshot 等）。
   */
  struct DisplayMessage
  {
    DisplayMessageType type;
    union
    {
      char time_text[16]; /* "HH:MM:SS" + '\0' */
    } payload;
  };

} // namespace ekeys

#endif // EKEYS_MESSAGE_TYPES_H
