/*
 * ui_screen_tag.h
 *
 * 屏幕标签（从 ui_KeyMapped.h 拆出，去掉 LVGL 依赖）。
 *
 * 拆分原因（docs/13-input-router-design.md Step 0）：
 *   InputRouter 只需要 ui_screen_tag_t 这一个 enum，不应把 lvgl.h
 *   拖进输入层；拆分后 host 单测也更容易。
 *
 * CUSTOM: do not regenerate. 若 SquareLine 重新导出 ui_KeyMapped.h，
 * 请保留本文件与其中的 UI_SCREEN_GAME 追加项。
 */

#ifndef UI_SCREEN_TAG_H
#define UI_SCREEN_TAG_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        UI_SCREEN_UNKNOWN = 0,
        UI_SCREEN_MAIN,
        UI_SCREEN_KEYMAPPED,
        UI_SCREEN_KEYMAPPED_SECONDARY,
        UI_SCREEN_MUSIC,
        UI_SCREEN_MUSIC_SECONDARY,
        UI_SCREEN_AUDIO,
        UI_SCREEN_AUDIO_SECONDARY,
        UI_SCREEN_PC_STATUS,
        UI_SCREEN_PC_STATUS_SECONDARY,
        UI_SCREEN_HA,
        UI_SCREEN_HA_SECONDARY,
        UI_SCREEN_SETTING,
        UI_SCREEN_SETTING_SECONDARY,
        /* 前台玩法屏（程序化创建，不走 SquareLine；docs/13） */
        UI_SCREEN_GAME
    } ui_screen_tag_t;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* UI_SCREEN_TAG_H */
