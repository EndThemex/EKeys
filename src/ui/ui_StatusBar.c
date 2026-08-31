#include "ui.h"
#include "ui_StatusBar.h"


// 状态栏对象
static lv_obj_t *status_bar = NULL;

// 状态栏组件
static lv_obj_t *workmode_icon = NULL;
static lv_obj_t *recording_dot = NULL;
static lv_obj_t *volume_icon = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *battery_icon = NULL;
static lv_obj_t *moda_status_icon = NULL;
static lv_timer_t *recording_blink_timer = NULL;
static bool recording_blink_visible = false;

static void recording_blink_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (recording_dot == NULL) {
        return;
    }

    recording_blink_visible = !recording_blink_visible;
    lv_obj_set_style_bg_opa(recording_dot, recording_blink_visible ? LV_OPA_COVER : LV_OPA_30, 0);
}
static lv_obj_t *modb_status_icon = NULL;

void ui_StatusBar_init(void)
{
    // 只在主屏幕存在时才创建状态栏
    if (ui_MainScreen == NULL) {
        return;
    }

    // 定义不同大小的符号字体样式
    static lv_style_t icon_small_style;
    lv_style_init(&icon_small_style);
    lv_style_set_text_font(&icon_small_style, &lv_font_montserrat_18);

    static lv_style_t icon_medium_style;
    lv_style_init(&icon_medium_style);
    lv_style_set_text_font(&icon_medium_style, &lv_font_montserrat_24);

    // 在顶层创建状态栏
    ///status_bar = lv_obj_create(lv_layer_top());
    status_bar = lv_obj_create(ui_MainScreen);
    lv_obj_remove_style_all(status_bar); // 移除默认样式
    lv_obj_set_size(status_bar, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(status_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 设置半透明背景
    static lv_style_t style;
    lv_style_init(&style);
    //lv_style_set_bg_opa(&style, LV_OPA_50);
    lv_style_set_bg_color(&style, lv_color_hex(0x000000));
    lv_style_set_pad_all(&style, 5);
    lv_style_set_text_color(&style, lv_color_white());
    lv_obj_add_style(status_bar, &style, 0);


    // 添加图标
    recording_dot = lv_obj_create(status_bar);
    lv_obj_remove_style_all(recording_dot);
    lv_obj_set_size(recording_dot, 20, 20);
    lv_obj_align(recording_dot, LV_ALIGN_TOP_LEFT, 278, 10);
    lv_obj_set_style_radius(recording_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(recording_dot, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_bg_opa(recording_dot, LV_OPA_COVER, 0);
    lv_obj_add_flag(recording_dot, LV_OBJ_FLAG_HIDDEN);

    workmode_icon = lv_label_create(status_bar);
    lv_label_set_text(workmode_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_add_style(workmode_icon, &icon_medium_style, 0);  // 小图标
    lv_obj_align(workmode_icon, LV_ALIGN_TOP_LEFT, 3, 15);
    lv_obj_set_style_text_color(workmode_icon, lv_color_hex(0x808080), 0);

    wifi_icon = lv_label_create(status_bar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WARNING);
    lv_obj_add_style(wifi_icon, &icon_medium_style, 0);  // 小图标
    lv_obj_align(wifi_icon, LV_ALIGN_TOP_LEFT, 313, 10);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x808080), 0);

    volume_icon = lv_label_create(status_bar);
    lv_label_set_text(volume_icon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_add_style(volume_icon, &icon_medium_style, 0);  // 小图标
    lv_obj_align(volume_icon, LV_ALIGN_TOP_LEFT, 348, 10);
    lv_obj_set_style_text_color(volume_icon, lv_color_hex(0x808080), 0);

    battery_icon = lv_label_create(status_bar);
    lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_align(battery_icon, LV_ALIGN_TOP_LEFT, 383, 10);
    lv_obj_add_style(battery_icon, &icon_medium_style, 0);  // 小图标
    lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x808080), 0);

    moda_status_icon = lv_label_create(status_bar);
    lv_label_set_text(moda_status_icon, LV_SYMBOL_OK);
    lv_obj_align(moda_status_icon, LV_ALIGN_TOP_LEFT, 328, 88);
    lv_obj_add_style(moda_status_icon, &icon_medium_style, 0);  // 小图标
    lv_obj_set_style_text_color(moda_status_icon, lv_color_hex(0x5CEA4C), 0);

    modb_status_icon = lv_label_create(status_bar);
    lv_label_set_text(modb_status_icon, LV_SYMBOL_CLOSE);
    lv_obj_align(modb_status_icon, LV_ALIGN_TOP_LEFT, 382, 88);
    lv_obj_add_style(modb_status_icon, &icon_medium_style, 0);  // 小图标
    lv_obj_set_style_text_color(modb_status_icon, lv_color_hex(0x901B27), 0);    
}

// 在status_bar.c中实现
void ui_StatusBar_show(bool show)
{
    if(status_bar == NULL) return;

    if(show) {
        lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(status_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

// 更新WiFi信号强度
void status_bar_set_wifi_strength(int strength)
{
    if((strength >= (-60)) && (strength < (-30))) {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    } else if((strength >= (-80)) && (strength < (-60))) {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    } else if((strength >= (-100)) && (strength < (-80))) {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    } else {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WARNING);
    }
}

// 更新电池电量
void status_bar_set_battery_level(uint8_t level)
{
    if(level > 80) {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
    } else if(level > 50) {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_3);
    } else if(level > 20) {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_2);
    } else if(level > 5) {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_1);
    } else {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_EMPTY);
    }

    // 可以添加颜色变化
    if(level < 20) {
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFF0000), 0);
    } else {
        //lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x808080), 0);
    }
}

