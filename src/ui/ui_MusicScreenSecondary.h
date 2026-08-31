#ifndef UI_MUSICSCREENSECONDARY_H
#define UI_MUSICSCREENSECONDARY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void ui_MusicScreenSecondary_screen_init(void);
extern void ui_MusicScreenSecondary_screen_destroy(void);
extern void ui_event_MusicScreenSecondary(lv_event_t * e);
extern void ui_event_ButtonExitMusicSecondary(lv_event_t * e);
extern void ui_MusicScreenSecondary_set_player_state(const char *title,
											 const char *artist,
											 const char *player_name,
											 const char *lyric_current,
											 const char *lyric_next,
											 bool connected,
													 bool is_playing,
													 bool is_paused,
											 bool can_prev,
							 bool can_next,
									 uint16_t current_seconds,
							 uint16_t total_seconds);

typedef enum {
	UI_MUSIC_CONTROL_NONE = 0,
	UI_MUSIC_CONTROL_PREV,
	UI_MUSIC_CONTROL_TOGGLE,
	UI_MUSIC_CONTROL_NEXT,
} ui_music_control_request_t;

extern void ui_MusicScreenSecondary_request_control(ui_music_control_request_t request);
extern uint8_t ui_MusicScreenSecondary_consume_control_request(void);

extern lv_obj_t * ui_MusicScreenSecondary;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif