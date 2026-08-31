#include "ui.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MUSIC_SECONDARY_SCREEN_WIDTH 428
#define MUSIC_SECONDARY_VISUAL_WIDTH 428
#define MUSIC_SECONDARY_VISUAL_HEIGHT 26
#define MUSIC_SECONDARY_SHELL_WIDTH 428
#define MUSIC_SECONDARY_SHELL_HEIGHT 116
#define MUSIC_SECONDARY_ALBUM_WIDTH 104
#define MUSIC_SECONDARY_ALBUM_HEIGHT 92
#define MUSIC_SECONDARY_META_WIDTH 304
#define MUSIC_SECONDARY_META_HEIGHT 92

lv_obj_t * ui_MusicScreenSecondary = NULL;
static lv_obj_t * ui_MusicSecondaryButtonLeft = NULL;
static lv_obj_t * ui_MusicSecondaryButtonRight = NULL;
static lv_obj_t * ui_MusicSecondaryButtonEnter = NULL;
static lv_obj_t * ui_MusicSecondaryButtonExit = NULL;
static lv_obj_t * ui_MusicSecondaryVisualHost = NULL;
static lv_obj_t * ui_MusicSecondaryShell = NULL;
static lv_obj_t * ui_MusicSecondaryAlbumCard = NULL;
static lv_obj_t * ui_MusicSecondaryMetaCard = NULL;
static lv_obj_t * ui_MusicSecondaryRecord = NULL;
static lv_obj_t * ui_MusicSecondaryRecordLabel = NULL;
static lv_obj_t * ui_MusicSecondaryRecordHalo = NULL;
static lv_obj_t * ui_MusicSecondaryRecordGlint = NULL;
static lv_obj_t * ui_MusicSecondaryTonearm = NULL;
static lv_obj_t * ui_MusicSecondaryTonearmHead = NULL;
static lv_obj_t * ui_MusicSecondaryTonearmPivot = NULL;
static lv_obj_t * ui_MusicSecondaryTitle = NULL;
static lv_obj_t * ui_MusicSecondarySubtitle = NULL;
static lv_obj_t * ui_MusicSecondaryProgress = NULL;
static lv_obj_t * ui_MusicSecondaryCurrentTime = NULL;
static lv_obj_t * ui_MusicSecondaryTotalTime = NULL;
static lv_obj_t * ui_MusicSecondaryHintLeft = NULL;
static lv_obj_t * ui_MusicSecondaryHintCenter = NULL;
static lv_obj_t * ui_MusicSecondaryHintRight = NULL;
static lv_timer_t * ui_MusicSecondaryRecordTimer = NULL;
static int16_t s_music_record_angle = 0;

static volatile uint8_t s_music_control_request = UI_MUSIC_CONTROL_NONE;
static bool s_music_connected = false;
static bool s_music_is_playing = false;
static bool s_music_is_paused = false;
static bool s_music_can_prev = false;
static bool s_music_can_next = false;
static uint16_t s_music_current_seconds = 0;
static uint16_t s_music_total_seconds = 0;
static char s_music_title[96] = "WAITING FOR PLAYER";
static char s_music_artist[64] = "";
static char s_music_player_name[32] = "PC MUSIC";

static void music_secondary_refresh_player_ui(void);
static void music_secondary_record_timer_cb(lv_timer_t *timer);

static void music_secondary_format_time(char *buffer, size_t buffer_size, uint16_t total_seconds)
{
    if (!buffer || buffer_size == 0) {
        return;
    }

    if (total_seconds == 0) {
        snprintf(buffer, buffer_size, "--:--");
        return;
    }

    const uint16_t minutes = total_seconds / 60;
    const uint16_t seconds = total_seconds % 60;
    snprintf(buffer, buffer_size, "%02u:%02u", minutes, seconds);
}

static void music_secondary_copy_utf8(char *destination, size_t destination_size, const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }

    const char *input = (source && source[0] != '\0') ? source : "";
    const size_t input_length = strlen(input);
    size_t input_index = 0;
    size_t output_index = 0;

    while (input_index < input_length && output_index < destination_size - 1) {
        const uint8_t lead_byte = (uint8_t)input[input_index];
        size_t char_length = 1;

        if ((lead_byte & 0x80U) == 0x00U) {
            char_length = 1;
        } else if ((lead_byte & 0xE0U) == 0xC0U) {
            char_length = 2;
        } else if ((lead_byte & 0xF0U) == 0xE0U) {
            char_length = 3;
        } else if ((lead_byte & 0xF8U) == 0xF0U) {
            char_length = 4;
        }

        if (input_index + char_length > input_length || output_index + char_length > destination_size - 1) {
            break;
        }

        memcpy(destination + output_index, input + input_index, char_length);
        output_index += char_length;
        input_index += char_length;
    }

    destination[output_index] = '\0';
}

