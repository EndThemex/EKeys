#ifndef UI_AUDIOSCREEN_H
#define UI_AUDIOSCREEN_H

#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 音效页一级屏（Sound Pad entry）：
 *   - 图标 + 标题（与 KeyMapped / MusicScreen / SettingScreen 等一级页一致风格）
 *   - 旋钮 ENTER → 进入 AudioScreenSecondary（11 键音效板）
 *   - 旋钮 RIGHT → PC_STATUS / LEFT → MUSIC / ESC → MAIN
 *
 * 矩阵键 1~11 在一级屏不被截胡（HID 专用，由 DisplayTask 既有逻辑丢弃）。
 * 真正的音效触发在 AudioScreenSecondary。
 */

void ui_AudioScreen_screen_init(void);
void ui_AudioScreen_screen_destroy(void);
void ui_event_AudioScreen(lv_event_t *e);

extern lv_obj_t *ui_AudioScreen;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