// 更新音量状态
void status_bar_set_volume(uint8_t volume)
{
    if(volume == 0) {
        lv_label_set_text(volume_icon, LV_SYMBOL_MUTE);
    } else if(volume < 66) {
        lv_label_set_text(volume_icon, LV_SYMBOL_VOLUME_MID);
    } else {
        lv_label_set_text(volume_icon, LV_SYMBOL_VOLUME_MAX);
    }
}

// 更新录音状态
void status_bar_set_recording_state(bool is_recording)
{
    if (recording_dot == NULL) return;

    if (is_recording) {
        recording_blink_visible = true;
        lv_obj_clear_flag(recording_dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(recording_dot, LV_OPA_COVER, 0);

        if (recording_blink_timer == NULL) {
            // Blink by toggling the dot opacity.
            recording_blink_timer = lv_timer_create(recording_blink_timer_cb, 300, NULL);
        }
    } else {
        if (recording_blink_timer != NULL) {
            lv_timer_del(recording_blink_timer);
            recording_blink_timer = NULL;
        }
        recording_blink_visible = false;
        lv_obj_set_style_bg_opa(recording_dot, LV_OPA_COVER, 0);
        lv_obj_add_flag(recording_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

// 更新工作模式
void status_bar_set_working_mode(int mode)
{
    switch (mode) {
        case WIRED_KEYBOARD_MODE: {
            lv_label_set_text(workmode_icon, LV_SYMBOL_USB);
            break;
        }
        case BLUETOOTH_KEYBOARD_MODE: {
            lv_label_set_text(workmode_icon, LV_SYMBOL_BLUETOOTH);
            break;
        }
        case WIRELESS_2_4G_KEYBOARD_MODE: {
            lv_label_set_text(workmode_icon, LV_SYMBOL_DRIVE);
            break;
        }
        default:break;
    }
}

// 更新模块插入状态
void status_bar_set_module_status(int mode, bool status)
{
    switch (mode) {
        case UI_MODA: {
            if (status) {
                lv_label_set_text(moda_status_icon, LV_SYMBOL_OK);
                lv_obj_set_style_text_color(moda_status_icon, lv_color_hex(0x5CEA4C), 0);    
            } else {
                lv_label_set_text(moda_status_icon, LV_SYMBOL_CLOSE);
                lv_obj_set_style_text_color(moda_status_icon, lv_color_hex(0x901B27), 0);                    
            }
            break;
        }
        case UI_MODB: {
            if (status) {
                lv_label_set_text(modb_status_icon, LV_SYMBOL_OK);
                lv_obj_set_style_text_color(modb_status_icon, lv_color_hex(0x5CEA4C), 0);    
            } else {
                lv_label_set_text(modb_status_icon, LV_SYMBOL_CLOSE);
                lv_obj_set_style_text_color(modb_status_icon, lv_color_hex(0x901B27), 0);                    
            }            
            break;
        }
        default:break;
    }
}
