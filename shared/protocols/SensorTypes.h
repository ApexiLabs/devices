#pragma once

#include <stdint.h>

enum class SensorFault : uint8_t {
  None,
  Underrange,
  Overrange,
  AdcSaturation,
  AdcUnavailable,
};

inline const char *sensorFaultToString(const SensorFault fault) {
  switch (fault) {
    case SensorFault::None:
      return "none";
    case SensorFault::Underrange:
      return "underrange";
    case SensorFault::Overrange:
      return "overrange";
    case SensorFault::AdcSaturation:
      return "adc_saturation";
    case SensorFault::AdcUnavailable:
      return "adc_unavailable";
  }
  return "unknown";
}
