/**
 * @file ui_FlipClock.h
 * @brief 主页翻页时钟组件（HH:MM:SS 六张等宽卡片 + 两组冒号，数字变化播放翻页动画）
 */
#ifndef UI_FLIPCLOCK_H
#define UI_FLIPCLOCK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

  /**
   * @brief 在 parent 上创建翻页时钟（绝对布局，占满 428x142 屏幕区域）
   * @param parent 宿主屏幕对象（ui_MainScreen）
   */
  void ui_FlipClock_create(lv_obj_t *parent);

  /** 销毁翻页时钟（随主屏销毁时调用） */
  void ui_FlipClock_destroy(void);

  /**
   * @brief 更新时间并驱动翻页动画 / 冒号闪烁
   * @param time_text "HH:MM:SS"（长度 >= 8，冒号位校验，非法输入直接忽略）
   */
  void ui_FlipClock_update(const char *time_text);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* UI_FLIPCLOCK_H */
