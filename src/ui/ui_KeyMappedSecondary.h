#ifndef UI_KEYMAPPEDSECONDARY_H
#define UI_KEYMAPPEDSECONDARY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

	extern void ui_KeyMappedSecondary_screen_init(void);
	extern void ui_KeyMappedSecondary_screen_destroy(void);
	extern void ui_event_KeyMappedSecondaryScreen(lv_event_t *e);
	extern void ui_event_ButtonLeftKeyMappedSecondary(lv_event_t *e);
	extern void ui_event_ButtonRightKeyMappedSecondary(lv_event_t *e);
	extern lv_obj_t *ui_KeyMappedSecondary;

	void ui_KeyMappedSecondary_bind_main_screen_summary(lv_obj_t *icon_label,
																											lv_obj_t *icon_image,
																											lv_obj_t *profile_name);
	/*
	 * 设置二级页当前展示的 profile 摘要（图标 / 名称 / 文件名）。
	 * update_main_summary=false（预览消息）时跳过主屏 summary 的 bind
	 * 缓存刷新，避免预览态污染一级屏显示。
	 */
	void ui_KeyMappedSecondary_set_profile(const char *icon, const char *name,
																				 const char *file_name,
																				 bool update_main_summary);
	void ui_KeyMappedSecondary_set_profile_icon_source(const char *file_path, const char *fallback_symbol);
	void ui_KeyMappedSecondary_set_profile_icon_image_data(const uint8_t *image_data,
																												 size_t image_size,
																												 uint16_t width,
																												 uint16_t height,
																												 const char *fallback_symbol);
	void ui_KeyMappedSecondary_set_key_label(unsigned int key_index, const char *text);

	/*
	 * 标记 FUN 组合键（1~11，0=未配置），对应键位格子整体着色标识。
	 * 由 DisplayTask 在推送键映射 Profile 时从 DeviceSettings 同步。
	 */
	void ui_KeyMappedSecondary_set_fun_keys(unsigned int fun_key1, unsigned int fun_key2);

	/*
	 * 标记"已应用"的 profile（1~8，0=未知）：当前展示的 profile 与之一致时，
	 * 序号格子红框加粗 + 序号 / 名称文字红色；预览未应用的显示默认灰。
	 */
	void ui_KeyMappedSecondary_set_applied_index(unsigned int index);

	/* "应用中..." 等待遮罩（applied 消息 / 3s 兜底 timer 收尾） */
	void ui_KeyMappedSecondary_show_apply_waiting(void);
	void ui_KeyMappedSecondary_hide_apply_waiting(void);

	/*
	 * 键映射二级页旋钮预览请求（MainTask.cpp 定义，C 链接）。
	 * step：+1=顺时针下一个，-1=逆时针上一个；MainTask 合并消费，
	 * 只切预览视图，不应用不落盘。g_main_task 未就绪时返回 false。
	 */
	bool ui_keymap_request_profile_switch(int step);

	/*
	 * 单击确认应用当前预览的 Profile（MainTask.cpp 定义，C 链接）。
	 * MainTask 消费后回发 applied 消息收尾等待遮罩。未就绪时返回 false，
	 * 调用方回滚遮罩。
	 */
	bool ui_keymap_request_profile_apply(void);

	/*
	 * 把第 key_id 个应用键（1~11）映射到二级页的 3×3 槽位并高亮。
	 * key_id 越界或无对应槽位（10/11）时清空高亮。
	 * A1 修复：让应用键 1~11 直接作为 KEYMAPPED → KEYMAPPED_SECONDARY 的入口
	 * 并把即将被编辑的键标红。
	 */
	void ui_KeyMappedSecondary_set_focus(unsigned int key_id);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif