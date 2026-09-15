#pragma once

#include <Arduino.h>
#if !defined(MDA_HEADLESS_DISPLAY)
#include <TFT_eSPI.h>
#endif
#include "AppConfig.h"
#include "Types.h"

class Dashboard {
 public:
  enum class Screen : uint8_t {
    Main,
    Diagnostics,
  };

  bool begin();
  bool isReady() const;
  void render(const AppState &state);
  void nextScreen();
 Screen currentScreen() const;

 private:
#if !defined(MDA_HEADLESS_DISPLAY)
  void drawMainScreen(const AppState &state);
  void drawDiagnosticsScreen(const AppState &state);
  uint8_t diagnosticsPageCount() const;
  void drawGauge(int16_t centerX,
                 int16_t centerY,
                 int16_t radius,
                 const SensorSnapshot &sensor);
  void drawGaugeArc(int16_t centerX,
                    int16_t centerY,
                    int16_t radius,
                    int16_t thickness,
                    float startDeg,
                    float endDeg,
                    uint16_t color);
  void drawStatusLine(int16_t x,
                      int16_t y,
                      const char *label,
                      const String &value,
                      uint16_t color);
  uint16_t valueColor(const SensorSnapshot &sensor) const;

  TFT_eSPI tft_;
#endif
  Screen screen_ = Screen::Main;
  uint8_t diagnosticsPage_ = 0;
  bool ready_ = false;
};
