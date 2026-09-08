#include "ui.h"
#include "ui_KeyMappedSecondary.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "lvgl.h"

// 必须与 MatrixScanner.h 中 PHYSICAL_KEY_NUM（11）保持一致
#define KEYMAP_SECONDARY_KEY_NUM 11

#define KEYMAP_SECONDARY_APP_CARD_WIDTH 60
#define KEYMAP_SECONDARY_APP_CARD_HEIGHT 134
#define KEYMAP_SECONDARY_KEYS_CARD_WIDTH 358
#define KEYMAP_SECONDARY_KEYS_CARD_HEIGHT 134

lv_obj_t *ui_KeyMappedSecondary = NULL;
static lv_obj_t *ui_KeyMappedSecondaryButtonLeft = NULL;
static lv_obj_t *ui_KeyMappedSecondaryButtonRight = NULL;
static lv_obj_t *ui_KeyMappedSecondaryButtonEnter = NULL;
static lv_obj_t *ui_KeyMappedSecondaryButtonExit = NULL;
static lv_obj_t *ui_KeyMappedSecondaryShell = NULL;
static lv_obj_t *ui_KeyMappedSecondaryAppCard = NULL;
static lv_obj_t *ui_KeyMappedSecondaryKeysCard = NULL;
static lv_obj_t *ui_KeyMappedSecondaryIcon = NULL;
static lv_obj_t *ui_KeyMappedSecondaryIconImage = NULL;
static lv_obj_t *ui_KeyMappedSecondaryTitle = NULL;
static lv_obj_t *ui_KeyMappedSecondaryProfileName = NULL;
static lv_obj_t *ui_KeyMappedSecondaryFileName = NULL;
static lv_obj_t *ui_KeyMappedSecondaryKeyLabels[KEYMAP_SECONDARY_KEY_NUM] = {NULL};
static lv_obj_t *ui_KeyMappedSecondaryKeyCells[KEYMAP_SECONDARY_KEY_NUM] = {NULL}; /* A1：cell 用于 focus 高亮 */
static lv_obj_t *ui_KeyMappedSecondaryLastGroup = NULL;
static uint8_t s_keymapped_secondary_focus_slot = 0xFFu; /* A1：当前高亮槽位，0xFF = 无 */

/* 前向声明：dispatch_key 在 apply_focus 之前定义，需先声明 */
static void keymapped_secondary_apply_focus(void);
static lv_obj_t *s_keymapped_secondary_main_screen_icon = NULL;
static lv_obj_t *s_keymapped_secondary_main_screen_icon_image = NULL;
static lv_obj_t *s_keymapped_secondary_main_screen_profile_name = NULL;
static char s_keymapped_secondary_icon_src[48] = {0};
static char s_keymapped_secondary_profile_name[24] = {0};
static char s_keymapped_secondary_profile_file_name[24] = {0};
static char s_keymapped_secondary_profile_icon_symbol[8] = {0};
static char s_keymapped_secondary_key_labels[KEYMAP_SECONDARY_KEY_NUM][24] = {{0}};
static uint8_t *s_keymapped_secondary_icon_image_data = NULL;
static size_t s_keymapped_secondary_icon_image_size = 0;
static lv_img_dsc_t s_keymapped_secondary_icon_image_dsc;

typedef struct
{
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
} key_cell_rect_t;

// 11 键布局：3 行 × 4 列，首行仅 3 键；KEYS_CARD 358×134
// 列宽 82, 间距 ~6；行高 36, 间距 ~6
static const key_cell_rect_t kKeyRects[KEYMAP_SECONDARY_KEY_NUM] = {
    // 第 0 行：键 1/2/3（左对齐）
    {4, 4, 82, 36},   // 应用键 1
    {92, 4, 82, 36},  // 应用键 2
    {180, 4, 82, 36}, // 应用键 3
    // 第 1 行：键 4/5/6/7（满 4 列）
    {4, 46, 82, 36},   // 应用键 4
    {92, 46, 82, 36},  // 应用键 5
    {180, 46, 82, 36}, // 应用键 6
    {268, 46, 82, 36}, // 应用键 7
    // 第 2 行：键 8/9/10/11（满 4 列）
    {4, 88, 82, 36},   // 应用键 8
    {92, 88, 82, 36},  // 应用键 9
    {180, 88, 82, 36}, // 应用键 10
    {268, 88, 82, 36}, // 应用键 11
};

static unsigned int keymapped_secondary_extract_profile_index(const char *file_name)
{
    unsigned int profile_index = 0;
    if (file_name == NULL)
    {
        return 0;
    }

    if (sscanf(file_name, "config_profile_%u.ini", &profile_index) == 1)
    {
        return profile_index + 1;
    }

    return 0;
}

static void keymapped_secondary_style_card(lv_obj_t *obj,
                                           lv_color_t bg_color,
                                           lv_color_t border_color)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, border_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void keymapped_secondary_style_key_cell(lv_obj_t *obj)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x0F141B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x2B3442), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void keymapped_secondary_apply_cached_profile(void)
{
    char profile_hint[20] = {0};
    const unsigned int profile_number = keymapped_secondary_extract_profile_index(s_keymapped_secondary_profile_file_name);
    const char *profile_symbol = s_keymapped_secondary_profile_icon_symbol[0]
                                     ? s_keymapped_secondary_profile_icon_symbol
                                     : LV_SYMBOL_LIST;
    const char *profile_text = "Conf1";

    if (profile_number > 0)
    {
        snprintf(profile_hint, sizeof(profile_hint), "Conf%u", profile_number);
        profile_text = profile_hint;
    }

    if (ui_KeyMappedSecondaryIcon)
    {
        lv_label_set_text(ui_KeyMappedSecondaryIcon, profile_symbol);
    }
    if (s_keymapped_secondary_main_screen_icon)
    {
        lv_label_set_text(s_keymapped_secondary_main_screen_icon, profile_symbol);
    }
    if (ui_KeyMappedSecondaryProfileName)
    {
        lv_label_set_text(ui_KeyMappedSecondaryProfileName, profile_text);
    }
    if (s_keymapped_secondary_main_screen_profile_name)
    {
        lv_label_set_text(s_keymapped_secondary_main_screen_profile_name, profile_text);
    }
    if (ui_KeyMappedSecondaryFileName)
    {
        lv_label_set_text(ui_KeyMappedSecondaryFileName,
                          s_keymapped_secondary_profile_name[0] ? s_keymapped_secondary_profile_name : "当前布局");
    }
}

static void keymapped_secondary_apply_cached_key_labels(void)
{
    for (unsigned int i = 0; i < KEYMAP_SECONDARY_KEY_NUM; ++i)
    {
        if (ui_KeyMappedSecondaryKeyLabels[i])
        {
            lv_label_set_text(ui_KeyMappedSecondaryKeyLabels[i],
                              s_keymapped_secondary_key_labels[i][0] ? s_keymapped_secondary_key_labels[i] : "--");
        }
    }
}

static void keymapped_secondary_release_cached_image(void)
{
    if (s_keymapped_secondary_icon_image_data != NULL)
    {
        lv_mem_free(s_keymapped_secondary_icon_image_data);
        s_keymapped_secondary_icon_image_data = NULL;
    }
    s_keymapped_secondary_icon_image_size = 0;
    lv_memset_00(&s_keymapped_secondary_icon_image_dsc, sizeof(s_keymapped_secondary_icon_image_dsc));
}