static void music_secondary_forward_key(uint32_t key)
{
    lv_obj_t *active_screen = lv_scr_act();
    lv_group_t *group = lv_group_get_default();
    lv_obj_t *target = group ? lv_group_get_focused(group) : active_screen;
    if (target) {
        lv_event_send(target, LV_EVENT_KEY, (void *)key);
    }
}

static void music_secondary_style_card(lv_obj_t *obj,
                                       lv_color_t bg_color,
                                       lv_color_t border_color,
                                       lv_coord_t radius)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, border_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void music_secondary_style_control_chip(lv_obj_t *obj,
                                               lv_color_t bg_color,
                                               lv_opa_t bg_opa,
                                               lv_color_t text_color,
                                               const lv_font_t *font)
{
    lv_obj_set_style_radius(obj, 14, 0);
    lv_obj_set_style_bg_color(obj, bg_color, 0);
    lv_obj_set_style_bg_opa(obj, bg_opa, 0);
    lv_obj_set_style_text_color(obj, text_color, 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_pad_left(obj, 10, 0);
    lv_obj_set_style_pad_right(obj, 10, 0);
    lv_obj_set_style_pad_top(obj, 5, 0);
    lv_obj_set_style_pad_bottom(obj, 5, 0);
}

static void music_secondary_set_record_angle(int16_t angle_tenths)
{
    s_music_record_angle = angle_tenths;
    if (!ui_MusicSecondaryRecordGlint) {
        return;
    }

    static const lv_coord_t glint_positions[12][2] = {
        {26, 5},
        {36, 7},
        {44, 13},
        {48, 24},
        {44, 35},
        {36, 43},
        {26, 47},
        {16, 43},
        {8, 35},
        {5, 24},
        {8, 13},
        {16, 7},
    };

    const uint8_t sector = (uint8_t)((angle_tenths / 300) % 12);
    lv_obj_set_pos(ui_MusicSecondaryRecordGlint,
                   glint_positions[sector][0],
                   glint_positions[sector][1]);

    if (ui_MusicSecondaryRecordHalo) {
        const lv_opa_t halo_opa = s_music_is_playing ? 84 : (s_music_is_paused ? 52 : 30);
        lv_obj_set_style_outline_opa(ui_MusicSecondaryRecordHalo, halo_opa, 0);
    }
}

static void music_secondary_record_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (!ui_MusicSecondaryRecord || !s_music_connected || !s_music_is_playing) {
        return;
    }

    music_secondary_set_record_angle((int16_t)((s_music_record_angle + 30) % 3600));
}

static void music_secondary_apply_optimistic_toggle_state(bool is_playing)
{
    s_music_is_playing = is_playing;
    s_music_is_paused = !is_playing;
    music_secondary_refresh_player_ui();
}

void ui_MusicScreenSecondary_request_control(ui_music_control_request_t request)
{
    s_music_control_request = (uint8_t)request;
}

uint8_t ui_MusicScreenSecondary_consume_control_request(void)
{
    const uint8_t request = s_music_control_request;
    s_music_control_request = UI_MUSIC_CONTROL_NONE;
    return request;
}

