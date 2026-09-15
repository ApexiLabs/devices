#pragma once

// Waveshare ESP32-S3-Touch-LCD-1.28 onboard GC9A01A wiring.
#define USER_SETUP_ID 128
#define USER_SETUP_INFO "Waveshare ESP32-S3-Touch-LCD-1.28"

#define GC9A01_DRIVER
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

#define TFT_MISO 12
#define TFT_MOSI 11
#define TFT_SCLK 10
#define TFT_CS 9
#define TFT_DC 8
#define TFT_RST 14
#define TFT_BL 2
#define TFT_BACKLIGHT_ON HIGH

// TFT_eSPI 2.5.x needs the explicit register-port mapping with Arduino 3.x:
// Arduino's FSPI identifier is 0 on S3, but the LCD uses SPI2 registers.
#ifndef USE_FSPI_PORT
#define USE_FSPI_PORT
#endif

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_GFXFF

#define SPI_FREQUENCY 80000000
