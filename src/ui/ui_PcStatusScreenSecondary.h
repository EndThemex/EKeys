#ifndef UI_PCSTATUSSCREENSECONDARY_H
#define UI_PCSTATUSSCREENSECONDARY_H

#ifdef __cplusplus
extern "C"
{
#endif

  extern void ui_PcStatusScreenSecondary_screen_init(void);
  extern void ui_PcStatusScreenSecondary_screen_destroy(void);
  extern void ui_event_PcStatusScreenSecondary(lv_event_t *e);
  extern void ui_event_ButtonExitPcStatusSecondary(lv_event_t *e);

  extern lv_obj_t *ui_PcStatusScreenSecondary;

  /* 二级屏渲染 setter，由 ui_PcStatusScreen_set_* 收口转发，勿直接调用
   * （联网状态已移除，原 NET 槽位改为磁盘空间） */
  extern void ui_PcStatusScreenSecondary_set_disk_space_percent(float pct);
  extern void ui_PcStatusScreenSecondary_set_net_up_kbps(float kbps);
  extern void ui_PcStatusScreenSecondary_set_net_down_kbps(float kbps);
  extern void ui_PcStatusScreenSecondary_set_cpu_percent(float pct);
  extern void ui_PcStatusScreenSecondary_set_cpu_temp_c(float c);
  extern void ui_PcStatusScreenSecondary_set_mem_percent(float pct);
  extern void ui_PcStatusScreenSecondary_set_disk_io_percent(float pct);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif