#include "display_manager.h"
#include "config.h"
#include <lvgl.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_st7735.h"

LV_IMAGE_DECLARE(c1); LV_IMAGE_DECLARE(c2); LV_IMAGE_DECLARE(c3); 
LV_IMAGE_DECLARE(c4); LV_IMAGE_DECLARE(c5); LV_IMAGE_DECLARE(c6); 
LV_IMAGE_DECLARE(c7); LV_IMAGE_DECLARE(c8); 

LV_IMAGE_DECLARE(alarmbutton);
LV_IMAGE_DECLARE(alarm_sprite); 

LV_IMAGE_DECLARE(cat_idle_small);
LV_IMAGE_DECLARE(cat_searching_small);
LV_IMAGE_DECLARE(cat_connected_small);
LV_IMAGE_DECLARE(cat_error_small); 

static esp_lcd_panel_handle_t panel_handle = NULL;

static lv_obj_t * cont_text;
static lv_obj_t * cont_graphic;
static lv_obj_t * overlay_label;
static lv_timer_t * overlay_timer;

static lv_obj_t *ble_status_label;
static lv_obj_t *ble_data_label;

static lv_obj_t *img_cat_idle;
static lv_obj_t *img_cat_scan;
static lv_obj_t *img_cat_conn;
static lv_obj_t *img_cat_error; 

static lv_obj_t *img_alarm_bg;
static lv_obj_t *img_cat_story;
static lv_obj_t *img_alarm_button;

static lv_obj_t *error_overlay_label;
static lv_timer_t *error_timer;
static bool isErrorActive = false;

static bool isAlarmActive = false;
static DisplayMode current_mode = MODE_GRAPHIC_LED_OFF; 
static uint8_t display_brightness = 100; 

const void* alarm_story_frames[] = {
    &c1, &c2, &c3, &c1, &c2, &c3, &c1, &c2, &c3, 
    &c4, &c5,                                    
    &c6, &c7, &c8,                               
    &c8, &c8, &c8, &c8, &c8, &c8                 
};

void applyBacklight() {
    if (isAlarmActive || isErrorActive) {
        ledcWrite(0, map(display_brightness, 0, 100, 255, 0));
        return;
    }
    bool isOff = (current_mode == MODE_OFF_LED_OFF || current_mode == MODE_OFF_LED_ON);
    if (isOff) ledcWrite(0, 255); 
    else ledcWrite(0, map(display_brightness, 0, 100, 255, 0));
}

void setDisplayBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    display_brightness = percent;
    applyBacklight(); 
}

uint8_t getDisplayBrightness() { return display_brightness; }

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    size_t len = lv_area_get_size(area);
    lv_draw_sw_rgb565_swap(color_p, len);
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, (uint16_t *)color_p);
    lv_display_flush_ready(disp);
}

static void overlay_timer_cb(lv_timer_t * timer) {
    lv_obj_add_flag(overlay_label, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer); 
}

void showModeOverlay(String modeName) {
    lv_label_set_text(overlay_label, modeName.c_str());
    lv_obj_clear_flag(overlay_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay_label);
    lv_timer_reset(overlay_timer);
    lv_timer_resume(overlay_timer);
}

