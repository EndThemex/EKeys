#ifndef UI_KEYMAPPEDSECONDARY_H
#define UI_KEYMAPPEDSECONDARY_H

#include <stddef.h>
#include <stdint.h>

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
	void ui_KeyMappedSecondary_set_profile(const char *icon, const char *name, const char *file_name);
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
	 * 键映射二级页旋钮切 Profile 请求（MainTask.cpp 定义，C 链接）。
	 * step：+1=顺时针下一个，-1=逆时针上一个；MainTask 合并消费。
	 * g_main_task 未就绪时返回 false，由 UI 侧忽略。
	 */
	bool ui_keymap_request_profile_switch(int step);

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