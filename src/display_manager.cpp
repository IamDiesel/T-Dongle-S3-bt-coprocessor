#include "display_manager.h"
#include "config.h"
#include <lvgl.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_st7735.h"

static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_obj_t *ble_status_label;
static lv_obj_t *ble_data_label;

LV_FONT_DECLARE(lv_font_montserrat_8);

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    size_t len = lv_area_get_size(area);
    lv_draw_sw_rgb565_swap(color_p, len);
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, (uint16_t *)color_p);
    lv_display_flush_ready(disp);
}

static uint32_t lv_tick_get_callback(void) { return millis(); }

void initDisplay() {
    pinMode(PIN_NUM_BCKL, OUTPUT);
    digitalWrite(PIN_NUM_BCKL, HIGH);

    static spi_bus_config_t spi_config = ST7735_PANEL_BUS_SPI_CONFIG(PIN_NUM_CLK, PIN_NUM_MOSI, LCD_PIXEL_WIDTH * LCD_PIXEL_HEIGHT * sizeof(uint16_t));
    static esp_lcd_panel_io_spi_config_t io_config = ST7735_PANEL_IO_SPI_CONFIG(PIN_NUM_CS, PIN_NUM_DC, NULL, NULL);
    static esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
#else
        .color_space = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
#endif
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
    lv_display_set_color_format(disp_drv, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp_drv, disp_flush);
    lv_tick_set_cb(lv_tick_get_callback);

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    
    ble_status_label = lv_label_create(lv_screen_active());
    lv_label_set_text(ble_status_label, "System Boot");
    lv_obj_set_style_text_color(ble_status_label, lv_color_make(69, 163, 76), 0); 
    lv_obj_align(ble_status_label, LV_ALIGN_TOP_MID, 0, 0); 

    ble_data_label = lv_label_create(lv_screen_active());
    lv_label_set_text(ble_data_label, "Initialisiere...");
    lv_obj_set_style_text_color(ble_data_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(ble_data_label, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_line_space(ble_data_label, 1, 0); 
    lv_obj_align(ble_data_label, LV_ALIGN_TOP_LEFT, 2, 18); 

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
    ledcAttach(PIN_NUM_BCKL, LEDC_BACKLIGHT_FREQ, LEDC_BACKLIGHT_BIT_WIDTH);
    ledcWrite(PIN_NUM_BCKL, 0); 
#else
    ledcSetup(LEDC_BACKLIGHT_CHANNEL, LEDC_BACKLIGHT_FREQ, LEDC_BACKLIGHT_BIT_WIDTH);
    ledcAttachPin(PIN_NUM_BCKL, LEDC_BACKLIGHT_CHANNEL);
    ledcWrite(LEDC_BACKLIGHT_CHANNEL, 0); 
#endif
}

void updateDisplayUi(const std::vector<BleDevice>& devices, String statusMsg, bool isConnected) {
    if (isConnected) {
        lv_label_set_text(ble_status_label, "BLE Client Mode");
        lv_label_set_text(ble_data_label, statusMsg.c_str());
    } else {
        lv_label_set_text(ble_status_label, "Top 5 Beacons");
        
        String displayText = "";
        for (size_t i = 0; i < std::min((size_t)5, devices.size()); ++i) {
            String dName = devices[i].name != "" ? devices[i].name : devices[i].mac.substring(9);
            if (dName.length() > 16) dName = dName.substring(0, 14) + "..";
            displayText += String(i+1) + ". " + dName + " : " + String(devices[i].rssi) + " dBm\n";
        }
        
        if (displayText == "") displayText = "Keine Geräte in\nReichweite...";
        lv_label_set_text(ble_data_label, displayText.c_str());
    }
}

void keepDisplayAlive() {
    lv_timer_handler();
}