static void keymapped_secondary_apply_cached_image(void)
{
    if (ui_KeyMappedSecondaryIcon)
    {
        lv_obj_add_flag(ui_KeyMappedSecondaryIcon, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_keymapped_secondary_main_screen_icon)
    {
        lv_obj_add_flag(s_keymapped_secondary_main_screen_icon, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui_KeyMappedSecondaryIconImage)
    {
        lv_img_cache_invalidate_src(&s_keymapped_secondary_icon_image_dsc);
        lv_img_set_src(ui_KeyMappedSecondaryIconImage, &s_keymapped_secondary_icon_image_dsc);
        lv_obj_clear_flag(ui_KeyMappedSecondaryIconImage, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_keymapped_secondary_main_screen_icon_image)
    {
        lv_img_cache_invalidate_src(&s_keymapped_secondary_icon_image_dsc);
        lv_img_set_src(s_keymapped_secondary_main_screen_icon_image, &s_keymapped_secondary_icon_image_dsc);
        lv_obj_clear_flag(s_keymapped_secondary_main_screen_icon_image, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_KeyMappedSecondary_bind_main_screen_summary(lv_obj_t *icon_label,
                                                    lv_obj_t *icon_image,
                                                    lv_obj_t *profile_name)
{
    s_keymapped_secondary_main_screen_icon = icon_label;
    s_keymapped_secondary_main_screen_icon_image = icon_image;
    s_keymapped_secondary_main_screen_profile_name = profile_name;

    keymapped_secondary_apply_cached_profile();
    if (s_keymapped_secondary_icon_image_data != NULL && s_keymapped_secondary_icon_image_size > 0)
    {
        keymapped_secondary_apply_cached_image();
    }
    else
    {
        ui_KeyMappedSecondary_set_profile_icon_source(s_keymapped_secondary_icon_src,
                                                      s_keymapped_secondary_profile_icon_symbol);
    }
}

void ui_KeyMappedSecondary_set_profile_icon_source(const char *file_path, const char *fallback_symbol)
{
    const bool has_file = file_path != NULL && file_path[0] != '\0';

    snprintf(s_keymapped_secondary_profile_icon_symbol,
             sizeof(s_keymapped_secondary_profile_icon_symbol),
             "%s",
             (fallback_symbol && fallback_symbol[0]) ? fallback_symbol : LV_SYMBOL_LIST);

    if (has_file)
    {
        snprintf(s_keymapped_secondary_icon_src, sizeof(s_keymapped_secondary_icon_src), "%s", file_path);
    }
    else
    {
        s_keymapped_secondary_icon_src[0] = '\0';
    }

    if (ui_KeyMappedSecondaryIcon)
    {
        lv_label_set_text(ui_KeyMappedSecondaryIcon,
                          s_keymapped_secondary_profile_icon_symbol);
        if (has_file)
        {
            lv_obj_add_flag(ui_KeyMappedSecondaryIcon, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(ui_KeyMappedSecondaryIcon, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_keymapped_secondary_main_screen_icon)
    {
        lv_label_set_text(s_keymapped_secondary_main_screen_icon,
                          s_keymapped_secondary_profile_icon_symbol);
        if (has_file)
        {
            lv_obj_add_flag(s_keymapped_secondary_main_screen_icon, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(s_keymapped_secondary_main_screen_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (!has_file)
    {
        lv_img_cache_invalidate_src(s_keymapped_secondary_icon_src);
        if (ui_KeyMappedSecondaryIconImage)
        {
            lv_img_set_src(ui_KeyMappedSecondaryIconImage, NULL);
            lv_obj_add_flag(ui_KeyMappedSecondaryIconImage, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_keymapped_secondary_main_screen_icon_image)
        {
            lv_img_set_src(s_keymapped_secondary_main_screen_icon_image, NULL);
            lv_obj_add_flag(s_keymapped_secondary_main_screen_icon_image, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    lv_img_cache_invalidate_src(s_keymapped_secondary_icon_src);
    if (ui_KeyMappedSecondaryIconImage)
    {
        lv_img_set_src(ui_KeyMappedSecondaryIconImage, s_keymapped_secondary_icon_src);
        lv_obj_clear_flag(ui_KeyMappedSecondaryIconImage, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_keymapped_secondary_main_screen_icon_image)
    {
        lv_img_set_src(s_keymapped_secondary_main_screen_icon_image, s_keymapped_secondary_icon_src);
        lv_obj_clear_flag(s_keymapped_secondary_main_screen_icon_image, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_KeyMappedSecondary_set_profile_icon_image_data(const uint8_t *image_data,
                                                       size_t image_size,
                                                       uint16_t width,
                                                       uint16_t height,
                                                       const char *fallback_symbol)
{
    ui_KeyMappedSecondary_set_profile_icon_source(NULL, fallback_symbol);
    keymapped_secondary_release_cached_image();

    if (image_data == NULL || image_size == 0 || width == 0 || height == 0)
    {
        return;
    }

    s_keymapped_secondary_icon_image_data = lv_mem_alloc(image_size);
    if (s_keymapped_secondary_icon_image_data == NULL)
    {
        return;
    }

    memcpy(s_keymapped_secondary_icon_image_data, image_data, image_size);
    s_keymapped_secondary_icon_image_size = image_size;
    lv_memset_00(&s_keymapped_secondary_icon_image_dsc, sizeof(s_keymapped_secondary_icon_image_dsc));
    s_keymapped_secondary_icon_image_dsc.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    s_keymapped_secondary_icon_image_dsc.header.w = width;
    s_keymapped_secondary_icon_image_dsc.header.h = height;
    s_keymapped_secondary_icon_image_dsc.data = s_keymapped_secondary_icon_image_data;
    s_keymapped_secondary_icon_image_dsc.data_size = (uint32_t)s_keymapped_secondary_icon_image_size;
    keymapped_secondary_apply_cached_image();
}

static void keymapped_secondary_forward_key(uint32_t key)
{
    lv_obj_t *active_screen = lv_scr_act();
    lv_group_t *g = lv_group_get_default();
    lv_obj_t *target = g ? lv_group_get_focused(g) : active_screen;
    if (target)
    {
        lv_event_send(target, LV_EVENT_KEY, (void *)key);
    }
}

/*
 * 二级页 key 分派（不在 group 模型下，screen 回调就是唯一的派发点）。
 * 入口：
 *   - DisplayTask 经 ActionInput 转 LV_EVENT_KEY（旋钮 / 应用键 1~11）
 *   - SquareLine ButtonLeft/Right/Enter/Exit 转发 LV_KEY_LEFT/RIGHT/ENTER/ESC
 * 行为：
 *   LV_KEY_LEFT/RIGHT → focus_slot 移动 1（3 列布局，超出范围按列换行）
 *   LV_KEY_ENTER       → 当前槽位标红（占位：未来接"编辑该键"流程）
 *   LV_KEY_ESC         → 回 KEYMAPPED
 *   1 ~ 9              → 立即把焦点跳到对应槽位（key_id - 1）
 *   10 / 11            → 与 1~9 等同（占 9 槽 11 键的折中，11 取模映射）
 */
static void keymapped_secondary_dispatch_key(uintptr_t key)
{
    if (key == (uintptr_t)LV_KEY_ESC)
    {
        ui_set_active_screen_tag(UI_SCREEN_KEYMAPPED);
        _ui_screen_change(&ui_KeyMapped, LV_SCR_LOAD_ANIM_NONE, 0, 0,
                          &ui_KeyMapped_screen_init);
        lv_refr_now(NULL);
        return;
    }

    if (key >= 1 && key <= 11)
    {
        ui_KeyMappedSecondary_set_focus((unsigned int)key);
        return;
    }

    if (s_keymapped_secondary_focus_slot >= KEYMAP_SECONDARY_KEY_NUM)
    {
        s_keymapped_secondary_focus_slot = 0;
    }
    if (key == (uintptr_t)LV_KEY_RIGHT)
    {
        const uint8_t cur = s_keymapped_secondary_focus_slot;
        const uint8_t col = cur % 4;
        if (cur + 1 < KEYMAP_SECONDARY_KEY_NUM)
        {
            s_keymapped_secondary_focus_slot = (uint8_t)(cur + 1);
        }
    }
    else if (key == (uintptr_t)LV_KEY_LEFT)
    {
        const uint8_t cur = s_keymapped_secondary_focus_slot;
        if (cur > 0)
        {
            s_keymapped_secondary_focus_slot = (uint8_t)(cur - 1);
        }
    }
    else if (key == (uintptr_t)LV_KEY_ENTER)
    {
        /* 占位：未来 KEYMAPPED 二级页要支持"重新映射该键"时再接 editing 流程 */
    }
    keymapped_secondary_apply_focus();
}

void ui_event_KeyMappedSecondaryScreen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code != LV_EVENT_KEY)
    {
        return;
    }

    const uintptr_t key = (uintptr_t)lv_event_get_param(e);
    keymapped_secondary_dispatch_key(key);
}

void ui_event_ButtonLeftKeyMappedSecondary(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        keymapped_secondary_forward_key(LV_KEY_LEFT);
    }
}

void ui_event_ButtonRightKeyMappedSecondary(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        keymapped_secondary_forward_key(LV_KEY_RIGHT);
    }
}

void ui_event_ButtonEnterKeyMappedSecondary(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        keymapped_secondary_forward_key(LV_KEY_ENTER);
    }
}

void ui_event_ButtonExitKeyMappedSecondary(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        keymapped_secondary_forward_key(LV_KEY_ESC);
    }
}

void ui_KeyMappedSecondary_set_profile(const char *icon, const char *name, const char *file_name)
{
    snprintf(s_keymapped_secondary_profile_icon_symbol,
             sizeof(s_keymapped_secondary_profile_icon_symbol),
             "%s",
             (icon && icon[0]) ? icon : LV_SYMBOL_LIST);
    snprintf(s_keymapped_secondary_profile_name,
             sizeof(s_keymapped_secondary_profile_name),
             "%s",
             (name && name[0]) ? name : "当前布局");
    snprintf(s_keymapped_secondary_profile_file_name,
             sizeof(s_keymapped_secondary_profile_file_name),
             "%s",
             (file_name && file_name[0]) ? file_name : "config_profile_0.ini");

    keymapped_secondary_apply_cached_profile();
}

void ui_KeyMappedSecondary_set_key_label(unsigned int key_index, const char *text)
{
    if (key_index >= 16)
    {
        return;
    }

    snprintf(s_keymapped_secondary_key_labels[key_index],
             sizeof(s_keymapped_secondary_key_labels[key_index]),
             "%s",
             (text && text[0]) ? text : "--");

    if (!ui_KeyMappedSecondaryKeyLabels[key_index])
    {
        return;
    }
    lv_label_set_text(ui_KeyMappedSecondaryKeyLabels[key_index], s_keymapped_secondary_key_labels[key_index]);
}

/*
 * A1 修复：把当前高亮槽位的边框刷成主题红，其余还原。
 * 活动 cell 即 ui_KeyMappedSecondaryKeyCells[slot]。
 */
static void keymapped_secondary_apply_focus(void)
{
    const lv_color_t focus_color = lv_color_hex(0xD33A31);
    const lv_color_t default_color = lv_color_hex(0x2B3442);

    for (unsigned int i = 0; i < KEYMAP_SECONDARY_KEY_NUM; ++i)
    {
        lv_obj_t *cell = ui_KeyMappedSecondaryKeyCells[i];
        if (cell == NULL)
        {
            continue;
        }
        if (i == s_keymapped_secondary_focus_slot)
        {
            lv_obj_set_style_border_color(cell, focus_color, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(cell, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        else
        {
            lv_obj_set_style_border_color(cell, default_color, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void ui_KeyMappedSecondary_set_focus(unsigned int key_id)
{
    /*
     * 应用键 1~9 → 直接映射 9 个槽位（slot = key_id - 1）。
     * 10/11 在二级页没有对应槽位时清空高亮，但屏幕仍然跳转。
     */
    if (key_id >= 1 && key_id <= KEYMAP_SECONDARY_KEY_NUM)
    {
        s_keymapped_secondary_focus_slot = (uint8_t)(key_id - 1);
    }
    else
    {
        s_keymapped_secondary_focus_slot = 0xFFu;
    }
    keymapped_secondary_apply_focus();
}

void ui_KeyMappedSecondary_screen_init(void)
{
    ui_KeyMappedSecondary = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_KeyMappedSecondary, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_KeyMappedSecondary, lv_color_hex(0x070A0F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_KeyMappedSecondary, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_KeyMappedSecondary, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_KeyMappedSecondary, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_set_active_screen_tag(UI_SCREEN_KEYMAPPED_SECONDARY);

    ui_KeyMappedSecondaryShell = lv_obj_create(ui_KeyMappedSecondary);
    lv_obj_set_size(ui_KeyMappedSecondaryShell, 428, 142);
    lv_obj_align(ui_KeyMappedSecondaryShell, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(ui_KeyMappedSecondaryShell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_KeyMappedSecondaryShell, lv_color_hex(0x0C0F14), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_KeyMappedSecondaryShell, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_KeyMappedSecondaryShell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_KeyMappedSecondaryShell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_KeyMappedSecondaryShell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_KeyMappedSecondaryAppCard = lv_obj_create(ui_KeyMappedSecondaryShell);
    lv_obj_set_size(ui_KeyMappedSecondaryAppCard, KEYMAP_SECONDARY_APP_CARD_WIDTH, KEYMAP_SECONDARY_APP_CARD_HEIGHT);
    lv_obj_align(ui_KeyMappedSecondaryAppCard, LV_ALIGN_TOP_LEFT, 4, 4);
    keymapped_secondary_style_card(ui_KeyMappedSecondaryAppCard, lv_color_hex(0x161C25), lv_color_hex(0x2E3947));

    ui_KeyMappedSecondaryKeysCard = lv_obj_create(ui_KeyMappedSecondaryShell);
    lv_obj_set_size(ui_KeyMappedSecondaryKeysCard, KEYMAP_SECONDARY_KEYS_CARD_WIDTH, KEYMAP_SECONDARY_KEYS_CARD_HEIGHT);
    lv_obj_align(ui_KeyMappedSecondaryKeysCard, LV_ALIGN_TOP_RIGHT, -4, 4);
    keymapped_secondary_style_card(ui_KeyMappedSecondaryKeysCard, lv_color_hex(0x121820), lv_color_hex(0x273242));

    ui_KeyMappedSecondaryTitle = lv_label_create(ui_KeyMappedSecondaryAppCard);
    lv_label_set_text(ui_KeyMappedSecondaryTitle, "APP");
    lv_obj_set_width(ui_KeyMappedSecondaryTitle, KEYMAP_SECONDARY_APP_CARD_WIDTH - 12);
    lv_label_set_long_mode(ui_KeyMappedSecondaryTitle, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(ui_KeyMappedSecondaryTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_KeyMappedSecondaryTitle, lv_color_hex(0x8FA0B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_KeyMappedSecondaryTitle, &ui_font_BebasNeueFont14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(ui_KeyMappedSecondaryTitle, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *iconBadge = lv_obj_create(ui_KeyMappedSecondaryAppCard);
    lv_obj_set_size(iconBadge, 50, 50);
    lv_obj_align(iconBadge, LV_ALIGN_CENTER, 0, -4);
    lv_obj_clear_flag(iconBadge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(iconBadge, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(iconBadge, lv_color_hex(0x0D1015), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(iconBadge, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(iconBadge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(iconBadge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_KeyMappedSecondaryIcon = lv_label_create(iconBadge);
    lv_label_set_text(ui_KeyMappedSecondaryIcon, LV_SYMBOL_LIST);
    lv_obj_center(ui_KeyMappedSecondaryIcon);
    lv_obj_set_style_text_font(ui_KeyMappedSecondaryIcon, &ui_font_BebasNeueFont36, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_KeyMappedSecondaryIcon, lv_color_hex(0xF5F7FA), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_KeyMappedSecondaryIconImage = lv_img_create(iconBadge);
    lv_obj_center(ui_KeyMappedSecondaryIconImage);
    lv_obj_add_flag(ui_KeyMappedSecondaryIconImage, LV_OBJ_FLAG_HIDDEN);

    ui_KeyMappedSecondaryProfileName = lv_label_create(ui_KeyMappedSecondaryAppCard);
    lv_label_set_text(ui_KeyMappedSecondaryProfileName, "Conf1");
    lv_obj_set_width(ui_KeyMappedSecondaryProfileName, KEYMAP_SECONDARY_APP_CARD_WIDTH - 10);
    lv_label_set_long_mode(ui_KeyMappedSecondaryProfileName, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(ui_KeyMappedSecondaryProfileName, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_KeyMappedSecondaryProfileName, lv_color_hex(0xD33A31), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_KeyMappedSecondaryProfileName, &ui_font_FontCKJGT16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(ui_KeyMappedSecondaryProfileName, LV_ALIGN_BOTTOM_MID, 0, -12);

    for (unsigned int i = 0; i < KEYMAP_SECONDARY_KEY_NUM; ++i)
    {
        lv_obj_t *cell = lv_obj_create(ui_KeyMappedSecondaryKeysCard);
        lv_obj_set_size(cell, kKeyRects[i].w, kKeyRects[i].h);
        lv_obj_set_pos(cell, kKeyRects[i].x, kKeyRects[i].y);
        keymapped_secondary_style_key_cell(cell);
        ui_KeyMappedSecondaryKeyCells[i] = cell; /* A1：记录 cell 以做 focus 高亮 */

        ui_KeyMappedSecondaryKeyLabels[i] = lv_label_create(cell);
        lv_label_set_text(ui_KeyMappedSecondaryKeyLabels[i], "--");
        lv_obj_set_width(ui_KeyMappedSecondaryKeyLabels[i], kKeyRects[i].w - 8);
        lv_label_set_long_mode(ui_KeyMappedSecondaryKeyLabels[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(ui_KeyMappedSecondaryKeyLabels[i], &ui_font_BebasNeueFont14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_KeyMappedSecondaryKeyLabels[i], lv_color_hex(0xF5F7FA), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_KeyMappedSecondaryKeyLabels[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(ui_KeyMappedSecondaryKeyLabels[i]);
    }

    ui_KeyMappedSecondaryButtonLeft = lv_btn_create(ui_KeyMappedSecondary);
    lv_obj_set_size(ui_KeyMappedSecondaryButtonLeft, 142, 32);
    lv_obj_align(ui_KeyMappedSecondaryButtonLeft, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ui_KeyMappedSecondaryButtonLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_KeyMappedSecondaryButtonLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_KeyMappedSecondaryButtonLeft, LV_OBJ_FLAG_SCROLLABLE);

    ui_KeyMappedSecondaryButtonEnter = lv_btn_create(ui_KeyMappedSecondary);
    lv_obj_set_size(ui_KeyMappedSecondaryButtonEnter, 144, 32);
    lv_obj_align(ui_KeyMappedSecondaryButtonEnter, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(ui_KeyMappedSecondaryButtonEnter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_KeyMappedSecondaryButtonEnter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_KeyMappedSecondaryButtonEnter, LV_OBJ_FLAG_SCROLLABLE);

    ui_KeyMappedSecondaryButtonRight = lv_btn_create(ui_KeyMappedSecondary);
    lv_obj_set_size(ui_KeyMappedSecondaryButtonRight, 142, 32);
    lv_obj_align(ui_KeyMappedSecondaryButtonRight, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(ui_KeyMappedSecondaryButtonRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_KeyMappedSecondaryButtonRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_KeyMappedSecondaryButtonRight, LV_OBJ_FLAG_SCROLLABLE);

    ui_KeyMappedSecondaryButtonExit = lv_btn_create(ui_KeyMappedSecondary);
    lv_obj_set_size(ui_KeyMappedSecondaryButtonExit, 48, 24);
    lv_obj_align(ui_KeyMappedSecondaryButtonExit, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ui_KeyMappedSecondaryButtonExit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_KeyMappedSecondaryButtonExit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_KeyMappedSecondaryButtonExit, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_KeyMappedSecondaryButtonLeft, ui_event_ButtonLeftKeyMappedSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_KeyMappedSecondaryButtonEnter, ui_event_ButtonEnterKeyMappedSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_KeyMappedSecondaryButtonRight, ui_event_ButtonRightKeyMappedSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_KeyMappedSecondaryButtonExit, ui_event_ButtonExitKeyMappedSecondary, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_KeyMappedSecondary, ui_event_KeyMappedSecondaryScreen, LV_EVENT_ALL, NULL);

    keymapped_secondary_apply_cached_profile();
    if (s_keymapped_secondary_icon_image_data != NULL && s_keymapped_secondary_icon_image_size > 0)
    {
        keymapped_secondary_apply_cached_image();
    }
    else
    {
        ui_KeyMappedSecondary_set_profile_icon_source(s_keymapped_secondary_icon_src,
                                                      s_keymapped_secondary_profile_icon_symbol);
    }
    keymapped_secondary_apply_cached_key_labels();
}

void ui_KeyMappedSecondary_screen_destroy(void)
{
    if (ui_KeyMappedSecondary)
    {
        lv_obj_del(ui_KeyMappedSecondary);
    }
    ui_KeyMappedSecondary = NULL;
    ui_KeyMappedSecondaryButtonLeft = NULL;
    ui_KeyMappedSecondaryButtonRight = NULL;
    ui_KeyMappedSecondaryButtonEnter = NULL;
    ui_KeyMappedSecondaryButtonExit = NULL;
    ui_KeyMappedSecondaryShell = NULL;
    ui_KeyMappedSecondaryAppCard = NULL;
    ui_KeyMappedSecondaryKeysCard = NULL;
    ui_KeyMappedSecondaryIcon = NULL;
    ui_KeyMappedSecondaryIconImage = NULL;
    ui_KeyMappedSecondaryTitle = NULL;
    ui_KeyMappedSecondaryProfileName = NULL;
    ui_KeyMappedSecondaryFileName = NULL;
    ui_KeyMappedSecondaryLastGroup = NULL;
    for (unsigned int i = 0; i < KEYMAP_SECONDARY_KEY_NUM; ++i)
    {
        ui_KeyMappedSecondaryKeyLabels[i] = NULL;
    }
}