static void music_secondary_refresh_player_ui(void)
{
    if (!ui_MusicSecondaryTitle || !ui_MusicSecondarySubtitle || !ui_MusicSecondaryProgress ||
        !ui_MusicSecondaryCurrentTime || !ui_MusicSecondaryTotalTime ||
        !ui_MusicSecondaryHintLeft || !ui_MusicSecondaryHintCenter || !ui_MusicSecondaryHintRight) {
        return;
    }

    char current_text[8] = {0};
    char total_text[8] = {0};
    char subtitle_text[120] = {0};

    lv_label_set_text(ui_MusicSecondaryTitle, s_music_title);

    if (s_music_artist[0] != '\0') {
        snprintf(subtitle_text, sizeof(subtitle_text), "%s  |  %s", s_music_artist, s_music_player_name);
    } else {
        snprintf(subtitle_text, sizeof(subtitle_text), "%s", s_music_player_name);
    }
    lv_label_set_text(ui_MusicSecondarySubtitle, subtitle_text);

    music_secondary_format_time(current_text, sizeof(current_text), s_music_current_seconds);
    music_secondary_format_time(total_text, sizeof(total_text), s_music_total_seconds);
    lv_label_set_text(ui_MusicSecondaryCurrentTime, current_text);
    lv_label_set_text(ui_MusicSecondaryTotalTime, total_text);
    lv_bar_set_value(ui_MusicSecondaryProgress,
                     s_music_total_seconds > 0
                         ? (int)((1000UL * s_music_current_seconds) / s_music_total_seconds)
                         : 0,
                     LV_ANIM_OFF);

    if (!s_music_connected) {
        if (ui_MusicSecondaryRecordLabel) {
            lv_label_set_text(ui_MusicSecondaryRecordLabel, LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(ui_MusicSecondaryRecordLabel, lv_color_hex(0xFF7070), 0);
        }
        if (ui_MusicSecondaryTonearm) {
            lv_obj_set_style_transform_angle(ui_MusicSecondaryTonearm, 220, 0);
        }
        if (ui_MusicSecondaryRecordGlint) {
            lv_obj_set_style_bg_opa(ui_MusicSecondaryRecordGlint, 70, 0);
        }
    } else if (s_music_is_playing) {
        if (ui_MusicSecondaryRecordLabel) {
            lv_label_set_text(ui_MusicSecondaryRecordLabel, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(ui_MusicSecondaryRecordLabel, lv_color_hex(0xFFFFFF), 0);
        }
        if (ui_MusicSecondaryTonearm) {
            lv_obj_set_style_transform_angle(ui_MusicSecondaryTonearm, 330, 0);
        }
        if (ui_MusicSecondaryRecordGlint) {
            lv_obj_set_style_bg_opa(ui_MusicSecondaryRecordGlint, 180, 0);
        }
    } else if (s_music_is_paused) {
        if (ui_MusicSecondaryRecordLabel) {
            lv_label_set_text(ui_MusicSecondaryRecordLabel, LV_SYMBOL_PAUSE);
            lv_obj_set_style_text_color(ui_MusicSecondaryRecordLabel, lv_color_hex(0xFFE0AE), 0);
        }
        if (ui_MusicSecondaryTonearm) {
            lv_obj_set_style_transform_angle(ui_MusicSecondaryTonearm, 280, 0);
        }
        if (ui_MusicSecondaryRecordGlint) {
            lv_obj_set_style_bg_opa(ui_MusicSecondaryRecordGlint, 120, 0);
        }
    } else {
        if (ui_MusicSecondaryRecordLabel) {
            lv_label_set_text(ui_MusicSecondaryRecordLabel, LV_SYMBOL_AUDIO);
            lv_obj_set_style_text_color(ui_MusicSecondaryRecordLabel, lv_color_hex(0xA7C3FF), 0);
        }
        if (ui_MusicSecondaryTonearm) {
            lv_obj_set_style_transform_angle(ui_MusicSecondaryTonearm, 260, 0);
        }
        if (ui_MusicSecondaryRecordGlint) {
            lv_obj_set_style_bg_opa(ui_MusicSecondaryRecordGlint, 100, 0);
        }
    }

    music_secondary_set_record_angle(s_music_record_angle);

    lv_label_set_text(ui_MusicSecondaryHintLeft,
                      s_music_can_prev ? LV_SYMBOL_PREV "  PREV" : LV_SYMBOL_PREV "  WAIT");
    lv_label_set_text(ui_MusicSecondaryHintCenter, s_music_is_playing ? LV_SYMBOL_PAUSE "  PAUSE" : LV_SYMBOL_PLAY "  PLAY");
    lv_label_set_text(ui_MusicSecondaryHintRight,
                      s_music_can_next ? LV_SYMBOL_NEXT "  NEXT" : LV_SYMBOL_NEXT "  QUEUE");

    lv_obj_set_style_text_color(ui_MusicSecondaryHintLeft, lv_color_hex(0xBAC5D3), 0);
    lv_obj_set_style_text_color(ui_MusicSecondaryHintRight, lv_color_hex(0xBAC5D3), 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryHintLeft, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryHintRight, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_left(ui_MusicSecondaryHintLeft, 0, 0);
    lv_obj_set_style_pad_right(ui_MusicSecondaryHintLeft, 0, 0);
    lv_obj_set_style_pad_top(ui_MusicSecondaryHintLeft, 0, 0);
    lv_obj_set_style_pad_bottom(ui_MusicSecondaryHintLeft, 0, 0);
    lv_obj_set_style_pad_left(ui_MusicSecondaryHintRight, 0, 0);
    lv_obj_set_style_pad_right(ui_MusicSecondaryHintRight, 0, 0);
    lv_obj_set_style_pad_top(ui_MusicSecondaryHintRight, 0, 0);
    lv_obj_set_style_pad_bottom(ui_MusicSecondaryHintRight, 0, 0);
    lv_obj_set_style_text_opa(ui_MusicSecondaryHintLeft, LV_OPA_COVER, 0);
    lv_obj_set_style_text_opa(ui_MusicSecondaryHintRight, LV_OPA_COVER, 0);
}

void ui_MusicScreenSecondary_set_player_state(const char *title,
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
                                              uint16_t total_seconds)
{
    LV_UNUSED(lyric_current);
    LV_UNUSED(lyric_next);

    s_music_connected = connected;
    s_music_is_playing = is_playing;
    s_music_is_paused = is_paused;
    s_music_can_prev = can_prev;
    s_music_can_next = can_next;
    s_music_current_seconds = current_seconds;
    s_music_total_seconds = total_seconds;

    music_secondary_copy_utf8(s_music_title, sizeof(s_music_title),
                              (title && title[0] != '\0') ? title : "WAITING FOR PLAYER");
    music_secondary_copy_utf8(s_music_artist, sizeof(s_music_artist),
                              (artist && artist[0] != '\0') ? artist : "");
    music_secondary_copy_utf8(s_music_player_name, sizeof(s_music_player_name),
                              (player_name && player_name[0] != '\0') ? player_name : "PC MUSIC");

    music_secondary_refresh_player_ui();
}

void ui_event_MusicScreenSecondary(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_KEY) {
        return;
    }

    const uintptr_t key = (uintptr_t)lv_event_get_param(e);
    if (key == (uintptr_t)LV_KEY_LEFT) {
        ui_MusicScreenSecondary_request_control(UI_MUSIC_CONTROL_PREV);
    } else if (key == (uintptr_t)LV_KEY_RIGHT) {
        ui_MusicScreenSecondary_request_control(UI_MUSIC_CONTROL_NEXT);
    } else if (key == (uintptr_t)LV_KEY_ENTER) {
        if (s_music_connected) {
            music_secondary_apply_optimistic_toggle_state(!s_music_is_playing);
        }
        ui_MusicScreenSecondary_request_control(UI_MUSIC_CONTROL_TOGGLE);
    } else if (key == (uintptr_t)LV_KEY_ESC) {
        ui_set_active_screen_tag(UI_SCREEN_MUSIC);
        _ui_screen_change(&ui_MusicScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_MusicScreen_screen_init);
        lv_refr_now(NULL);
    }
}

void ui_event_ButtonExitMusicSecondary(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        music_secondary_forward_key(LV_KEY_ESC);
    }
}

static void ui_event_ButtonLeftMusicSecondary(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_MusicScreenSecondary_request_control(UI_MUSIC_CONTROL_PREV);
    }
}

