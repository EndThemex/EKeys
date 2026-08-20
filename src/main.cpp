#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_DC 9
#define TFT_RST 21
#define TFT_CS 10
#define TFT_BL 2

#define TFT_MISO GFX_NOT_DEFINED

/*
 * ------------------------------------------------------------
 * Display configuration
 * ------------------------------------------------------------
 *
 * Native:
 *
 *     142 x 428
 *
 * After rotation 1:
 *
 *     428 x 142
 */
#define TFT_WIDTH 142
#define TFT_HEIGHT 428
#define SCREEN_WIDTH 428
#define SCREEN_HEIGHT 142

/*
 * ------------------------------------------------------------
 * SPI bus
 * ------------------------------------------------------------
 */
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    TFT_DC,
    TFT_CS,
    TFT_SCLK,
    TFT_MOSI,
    TFT_MISO);

/*
 * ------------------------------------------------------------
 * NV3007 display
 * ------------------------------------------------------------
 *
 * The 2.79" NV3007 uses a special initialization sequence.
 *
 * Arduino_GFX already contains:
 *
 *     nv3007_279_init_operations
 *
 * and the corresponding constructor.
 */
Arduino_GFX *gfx = new Arduino_NV3007(
    bus,
    TFT_RST,
    1,          // rotation
    false,      // IPS
    TFT_WIDTH,  // 142
    TFT_HEIGHT, // 428
    12,         // column offset 1
    0,          // row offset 1
    14,         // column offset 2
    0,          // row offset 2
    nv3007_279_init_operations,
    sizeof(nv3007_279_init_operations));

/*
 * ------------------------------------------------------------
 * LVGL configuration
 * ------------------------------------------------------------
 */

static lv_disp_draw_buf_t draw_buf;

/*
 * 428 x 20 x RGB565
 *
 * Approximately:
 *
 *     428 * 20 * 2 = 17.1 KB
 *
 * Using partial rendering keeps RAM usage low.
 */
#define LVGL_BUFFER_LINES 20

static lv_color_t lv_buf1[SCREEN_WIDTH * LVGL_BUFFER_LINES];
static lv_color_t lv_buf2[SCREEN_WIDTH * LVGL_BUFFER_LINES];

/*
 * ------------------------------------------------------------
 * LVGL tick
 * ------------------------------------------------------------
 */

static uint32_t lvgl_tick_cb()
{
    return millis();
}

/*
 * ------------------------------------------------------------
 * Display flush
 * ------------------------------------------------------------
 */

static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width = (area->x2 - area->x1 + 1);
    uint32_t height = (area->y2 - area->y1 + 1);

    /*
     * LVGL RGB565 byte order
     *
     * LV_COLOR_16_SWAP = 0
     *
     * Therefore use draw16bitRGBBitmap().
     */
#if LV_COLOR_16_SWAP != 0
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, width, height);
#endif

    lv_disp_flush_ready(disp_drv);
}

/*
 * ------------------------------------------------------------
 * LVGL display initialization
 * ------------------------------------------------------------
 */

static void lvgl_display_init()
{
    /* Initialize draw buffers */
    lv_disp_draw_buf_init(&draw_buf, lv_buf1, lv_buf2, SCREEN_WIDTH * LVGL_BUFFER_LINES);

    /* Initialize display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    /* Logical resolution */
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;

    /* Partial rendering buffer */
    disp_drv.draw_buf = &draw_buf;

    /* Flush callback */
    disp_drv.flush_cb = my_disp_flush;

    /* Register LVGL display */
    lv_disp_drv_register(&disp_drv);
}

/*
 * ------------------------------------------------------------
 * Create LVGL UI
 * ------------------------------------------------------------
 */

static void create_ui()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), LV_PART_MAIN);

    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "NV3007");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 15, 10);

    /* Resolution label */
    lv_obj_t *resolution = lv_label_create(screen);
    lv_label_set_text(resolution, "428 x 142");
    lv_obj_set_style_text_color(resolution, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(resolution, LV_ALIGN_TOP_LEFT, 15, 38);

    /* Status */
    lv_obj_t *status = lv_label_create(screen);
    lv_label_set_text(status, "ESP32-S3 + LVGL");
    lv_obj_set_style_text_color(status, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_TOP_LEFT, 15, 62);

    /* Button */
    lv_obj_t *button = lv_btn_create(screen);
    lv_obj_set_size(button, 120, 45);
    lv_obj_align(button, LV_ALIGN_RIGHT_MID, -15, 0);

    /* Button label */
    lv_obj_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, "TEST");
    lv_obj_center(button_label);
}

/*
 * ------------------------------------------------------------
 * Arduino setup
 * ------------------------------------------------------------
 */

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================");
    Serial.println("ESP32-S3 NV3007 + LVGL");
    Serial.println("==============================");

    /* Backlight */
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    /* Initialize LCD */
    Serial.println("Initializing NV3007...");
    if (!gfx->begin(10000000))
    {
        Serial.println("ERROR: gfx->begin() failed!");
        while (true)
        {
            delay(1000);
        }
    }
    Serial.println("NV3007 initialized.");

    /* Clear screen */
    gfx->fillScreen(RGB565_BLACK);

    /* Initialize LVGL */
    lv_init();
    lvgl_display_init();
    create_ui();

    Serial.println("LVGL initialized.");
    Serial.println("Setup completed.");
}

/*
 * ------------------------------------------------------------
 * Arduino loop
 * ------------------------------------------------------------
 */

void loop()
{
    static uint32_t last_ms = millis();
    uint32_t now = millis();

    if (now != last_ms)
    {
        lv_tick_inc(now - last_ms);
        last_ms = now;
    }

    lv_timer_handler();
    delay(5);
}