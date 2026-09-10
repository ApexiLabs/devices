#pragma once

#include "PinDefinitions.h"

#define ST7796_DRIVER

#define TFT_MISO MDA_PIN_SPI_MISO
#define TFT_MOSI MDA_PIN_SPI_MOSI
#define TFT_SCLK MDA_PIN_SPI_SCLK
#define TFT_CS   PIN_TFT_CS
#define TFT_DC   PIN_TFT_DC
#define TFT_RST  PIN_TFT_RST

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 16000000
#define SPI_TOUCH_FREQUENCY 2500000
