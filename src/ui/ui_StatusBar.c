#include "ui.h"
#include "ui_StatusBar.h"

// 状态栏对象
static lv_obj_t *status_bar = NULL;

// 状态栏组件（全部位于屏幕底部一行，从左到右：工作模式 → 录音点 → WiFi → 音量 → 电量）
static lv_obj_t *workmode_icon = NULL;
static lv_obj_t *recording_dot = NULL;
static lv_obj_t *volume_icon = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *battery_icon = NULL;
static lv_timer_t *recording_blink_timer = NULL;
static bool recording_blink_visible = false;

/*
 * 底部一行图标布局约定：
 *   - 以第一个图标（工作模式）为唯一基准，固定绝对坐标；
 *   - 其余图标一律 lv_obj_align_to 排到前一个图标的右侧居中
 *     （OUT_RIGHT_MID），x 由前一图标宽度 + 间隔相对计算，
 *     y 由 MID 对齐自动保持一致；
 *   - 图标统一 montserrat_24 字号，视觉大小一致。
 * 注意 status_bar 有 pad_all(5)，基准坐标按内容区写，实际渲染 = 设定值 + 5。
 */
#define STATUS_BAR_FIRST_ICON_X 8
#define STATUS_BAR_FIRST_ICON_Y 101
#define STATUS_BAR_ICON_GAP 6

static void recording_blink_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (recording_dot == NULL)
    {
        return;
    }

    recording_blink_visible = !recording_blink_visible;
    lv_obj_set_style_bg_opa(recording_dot, recording_blink_visible ? LV_OPA_COVER : LV_OPA_30, 0);
}

void ui_StatusBar_init(void)
{
    // 只在主屏幕存在时才创建状态栏
    if (ui_MainScreen == NULL)
    {
        return;
    }

    // 图标统一字号样式
    static lv_style_t icon_style;
    lv_style_init(&icon_style);
    lv_style_set_text_font(&icon_style, &lv_font_montserrat_24);

    // 在顶层创建状态栏
    /// status_bar = lv_obj_create(lv_layer_top());
    status_bar = lv_obj_create(ui_MainScreen);
    lv_obj_remove_style_all(status_bar); // 移除默认样式
    lv_obj_set_size(status_bar, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(status_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // 设置半透明背景
    static lv_style_t style;
    lv_style_init(&style);
    // lv_style_set_bg_opa(&style, LV_OPA_50);
    lv_style_set_bg_color(&style, lv_color_hex(0x000000));
    lv_style_set_pad_all(&style, 5);
    lv_style_set_text_color(&style, lv_color_white());
    lv_obj_add_style(status_bar, &style, 0);

    // 基准图标：工作模式（有线 USB / 蓝牙 / 2.4G），其余图标相对它排布
    workmode_icon = lv_label_create(status_bar);
    lv_label_set_text(workmode_icon, LV_SYMBOL_USB);
    lv_obj_add_style(workmode_icon, &icon_style, 0);
    lv_obj_set_style_text_color(workmode_icon, lv_color_hex(0x808080), 0);
    lv_obj_align(workmode_icon, LV_ALIGN_TOP_LEFT, STATUS_BAR_FIRST_ICON_X, STATUS_BAR_FIRST_ICON_Y);

    // 录音点（12x12 圆点，隐藏，录音时闪烁；垂直居中对齐基准图标）
    recording_dot = lv_obj_create(status_bar);
    lv_obj_remove_style_all(recording_dot);
    lv_obj_set_size(recording_dot, 12, 12);
    lv_obj_set_style_radius(recording_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(recording_dot, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_bg_opa(recording_dot, LV_OPA_COVER, 0);
    lv_obj_add_flag(recording_dot, LV_OBJ_FLAG_HIDDEN);

    // WiFi 图标
    wifi_icon = lv_label_create(status_bar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_add_style(wifi_icon, &icon_style, 0);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x808080), 0);

    // 音量图标
    volume_icon = lv_label_create(status_bar);
    lv_label_set_text(volume_icon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_add_style(volume_icon, &icon_style, 0);
    lv_obj_set_style_text_color(volume_icon, lv_color_hex(0x808080), 0);

    // 电量图标
    battery_icon = lv_label_create(status_bar);
    lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_add_style(battery_icon, &icon_style, 0);
    lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x808080), 0);

    // 相对布局：先落定基准图标的坐标/尺寸，再依次把后续图标排到前一个右侧
    lv_obj_update_layout(workmode_icon);
    lv_obj_align_to(recording_dot, workmode_icon, LV_ALIGN_OUT_RIGHT_MID, STATUS_BAR_ICON_GAP, 0);
    lv_obj_align_to(wifi_icon, recording_dot, LV_ALIGN_OUT_RIGHT_MID, STATUS_BAR_ICON_GAP, 0);
    lv_obj_align_to(volume_icon, wifi_icon, LV_ALIGN_OUT_RIGHT_MID, STATUS_BAR_ICON_GAP, 0);
    lv_obj_align_to(battery_icon, volume_icon, LV_ALIGN_OUT_RIGHT_MID, STATUS_BAR_ICON_GAP, 0);
}

/*
 * 工作模式图标更新（由 DisplayTask 推送）：
 *   有线 → USB，蓝牙 → BT，2.4G → DRIVE
 */
void status_bar_set_working_mode(int mode)
{
    if (workmode_icon == NULL)
    {
        return;
    }
    switch (mode)
    {
    case WIRED_KEYBOARD_MODE:
        lv_label_set_text(workmode_icon, LV_SYMBOL_USB);
        break;
    case BLUETOOTH_KEYBOARD_MODE:
        lv_label_set_text(workmode_icon, LV_SYMBOL_BLUETOOTH);
        break;
    case WIRELESS_2_4G_KEYBOARD_MODE:
        lv_label_set_text(workmode_icon, LV_SYMBOL_DRIVE);
        break;
    default:
        break;
    }
}

// 在status_bar.c中实现
void ui_StatusBar_show(bool show)
{
    if (status_bar == NULL)
        return;

    if (show)
    {
        lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(status_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

/*
 * 状态栏 WiFi 图标更新：
 *   enabled=false → 图标灰（未开启 WiFi）
 *   enabled=true & !connected → 图标橙（正在连接）
 *   enabled=true & connected → 图标白（已连接）
 */
void status_bar_set_wifi_status(bool enabled, bool connected, int rssi)
{
    (void)rssi; // RSSI 仅用于将来扩展图标当前状态档位
    if (wifi_icon == NULL)
    {
        return;
    }
    if (!enabled)
    {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x808080), 0);
    }
    else if (connected)
    {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFFFFFF), 0);
    }
    else
    {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xF59E0B), 0);
    }
}

