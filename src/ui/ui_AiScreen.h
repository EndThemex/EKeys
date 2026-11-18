/**
 * @file ui_AiScreen.h
 * @brief AI Mode 一级入口页：图标 + 标题（与其它一级页一致）。
 *
 * 键值约定：LEFT→音效页，RIGHT→PC 状态页，ENTER→AI 功能页
 * （ui_AiScreenSecondary，进入后才启动串口/WiFi 收发），ESC→主页。
 * 矩阵键 101~111 在本页不截胡（HID 照发）。
 */
#ifndef UI_AI_SCREEN_H
#define UI_AI_SCREEN_H

#include "ui_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern lv_obj_t *ui_AiScreen;

    void ui_AiScreen_screen_init(void);
    void ui_AiScreen_screen_destroy(void);
    void ui_AiScreen_apply_language(uint8_t lang);

    /* 屏幕级 KEY 事件处理（LV_EVENT_KEY：导航） */
    void ui_event_AiScreen(lv_event_t *e);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
