#ifndef UI_AUDIOSCREENSECONDARY_H
#define UI_AUDIOSCREENSECONDARY_H

#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 音效页二级屏（Sound Pad board）：布局与键位映射二级页一致
 *   - 左侧信息卡 60×134（标题 / 图标 / 播放键位指示）
 *   - 右侧键位卡 358×134：11 个音效格 82×36 按 3/4/4 三行排布
 *     （与物理矩阵键位一致），第 0 行第 4 列空槽为状态格。
 *
 * 输入（DisplayTask 透传 LV_EVENT_KEY）：
 *   旋钮 ESC(双击) → 回 AudioScreen 一级屏
 *   旋钮 ENTER → 停止播放
 *   矩阵键 1~11：DisplayTask 在 UI_SCREEN_AUDIO_SECONDARY 下截胡为 trigger(key)，
 *   不进本屏事件回调。
 */

void ui_AudioScreenSecondary_screen_init(void);
void ui_AudioScreenSecondary_screen_destroy(void);
void ui_event_AudioScreenSecondary(lv_event_t *e);

/* 刷新 11 个键位的绑定显示（files[k-1] = 键 k 文件名，空串 = 未绑定） */
void ui_AudioScreenSecondary_set_pads(const char files[11][25]);

/* 播放高亮：key = 1~11 高亮该键位格；0 = 清除全部高亮 */
void ui_AudioScreenSecondary_set_playing(uint8_t key);

/* 旋钮 ENTER 停止播放（实现在 AudioPad.cpp，extern "C" 双向桥接） */
void ui_audio_pad_stop(void);

extern lv_obj_t *ui_AudioScreenSecondary;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
