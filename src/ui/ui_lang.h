/**
 * @file ui_lang.h
 * @brief UI 语言状态与多语言标题刷新收口。
 *
 * 语言值与 DeviceSettings::ui_lang 一致：0=中文（默认） 1=English。
 * 数据流：Configuration -> MainTask（diff 检测）-> SettingUpdate ->
 * DisplayTask::applySetting -> ui_lang_set（本模块统一刷新各屏标题）。
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** 当前 UI 语言（0=中文 1=English） */
  uint8_t ui_lang_current(void);

  /** 设置语言并立即刷新所有一级屏标题（越界值钳位为 English） */
  void ui_lang_set(uint8_t lang);

  /** 按当前语言刷新所有一级屏标题（屏幕未创建时跳过） */
  void ui_lang_apply(void);

#ifdef __cplusplus
}
#endif
