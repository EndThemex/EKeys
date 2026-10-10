/**
 * @file ui_lang.c
 * @brief UI 语言状态与多语言标题刷新收口。
 *
 * 注意：一级页标题样式字体为 ui_font_FontCKJGT28（仅 ASCII，无汉字字形，
 * 与 BebasNeue 同源生成参数 -r 0x20-0x7f），中文文本必须叠加
 * ui_font_FontCKJGT24（含常用汉字表）逐对象覆盖，否则 LVGL 8 静默渲染空白。
 */
#include "ui_lang.h"

#include "ui.h"
#include "ui_KeyMapped.h"
#include "ui_MusicScreen.h"
#include "ui_AudioScreen.h"
#include "ui_PcStatusScreen.h"
#include "ui_HaScreen.h"
#include "ui_SettingScreen.h"

/* 0=中文（默认） 1=English，与 Configuration 默认值一致 */
static uint8_t s_ui_lang = 0;

uint8_t ui_lang_current(void)
{
    return s_ui_lang;
}

void ui_lang_set(uint8_t lang)
{
    if (lang > 1)
    {
        lang = 1;
    }
    s_ui_lang = lang;
    ui_lang_apply();
}

void ui_lang_apply(void)
{
    ui_KeyMapped_apply_language(s_ui_lang);
    ui_MusicScreen_apply_language(s_ui_lang);
    ui_AudioScreen_apply_language(s_ui_lang);
    ui_PcStatusScreen_apply_language(s_ui_lang);
    ui_HaScreen_apply_language(s_ui_lang);
    ui_SettingScreen_apply_language(s_ui_lang);
}
