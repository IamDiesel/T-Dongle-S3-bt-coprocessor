#pragma once

// --- Hardware Pins ---
#ifndef BOOT_PIN
#define BOOT_PIN       0
#endif
#define LED_DI_PIN     40
#define LED_CI_PIN     39

#define LCD_HOST       SPI2_HOST
#define PIN_NUM_MISO   -1
#define PIN_NUM_MOSI   3
#define PIN_NUM_CLK    5
#define PIN_NUM_CS     4
#define PIN_NUM_DC     2
#define PIN_NUM_RST    1
#define PIN_NUM_BCKL   38

#define LCD_PIXEL_WIDTH             160
#define LCD_PIXEL_HEIGHT            80
#define LEDC_BACKLIGHT_FREQ         1000
#define LEDC_BACKLIGHT_BIT_WIDTH    8
#define LEDC_BACKLIGHT_CHANNEL      3

// --- Ziel-UUIDs für den Sensor ---
#define TARGET_SERVICE_UUID        "FFF0" 
#define TARGET_CHARACTERISTIC_UUID "FFF1"