// 更新电池电量
void status_bar_set_battery_level(uint8_t level)
{
    if (level > 80)
    {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
    }
    else if (level > 50)
    {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_3);
    }
    else if (level > 20)
    {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_2);
    }
    else if (level > 5)
    {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_1);
    }
    else
    {
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_EMPTY);
    }

    // 可以添加颜色变化
    if (level < 20)
    {
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFF0000), 0);
    }
    else
    {
        // lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x808080), 0);
    }
}

// 更新音量状态
void status_bar_set_volume(uint8_t volume)
{
    if (volume == 0)
    {
        lv_label_set_text(volume_icon, LV_SYMBOL_MUTE);
    }
    else if (volume < 66)
    {
        lv_label_set_text(volume_icon, LV_SYMBOL_VOLUME_MID);
    }
    else
    {
        lv_label_set_text(volume_icon, LV_SYMBOL_VOLUME_MAX);
    }
}

// 更新录音状态
void status_bar_set_recording_state(bool is_recording)
{
    if (recording_dot == NULL)
        return;

    if (is_recording)
    {
        recording_blink_visible = true;
        lv_obj_clear_flag(recording_dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(recording_dot, LV_OPA_COVER, 0);

        if (recording_blink_timer == NULL)
        {
            // Blink by toggling the dot opacity.
            recording_blink_timer = lv_timer_create(recording_blink_timer_cb, 300, NULL);
        }
    }
    else
    {
        if (recording_blink_timer != NULL)
        {
            lv_timer_del(recording_blink_timer);
            recording_blink_timer = NULL;
        }
        recording_blink_visible = false;
        lv_obj_set_style_bg_opa(recording_dot, LV_OPA_COVER, 0);
        lv_obj_add_flag(recording_dot, LV_OBJ_FLAG_HIDDEN);
    }
}

// 更新模块插入状态
void status_bar_set_module_status(int mode, bool status)
{
    LV_UNUSED(mode);
    LV_UNUSED(status);
    // MODA / MODB 状态显示已移除，仅保留接口以避免破坏调用方
}