static void ui_event_ButtonEnterMusicSecondary(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (s_music_connected) {
            music_secondary_apply_optimistic_toggle_state(!s_music_is_playing);
        }
        ui_MusicScreenSecondary_request_control(UI_MUSIC_CONTROL_TOGGLE);
    }
}

static void ui_event_ButtonRightMusicSecondary(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_MusicScreenSecondary_request_control(UI_MUSIC_CONTROL_NEXT);
    }
}

void ui_MusicScreenSecondary_screen_init(void)
{
    ui_MusicScreenSecondary = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_MusicScreenSecondary, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_MusicScreenSecondary, lv_color_hex(0x070A0F), 0);
    lv_obj_set_style_bg_opa(ui_MusicScreenSecondary, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_MusicScreenSecondary, 0, 0);
    lv_obj_set_style_radius(ui_MusicScreenSecondary, 0, 0);
    ui_set_active_screen_tag(UI_SCREEN_MUSIC_SECONDARY);

    ui_MusicSecondaryVisualHost = lv_obj_create(ui_MusicScreenSecondary);
    lv_obj_set_size(ui_MusicSecondaryVisualHost, MUSIC_SECONDARY_VISUAL_WIDTH, MUSIC_SECONDARY_VISUAL_HEIGHT);
    lv_obj_align(ui_MusicSecondaryVisualHost, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(ui_MusicSecondaryVisualHost, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_MusicSecondaryVisualHost, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryVisualHost, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_MusicSecondaryVisualHost, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_MusicSecondaryVisualHost, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_MusicScreen_set_visual_host(ui_MusicSecondaryVisualHost);

    ui_MusicSecondaryShell = lv_obj_create(ui_MusicScreenSecondary);
    lv_obj_set_size(ui_MusicSecondaryShell, MUSIC_SECONDARY_SHELL_WIDTH, MUSIC_SECONDARY_SHELL_HEIGHT);
    lv_obj_align(ui_MusicSecondaryShell, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(ui_MusicSecondaryShell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_MusicSecondaryShell, lv_color_hex(0x0C0F14), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryShell, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_MusicSecondaryShell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_MusicSecondaryShell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_MusicSecondaryShell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_MusicSecondaryAlbumCard = lv_obj_create(ui_MusicSecondaryShell);
    lv_obj_set_size(ui_MusicSecondaryAlbumCard, MUSIC_SECONDARY_ALBUM_WIDTH, MUSIC_SECONDARY_ALBUM_HEIGHT);
    lv_obj_align(ui_MusicSecondaryAlbumCard, LV_ALIGN_TOP_LEFT, 8, 8);
    music_secondary_style_card(ui_MusicSecondaryAlbumCard, lv_color_hex(0x161C25), lv_color_hex(0x2E3947), 18);

    ui_MusicSecondaryMetaCard = lv_obj_create(ui_MusicSecondaryShell);
    lv_obj_set_size(ui_MusicSecondaryMetaCard, MUSIC_SECONDARY_META_WIDTH, MUSIC_SECONDARY_META_HEIGHT);
    lv_obj_align(ui_MusicSecondaryMetaCard, LV_ALIGN_TOP_RIGHT, -8, 8);
    music_secondary_style_card(ui_MusicSecondaryMetaCard, lv_color_hex(0x121820), lv_color_hex(0x273242), 18);

    ui_MusicSecondaryRecord = lv_obj_create(ui_MusicSecondaryAlbumCard);
    lv_obj_set_size(ui_MusicSecondaryRecord, 62, 62);
    lv_obj_align(ui_MusicSecondaryRecord, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_clear_flag(ui_MusicSecondaryRecord, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_MusicSecondaryRecord, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui_MusicSecondaryRecord, lv_color_hex(0x0D1015), 0);
    lv_obj_set_style_border_color(ui_MusicSecondaryRecord, lv_color_hex(0x394657), 0);
    lv_obj_set_style_border_width(ui_MusicSecondaryRecord, 2, 0);
    lv_obj_set_style_outline_width(ui_MusicSecondaryRecord, 1, 0);
    lv_obj_set_style_outline_color(ui_MusicSecondaryRecord, lv_color_hex(0x1E2632), 0);
    lv_obj_set_style_transform_pivot_x(ui_MusicSecondaryRecord, 31, 0);
    lv_obj_set_style_transform_pivot_y(ui_MusicSecondaryRecord, 31, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryRecord, 0, 0);

    ui_MusicSecondaryRecordHalo = lv_obj_create(ui_MusicSecondaryAlbumCard);
    lv_obj_set_size(ui_MusicSecondaryRecordHalo, 70, 70);
    lv_obj_align_to(ui_MusicSecondaryRecordHalo, ui_MusicSecondaryRecord, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_background(ui_MusicSecondaryRecordHalo);
    lv_obj_clear_flag(ui_MusicSecondaryRecordHalo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_MusicSecondaryRecordHalo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryRecordHalo, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_MusicSecondaryRecordHalo, 0, 0);
    lv_obj_set_style_outline_width(ui_MusicSecondaryRecordHalo, 4, 0);
    lv_obj_set_style_outline_color(ui_MusicSecondaryRecordHalo, lv_color_hex(0xD33A31), 0);
    lv_obj_set_style_outline_opa(ui_MusicSecondaryRecordHalo, 30, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryRecordHalo, 0, 0);

    for (int groove_index = 0; groove_index < 3; ++groove_index) {
        lv_obj_t *groove = lv_obj_create(ui_MusicSecondaryRecord);
        const lv_coord_t groove_size = (lv_coord_t)(48 - groove_index * 12);
        lv_obj_set_size(groove, groove_size, groove_size);
        lv_obj_center(groove);
        lv_obj_clear_flag(groove, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(groove, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(groove, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(groove, 1, 0);
        lv_obj_set_style_border_color(groove, lv_color_hex(0x2B3442), 0);
        lv_obj_set_style_pad_all(groove, 0, 0);
    }

    lv_obj_t *record_label_disc = lv_obj_create(ui_MusicSecondaryRecord);
    lv_obj_set_size(record_label_disc, 22, 22);
    lv_obj_center(record_label_disc);
    lv_obj_clear_flag(record_label_disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(record_label_disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(record_label_disc, lv_color_hex(0xD33A31), 0);
    lv_obj_set_style_border_width(record_label_disc, 0, 0);
    lv_obj_set_style_pad_all(record_label_disc, 0, 0);

    lv_obj_t *record_spindle = lv_obj_create(record_label_disc);
    lv_obj_set_size(record_spindle, 6, 6);
    lv_obj_center(record_spindle);
    lv_obj_clear_flag(record_spindle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(record_spindle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(record_spindle, lv_color_hex(0xF2F5F8), 0);
    lv_obj_set_style_border_width(record_spindle, 0, 0);
    lv_obj_set_style_pad_all(record_spindle, 0, 0);

    ui_MusicSecondaryRecordGlint = lv_obj_create(ui_MusicSecondaryRecord);
    lv_obj_set_size(ui_MusicSecondaryRecordGlint, 9, 9);
    lv_obj_clear_flag(ui_MusicSecondaryRecordGlint, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_MusicSecondaryRecordGlint, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui_MusicSecondaryRecordGlint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryRecordGlint, 140, 0);
    lv_obj_set_style_border_width(ui_MusicSecondaryRecordGlint, 0, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryRecordGlint, 0, 0);

    ui_MusicSecondaryRecordLabel = lv_label_create(record_label_disc);
    lv_obj_set_width(ui_MusicSecondaryRecordLabel, 18);
    lv_obj_center(ui_MusicSecondaryRecordLabel);
    lv_obj_set_style_text_align(ui_MusicSecondaryRecordLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_MusicSecondaryRecordLabel, &lv_font_montserrat_14, 0);

    ui_MusicSecondaryTonearm = lv_obj_create(ui_MusicSecondaryAlbumCard);
    lv_obj_set_size(ui_MusicSecondaryTonearm, 28, 4);
    lv_obj_align(ui_MusicSecondaryTonearm, LV_ALIGN_TOP_RIGHT, -7, 15);
    lv_obj_clear_flag(ui_MusicSecondaryTonearm, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_MusicSecondaryTonearm, 2, 0);
    lv_obj_set_style_bg_color(ui_MusicSecondaryTonearm, lv_color_hex(0xAAB4C0), 0);
    lv_obj_set_style_border_width(ui_MusicSecondaryTonearm, 0, 0);
    lv_obj_set_style_transform_angle(ui_MusicSecondaryTonearm, 330, 0);
    lv_obj_set_style_transform_pivot_x(ui_MusicSecondaryTonearm, 0, 0);
    lv_obj_set_style_transform_pivot_y(ui_MusicSecondaryTonearm, 2, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryTonearm, 0, 0);

    ui_MusicSecondaryTonearmHead = lv_obj_create(ui_MusicSecondaryTonearm);
    lv_obj_set_size(ui_MusicSecondaryTonearmHead, 8, 8);
    lv_obj_align(ui_MusicSecondaryTonearmHead, LV_ALIGN_RIGHT_MID, 1, 0);
    lv_obj_clear_flag(ui_MusicSecondaryTonearmHead, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_MusicSecondaryTonearmHead, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui_MusicSecondaryTonearmHead, lv_color_hex(0xE6ECF2), 0);
    lv_obj_set_style_border_width(ui_MusicSecondaryTonearmHead, 0, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryTonearmHead, 0, 0);

    ui_MusicSecondaryTonearmPivot = lv_obj_create(ui_MusicSecondaryAlbumCard);
    lv_obj_set_size(ui_MusicSecondaryTonearmPivot, 10, 10);
    lv_obj_align(ui_MusicSecondaryTonearmPivot, LV_ALIGN_TOP_RIGHT, -5, 12);
    lv_obj_clear_flag(ui_MusicSecondaryTonearmPivot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_MusicSecondaryTonearmPivot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ui_MusicSecondaryTonearmPivot, lv_color_hex(0x7A8595), 0);
    lv_obj_set_style_border_width(ui_MusicSecondaryTonearmPivot, 0, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryTonearmPivot, 0, 0);

    ui_MusicSecondaryTitle = lv_label_create(ui_MusicSecondaryMetaCard);
    lv_obj_set_width(ui_MusicSecondaryTitle, 282);
    lv_obj_align(ui_MusicSecondaryTitle, LV_ALIGN_TOP_LEFT, 12, 8);
    lv_label_set_long_mode(ui_MusicSecondaryTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(ui_MusicSecondaryTitle, &ui_font_FontCKJGT24, 0);
    lv_obj_set_style_text_color(ui_MusicSecondaryTitle, lv_color_hex(0xF5F7FA), 0);

    ui_MusicSecondarySubtitle = lv_label_create(ui_MusicSecondaryMetaCard);
    lv_obj_set_width(ui_MusicSecondarySubtitle, 282);
    lv_obj_align(ui_MusicSecondarySubtitle, LV_ALIGN_TOP_LEFT, 12, 34);
    lv_label_set_long_mode(ui_MusicSecondarySubtitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(ui_MusicSecondarySubtitle, &ui_font_FontCKJGT16, 0);
    lv_obj_set_style_text_color(ui_MusicSecondarySubtitle, lv_color_hex(0x8FA0B5), 0);

    ui_MusicSecondaryProgress = lv_bar_create(ui_MusicSecondaryMetaCard);
    lv_obj_set_size(ui_MusicSecondaryProgress, 280, 8);
    lv_obj_align(ui_MusicSecondaryProgress, LV_ALIGN_TOP_LEFT, 12, 58);
    lv_bar_set_range(ui_MusicSecondaryProgress, 0, 1000);
    lv_obj_set_style_radius(ui_MusicSecondaryProgress, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_MusicSecondaryProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_MusicSecondaryProgress, lv_color_hex(0x283241), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_MusicSecondaryProgress, lv_color_hex(0xD33A31), LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_MusicSecondaryCurrentTime = lv_label_create(ui_MusicSecondaryMetaCard);
    lv_obj_set_width(ui_MusicSecondaryCurrentTime, 60);
    lv_obj_align(ui_MusicSecondaryCurrentTime, LV_ALIGN_TOP_LEFT, 12, 70);
    lv_obj_set_style_text_align(ui_MusicSecondaryCurrentTime, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(ui_MusicSecondaryCurrentTime, &ui_font_BebasNeueFont14, 0);
    lv_obj_set_style_text_color(ui_MusicSecondaryCurrentTime, lv_color_hex(0x91A0B2), 0);

    ui_MusicSecondaryTotalTime = lv_label_create(ui_MusicSecondaryMetaCard);
    lv_obj_set_width(ui_MusicSecondaryTotalTime, 60);
    lv_obj_align(ui_MusicSecondaryTotalTime, LV_ALIGN_TOP_RIGHT, -12, 70);
    lv_obj_set_style_text_align(ui_MusicSecondaryTotalTime, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(ui_MusicSecondaryTotalTime, &ui_font_BebasNeueFont14, 0);
    lv_obj_set_style_text_color(ui_MusicSecondaryTotalTime, lv_color_hex(0x91A0B2), 0);

    ui_MusicSecondaryHintLeft = lv_label_create(ui_MusicScreenSecondary);
    lv_obj_set_width(ui_MusicSecondaryHintLeft, 120);
    lv_obj_align(ui_MusicSecondaryHintLeft, LV_ALIGN_BOTTOM_LEFT, 10, -7);
    lv_obj_set_style_text_align(ui_MusicSecondaryHintLeft, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_MusicSecondaryHintLeft, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_MusicSecondaryHintLeft, lv_color_hex(0xBAC5D3), 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryHintLeft, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryHintLeft, 0, 0);

    ui_MusicSecondaryHintCenter = lv_label_create(ui_MusicScreenSecondary);
    lv_obj_set_width(ui_MusicSecondaryHintCenter, 160);
    lv_obj_align(ui_MusicSecondaryHintCenter, LV_ALIGN_BOTTOM_MID, 0, -7);
    lv_obj_set_style_text_align(ui_MusicSecondaryHintCenter, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_MusicSecondaryHintCenter, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_MusicSecondaryHintCenter, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryHintCenter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryHintCenter, 0, 0);

    ui_MusicSecondaryHintRight = lv_label_create(ui_MusicScreenSecondary);
    lv_obj_set_width(ui_MusicSecondaryHintRight, 120);
    lv_obj_align(ui_MusicSecondaryHintRight, LV_ALIGN_BOTTOM_RIGHT, -10, -7);
    lv_obj_set_style_text_align(ui_MusicSecondaryHintRight, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_MusicSecondaryHintRight, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui_MusicSecondaryHintRight, lv_color_hex(0xBAC5D3), 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryHintRight, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(ui_MusicSecondaryHintRight, 0, 0);

    ui_MusicSecondaryButtonLeft = lv_btn_create(ui_MusicScreenSecondary);
    lv_obj_set_size(ui_MusicSecondaryButtonLeft, 142, 32);
    lv_obj_align(ui_MusicSecondaryButtonLeft, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryButtonLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_MusicSecondaryButtonLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_MusicSecondaryButtonLeft, LV_OBJ_FLAG_SCROLLABLE);

    ui_MusicSecondaryButtonEnter = lv_btn_create(ui_MusicScreenSecondary);
    lv_obj_set_size(ui_MusicSecondaryButtonEnter, 144, 32);
    lv_obj_align(ui_MusicSecondaryButtonEnter, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryButtonEnter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_MusicSecondaryButtonEnter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_MusicSecondaryButtonEnter, LV_OBJ_FLAG_SCROLLABLE);

    ui_MusicSecondaryButtonRight = lv_btn_create(ui_MusicScreenSecondary);
    lv_obj_set_size(ui_MusicSecondaryButtonRight, 142, 32);
    lv_obj_align(ui_MusicSecondaryButtonRight, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryButtonRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_MusicSecondaryButtonRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_MusicSecondaryButtonRight, LV_OBJ_FLAG_SCROLLABLE);

    ui_MusicSecondaryButtonExit = lv_btn_create(ui_MusicScreenSecondary);
    lv_obj_set_size(ui_MusicSecondaryButtonExit, 48, 24);
    lv_obj_align(ui_MusicSecondaryButtonExit, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ui_MusicSecondaryButtonExit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_MusicSecondaryButtonExit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_MusicSecondaryButtonExit, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_MusicSecondaryButtonLeft, ui_event_ButtonLeftMusicSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_MusicSecondaryButtonEnter, ui_event_ButtonEnterMusicSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_MusicSecondaryButtonRight, ui_event_ButtonRightMusicSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_MusicSecondaryButtonExit, ui_event_ButtonExitMusicSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_MusicScreenSecondary, ui_event_MusicScreenSecondary, LV_EVENT_ALL, NULL);

    ui_MusicSecondaryRecordTimer = lv_timer_create(music_secondary_record_timer_cb, 60, NULL);
    music_secondary_set_record_angle(0);

    music_secondary_refresh_player_ui();
}

void ui_MusicScreenSecondary_screen_destroy(void)
{
    ui_MusicScreen_set_visual_host(NULL);
    if (ui_MusicSecondaryRecordTimer) {
        lv_timer_del(ui_MusicSecondaryRecordTimer);
    }
    if (ui_MusicScreenSecondary) {
        lv_obj_del(ui_MusicScreenSecondary);
    }

    ui_MusicScreenSecondary = NULL;
    ui_MusicSecondaryButtonLeft = NULL;
    ui_MusicSecondaryButtonRight = NULL;
    ui_MusicSecondaryButtonEnter = NULL;
    ui_MusicSecondaryButtonExit = NULL;
    ui_MusicSecondaryVisualHost = NULL;
    ui_MusicSecondaryShell = NULL;
    ui_MusicSecondaryAlbumCard = NULL;
    ui_MusicSecondaryMetaCard = NULL;
    ui_MusicSecondaryRecord = NULL;
    ui_MusicSecondaryRecordLabel = NULL;
    ui_MusicSecondaryRecordHalo = NULL;
    ui_MusicSecondaryRecordGlint = NULL;
    ui_MusicSecondaryTonearm = NULL;
    ui_MusicSecondaryTonearmHead = NULL;
    ui_MusicSecondaryTonearmPivot = NULL;
    ui_MusicSecondaryTitle = NULL;
    ui_MusicSecondarySubtitle = NULL;
    ui_MusicSecondaryProgress = NULL;
    ui_MusicSecondaryCurrentTime = NULL;
    ui_MusicSecondaryTotalTime = NULL;
    ui_MusicSecondaryHintLeft = NULL;
    ui_MusicSecondaryHintCenter = NULL;
    ui_MusicSecondaryHintRight = NULL;
    ui_MusicSecondaryRecordTimer = NULL;
    s_music_record_angle = 0;
}
