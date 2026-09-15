#pragma once

// Central pin definitions shared by the firmware and the TFT_eSPI setup.

#if defined(ARDUINO_TINYC6)
// Unexpected Maker TinyC6. I2C and SPI follow the board variant defaults.
#define PIN_I2C_SDA 6
#define PIN_I2C_SCL 7

#define MDA_PIN_SPI_MOSI 21
#define MDA_PIN_SPI_MISO 20
#define MDA_PIN_SPI_SCLK 19

#define PIN_TFT_CS 18
#define PIN_TFT_DC 8
#define PIN_TFT_RST 9
#define PIN_TFT_BL -1

#define PIN_SD_CS 18  // Stacked UM RTC Logger Shield; GPIO10 is VBUS sense.

#define PIN_UI_BUTTON 5
#define PIN_STATUS_LED LED_BUILTIN
#elif defined(ESP8266)
// NodeMCU 1.0 / ESP-12E DevKit V2. GPIO numbers are used in code; the
// matching NodeMCU D-labels are included for wiring.
#define PIN_I2C_SDA 4   // D2
#define PIN_I2C_SCL 5   // D1

#define MDA_PIN_SPI_MOSI 13  // D7
#define MDA_PIN_SPI_MISO 12  // D6
#define MDA_PIN_SPI_SCLK 14  // D5

#define PIN_TFT_CS 15    // D8; must remain low during boot
#define PIN_TFT_DC 0     // D3; must remain high during boot
#define PIN_TFT_RST 2    // D4; must remain high during boot
#define PIN_TFT_BL -1    // Wire backlight to the appropriate supply

#define PIN_SD_CS 16     // D0

#define PIN_UI_BUTTON 3  // RX; firmware only transmits serial diagnostics
#define PIN_STATUS_LED 2 // D4; NodeMCU built-in LED, active low
#elif defined(ESP32)
// Generic classic ESP32 DevKit / ESP32-WROOM-32. These are raw GPIO labels.
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

#define MDA_PIN_SPI_MOSI 23
#define MDA_PIN_SPI_MISO 19
#define MDA_PIN_SPI_SCLK 18

#define PIN_TFT_CS 27
#define PIN_TFT_DC 26
#define PIN_TFT_RST 25
#define PIN_TFT_BL -1

#define PIN_SD_CS 5

#define PIN_UI_BUTTON 32
#define PIN_STATUS_LED 2
#else
#error "Unsupported MCU: add its pin map to PinDefinitions.h"
#endif
