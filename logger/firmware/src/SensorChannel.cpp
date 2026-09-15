#include "SensorChannel.h"

#include <algorithm>

#include "Logic.h"

SensorChannel::SensorChannel(const AppConfig::SensorConfig &config) { configure(config); }

void SensorChannel::configure(const AppConfig::SensorConfig &config) { config_ = &config; }

void SensorChannel::update(const float sensedVoltage, const bool adcAvailable) {
  if (config_ == nullptr) {
    return;
  }

  rawVoltage_ = sensedVoltage;
  currentmA_ = Logic::currentFromVoltage(sensedVoltage, AppConfig::kShuntResistanceOhms);
  activeFault_ = Logic::determineSensorFault(sensedVoltage, currentmA_, adcAvailable);

  if (activeFault_ != SensorFault::None) {
    latchedFault_ = activeFault_;
    if (!hasValidSample_) {
      engineeringValue_ = config_->engMin;
      filteredValue_ = config_->engMin;
      minValue_ = config_->engMin;
      maxValue_ = config_->engMax;
    }
    return;
  }

  engineeringValue_ = Logic::scaleEngineeringValue(
      currentmA_, config_->currentMinmA, config_->currentMaxmA, config_->engMin, config_->engMax);
  if (!hasValidSample_) {
    filteredValue_ = engineeringValue_;
    minValue_ = engineeringValue_;
    maxValue_ = engineeringValue_;
    hasValidSample_ = true;
  } else {
    filteredValue_ =
        Logic::applyLowPassFilter(filteredValue_, engineeringValue_, config_->filterAlpha);
    minValue_ = std::min(minValue_, filteredValue_);
    maxValue_ = std::max(maxValue_, filteredValue_);
  }
}

void SensorChannel::clearLatchedFault() {
  latchedFault_ = activeFault_;
}

SensorSnapshot SensorChannel::snapshot() const {
  return SensorSnapshot{
      config_ != nullptr ? config_->id : "",
      config_ != nullptr ? config_->name : "",
      config_ != nullptr ? config_->units : "",
      rawVoltage_,
      currentmA_,
      engineeringValue_,
      filteredValue_,
      minValue_,
      maxValue_,
      config_ != nullptr ? config_->warnLow : 0.0f,
      config_ != nullptr ? config_->warnHigh : 0.0f,
      config_ != nullptr ? config_->engMin : 0.0f,
      config_ != nullptr ? config_->engMax : 0.0f,
      activeFault_,
      latchedFault_,
      hasValidSample_,
  };
}
