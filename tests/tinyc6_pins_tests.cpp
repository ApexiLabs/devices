#define ARDUINO_TINYC6
#include "PinDefinitions.h"

// UM RTC Logger Shield's published stacked TinyC6 connections.
static_assert(PIN_SD_CS == 18, "SD CS must not use VBUS sense GPIO10");
static_assert(MDA_PIN_SPI_MOSI == 21 && MDA_PIN_SPI_MISO == 20);
static_assert(MDA_PIN_SPI_SCLK == 19);
static_assert(PIN_I2C_SDA == 6 && PIN_I2C_SCL == 7);
int main() {}