static void error_timer_cb(lv_timer_t * timer) {
    isErrorActive = false;
    lv_obj_add_flag(img_cat_error, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(error_overlay_label, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
    setDisplayMode(current_mode); 
}

void showErrorDisplay(String errMsg) {
    if (isAlarmActive) return; 

    isErrorActive = true;
    applyBacklight(); 

    lv_obj_add_flag(cont_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cont_graphic, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(img_cat_idle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(img_cat_scan, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(img_cat_conn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(img_cat_error, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(error_overlay_label, errMsg.c_str());
    lv_obj_clear_flag(error_overlay_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(error_overlay_label);

    lv_timer_reset(error_timer);
    lv_timer_resume(error_timer);
}

bool getErrorActive() { return isErrorActive; }

void setDisplayMode(DisplayMode mode) {
    current_mode = mode;
    if (isAlarmActive || isErrorActive) return; 

    applyBacklight(); 

    bool isText = (mode == MODE_TEXT_LED_OFF || mode == MODE_TEXT_LED_ON);
    bool isGraphic = (mode == MODE_GRAPHIC_LED_OFF || mode == MODE_GRAPHIC_LED_ON);

    if (isText) {
        lv_obj_clear_flag(cont_text, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cont_graphic, LV_OBJ_FLAG_HIDDEN);
    } else if (isGraphic) {
        lv_obj_add_flag(cont_text, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cont_graphic, LV_OBJ_FLAG_HIDDEN);
    }
}

DisplayMode getDisplayMode() { return current_mode; }

void setAlarmActive(bool active) {
    if (isAlarmActive == active) return;
    isAlarmActive = active;

    if (active) {
        isErrorActive = false; 
        lv_timer_pause(error_timer);
        lv_obj_add_flag(error_overlay_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_cat_error, LV_OBJ_FLAG_HIDDEN);

        applyBacklight(); 
        lv_obj_add_flag(cont_text, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cont_graphic, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(img_cat_idle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_cat_scan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_cat_conn, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(img_cat_story, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(img_alarm_button, LV_OBJ_FLAG_HIDDEN);
        
        lv_obj_move_foreground(img_cat_story); 
        lv_obj_move_foreground(img_alarm_button);
        lv_obj_move_foreground(img_alarm_bg); 
        
    } else {
        lv_obj_add_flag(img_cat_story, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_alarm_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_alarm_bg, LV_OBJ_FLAG_HIDDEN);
        setDisplayMode(current_mode);
    }
}

bool getAlarmActive() { return isAlarmActive; }

static void alarm_anim_cb(void * var, int32_t v) {
    if (!isAlarmActive) return;
    lv_image_set_src(img_cat_story, alarm_story_frames[v]);

    int button_x = 35; 
    int cat_stop_x = 13; 
    
    if (v <= 8) {
        int current_x = map(v, 0, 8, -60, cat_stop_x); 
        lv_obj_align(img_cat_story, LV_ALIGN_CENTER, current_x, 0);
    } else {
        lv_obj_align(img_cat_story, LV_ALIGN_CENTER, cat_stop_x, 0);
    }

    lv_obj_align(img_alarm_button, LV_ALIGN_CENTER, button_x, 0);

    if (v >= 13) {
        if (v % 2 == 0) lv_obj_clear_flag(img_alarm_bg, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(img_alarm_bg, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(img_alarm_bg, LV_OBJ_FLAG_HIDDEN); 
    }
    lv_obj_invalidate(cont_graphic); 
}

static void cat_anim_cb(void * var, int32_t v) {
    if (!isAlarmActive && !isErrorActive) lv_obj_set_y((lv_obj_t *)var, v);
}

void initDisplay() {
    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_NUM_BCKL, 0);
    applyBacklight(); 

    static spi_bus_config_t spi_config = ST7735_PANEL_BUS_SPI_CONFIG(PIN_NUM_CLK, PIN_NUM_MOSI, LCD_PIXEL_WIDTH * LCD_PIXEL_HEIGHT * sizeof(uint16_t));
    static esp_lcd_panel_io_spi_config_t io_config = ST7735_PANEL_IO_SPI_CONFIG(PIN_NUM_CS, PIN_NUM_DC, NULL, NULL);
    static esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };

    esp_lcd_panel_io_handle_t io_handle = NULL;
    spi_bus_initialize(LCD_HOST, &spi_config, SPI_DMA_CH_AUTO);
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle);
    esp_lcd_new_panel_st7735(io_handle, &panel_config, &panel_handle);
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_set_gap(panel_handle, 1, 26);
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, true);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    lv_init();
    size_t lv_buffer_size = LCD_PIXEL_WIDTH * LCD_PIXEL_HEIGHT * sizeof(lv_color16_t);
    lv_color16_t *buf = (lv_color16_t *)malloc(lv_buffer_size);
    lv_display_t *disp_drv = lv_display_create(LCD_PIXEL_WIDTH, LCD_PIXEL_HEIGHT);
    lv_display_set_buffers(disp_drv, buf, NULL, lv_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp_drv, disp_flush);
    lv_tick_set_cb([]() { return (uint32_t)millis(); });

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
    lv_obj_remove_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE); 

    cont_text = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont_text, 160, 80);
    lv_obj_set_style_pad_all(cont_text, 0, 0);
    lv_obj_set_style_radius(cont_text, 0, 0); 
    lv_obj_set_style_bg_color(cont_text, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(cont_text, 0, 0);
    lv_obj_remove_flag(cont_text, LV_OBJ_FLAG_SCROLLABLE); 

    ble_status_label = lv_label_create(cont_text);
    lv_obj_set_style_text_font(ble_status_label, lv_theme_get_font_normal(NULL), 0);
    lv_obj_align(ble_status_label, LV_ALIGN_TOP_MID, 0, 2);

    static lv_point_precise_t line_points[] = { {0, 0}, {160, 0} };
    lv_obj_t * line = lv_line_create(cont_text);
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(0x333333), 0);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 18);

    ble_data_label = lv_label_create(cont_text);
    lv_obj_set_style_text_color(ble_data_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ble_data_label, lv_theme_get_font_small(NULL), 0);
    lv_obj_align(ble_data_label, LV_ALIGN_TOP_LEFT, 2, 22);

    cont_graphic = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont_graphic, 160, 80);
    lv_obj_set_style_pad_all(cont_graphic, 0, 0);
    lv_obj_set_style_radius(cont_graphic, 0, 0); 
    lv_obj_set_style_bg_color(cont_graphic, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(cont_graphic, 0, 0);
    lv_obj_remove_flag(cont_graphic, LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_add_flag(cont_graphic, LV_OBJ_FLAG_HIDDEN);

    img_alarm_bg = lv_image_create(cont_graphic);
    lv_image_set_src(img_alarm_bg, &alarm_sprite); 
    lv_obj_align(img_alarm_bg, LV_ALIGN_CENTER, 30, 0);
    lv_obj_add_flag(img_alarm_bg, LV_OBJ_FLAG_HIDDEN);

    img_cat_idle = lv_image_create(cont_graphic);
    lv_image_set_src(img_cat_idle, &cat_idle_small);
    lv_obj_align(img_cat_idle, LV_ALIGN_CENTER, 0, 0);

    img_cat_scan = lv_image_create(cont_graphic);
    lv_image_set_src(img_cat_scan, &cat_searching_small);
    lv_obj_align(img_cat_scan, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(img_cat_scan, LV_OBJ_FLAG_HIDDEN);

    img_cat_conn = lv_image_create(cont_graphic);
    lv_image_set_src(img_cat_conn, &cat_connected_small);
    lv_obj_align(img_cat_conn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(img_cat_conn, LV_OBJ_FLAG_HIDDEN);
    
    img_cat_error = lv_image_create(cont_graphic);
    lv_image_set_src(img_cat_error, &cat_error_small);
    lv_obj_align(img_cat_error, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(img_cat_error, LV_OBJ_FLAG_HIDDEN);

    img_cat_story = lv_image_create(cont_graphic);
    lv_image_set_src(img_cat_story, &c1);
    lv_obj_align(img_cat_story, LV_ALIGN_CENTER, -60, 0);
    lv_obj_add_flag(img_cat_story, LV_OBJ_FLAG_HIDDEN);

    img_alarm_button = lv_image_create(cont_graphic);
    lv_image_set_src(img_alarm_button, &alarmbutton);
    lv_obj_align(img_alarm_button, LV_ALIGN_CENTER, 35, 0);
    lv_obj_add_flag(img_alarm_button, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_values(&a, -2, 2);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_playback_duration(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, cat_anim_cb);
    lv_anim_set_var(&a, img_cat_idle); lv_anim_start(&a);
    lv_anim_set_var(&a, img_cat_scan); lv_anim_start(&a);
    lv_anim_set_var(&a, img_cat_conn); lv_anim_start(&a);
    lv_anim_set_var(&a, img_cat_error); lv_anim_start(&a); 

    lv_anim_t a_alarm;
    lv_anim_init(&a_alarm);
    lv_anim_set_var(&a_alarm, img_cat_story); 
    lv_anim_set_values(&a_alarm, 0, 19);      
    lv_anim_set_duration(&a_alarm, 3500);     
    lv_anim_set_repeat_count(&a_alarm, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a_alarm, alarm_anim_cb);
    lv_anim_set_path_cb(&a_alarm, lv_anim_path_linear); 
    lv_anim_start(&a_alarm);

    overlay_label = lv_label_create(lv_screen_active());
    lv_obj_set_style_bg_color(overlay_label, lv_color_hex(0xFF8800), 0);
    lv_obj_set_style_text_color(overlay_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(overlay_label, lv_theme_get_font_small(NULL), 0);
    lv_obj_set_style_pad_all(overlay_label, 3, 0);
    lv_obj_set_style_radius(overlay_label, 4, 0);
    lv_obj_align(overlay_label, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_add_flag(overlay_label, LV_OBJ_FLAG_HIDDEN);
    
    overlay_timer = lv_timer_create(overlay_timer_cb, 1200, NULL);
    lv_timer_pause(overlay_timer);

    // FIX: Fehler-Overlay jetzt am unteren Rand zentriert
    error_overlay_label = lv_label_create(lv_screen_active());
    lv_obj_set_style_bg_color(error_overlay_label, lv_color_hex(0xD32F2F), 0); 
    lv_obj_set_style_text_color(error_overlay_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(error_overlay_label, lv_theme_get_font_small(NULL), 0);
    lv_obj_set_style_pad_all(error_overlay_label, 4, 0);
    lv_obj_set_style_radius(error_overlay_label, 4, 0);
    lv_obj_align(error_overlay_label, LV_ALIGN_BOTTOM_MID, 0, -5); 
    lv_obj_add_flag(error_overlay_label, LV_OBJ_FLAG_HIDDEN);

    error_timer = lv_timer_create(error_timer_cb, 10000, NULL); 
    lv_timer_pause(error_timer);

    setDisplayMode(current_mode); 
}

void updateDisplayUi(const std::vector<BleDevice>& devices, String statusMsg, bool isConnected, bool isScanning) {
    if (isAlarmActive || isErrorActive) return; 

    if (isConnected) {
        lv_label_set_text(ble_status_label, "Verbunden");
        lv_obj_set_style_text_color(ble_status_label, lv_color_hex(0x45A34C), 0); 
        lv_label_set_text(ble_data_label, statusMsg.c_str());
    } else if (isScanning) {
        lv_label_set_text(ble_status_label, "Scanne Umgebung");
        lv_obj_set_style_text_color(ble_status_label, lv_color_hex(0x0096FF), 0); 
        String list = "";
        for (size_t i = 0; i < std::min((size_t)4, devices.size()); ++i) {
            String dName = devices[i].name != "" ? devices[i].name : devices[i].mac.substring(9);
            if (dName.length() > 14) dName = dName.substring(0, 12) + "..";
            list += String(i+1) + ". " + dName + " (" + String(devices[i].rssi) + ")\n";
        }
        if (list == "") list = "Warte auf Beacons...";
        lv_label_set_text(ble_data_label, list.c_str());
    } else {
        lv_label_set_text(ble_status_label, "Standby");
        lv_obj_set_style_text_color(ble_status_label, lv_color_hex(0xFF8800), 0); 
        lv_label_set_text(ble_data_label, "Idle-Modus.\nWarte auf Befehle\nvom M5Tab...");
    }

    if (isConnected) {
        lv_obj_add_flag(img_cat_idle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_cat_scan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(img_cat_conn, LV_OBJ_FLAG_HIDDEN); 
    } else if (isScanning) {
        lv_obj_add_flag(img_cat_idle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(img_cat_scan, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(img_cat_conn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(img_cat_idle, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(img_cat_scan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_cat_conn, LV_OBJ_FLAG_HIDDEN);
    }
}

void keepDisplayAlive() { 
    lv_timer_handler(); 
}