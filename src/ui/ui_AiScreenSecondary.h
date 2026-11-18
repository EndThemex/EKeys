/**
 * @file ui_AiScreenSecondary.h
 * @brief AI Mode 二级功能页：图像全屏渲染 + HUD。
 *
 * 只有进入本页（DisplayTask 激活钩子比对 UI_SCREEN_AI_SECONDARY）才会
 * 启动 AiMode 的串口 / WiFi 收发与 hello 握手；仅停留在一级入口页不启用。
 *
 * 键值约定：ENTER 切换 HUD，ESC 返回一级入口页；矩阵键 101~111 忽略。
 */
#ifndef UI_AI_SCREEN_SECONDARY_H
#define UI_AI_SCREEN_SECONDARY_H

#include <stdint.h>

#include "ui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern lv_obj_t *ui_AiScreenSecondary;

    /* 页面状态（与 ai::AiPageState 数值一一对应） */
    enum UI_AI_STATE
    {
        UI_AI_STATE_WAITING = 0,
        UI_AI_STATE_STREAMING = 1,
        /* 无 WiFi：走 USB 串口通信（原 WIFI_OFF 语义） */
        UI_AI_STATE_SERIAL = 2,
        UI_AI_STATE_ERROR = 3,
    };

    void ui_AiScreenSecondary_screen_init(void);
    void ui_AiScreenSecondary_screen_destroy(void);
    void ui_AiScreenSecondary_apply_language(uint8_t lang);

    void ui_event_AiScreenSecondary(lv_event_t *e);

    /* 换帧：只改 lv_img 数据源，不阻塞刷新 */
    void ui_AiScreenSecondary_show_frame(const lv_img_dsc_t *dsc);

    /* 状态文案：info 可传 nullptr（使用默认副行） */
    void ui_AiScreenSecondary_set_state(uint8_t state, const char *info);

    /* HUD 统计（2Hz） */
    void ui_AiScreenSecondary_set_stats(uint16_t fps_x10, uint32_t bytes_per_s,
                                        uint8_t format, uint16_t dropped,
                                        uint16_t width, uint16_t height);

    /* 旋钮单击：HUD 显示/隐藏（不影响接收） */
    void ui_AiScreenSecondary_toggle_hud(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
