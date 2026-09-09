#include "DashLink.h"
#include "SystemLog.h"

#include "DashLinkProtocol.h"

#if defined(ESP32)
#include <BLEDevice.h>

class DashLink::ClientCallbacks : public BLEClientCallbacks {
 public:
  explicit ClientCallbacks(DashLink &owner) : owner_(owner) {}

  void onConnect(BLEClient *) override { owner_.connected_ = true; }

  void onDisconnect(BLEClient *) override { owner_.connected_ = false; }

 private:
  DashLink &owner_;
};

DashLink *DashLink::activeInstance_ = nullptr;
#endif

void DashLink::begin(const AppConfig::DashLinkConfig &config) {
  config_ = config;
  if (!config_.enabled) {
    status_ = "disabled";
    return;
  }

#if defined(ESP32)
  BLEDevice::init(DashLinkProtocol::kLoggerDeviceName);
  activeInstance_ = this;
  client_ = BLEDevice::createClient();
  callbacks_ = new ClientCallbacks(*this);
  client_->setClientCallbacks(callbacks_);
  initialized_ = true;
  status_ = "waiting";
  lastAttemptMs_ = millis() - config_.retryIntervalMs;
#else
  status_ = "unsupported";
#endif
}

void DashLink::loop(const uint32_t nowMs) {
  if (!initialized_) {
    return;
  }

#if defined(ESP32)
  if (client_ != nullptr && client_->isConnected()) {
    connected_ = true;
    status_ = "connected";
    return;
  }

  connected_ = false;
  if (scanComplete_) {
    scanComplete_ = false;
    finishScan();
    return;
  }

  BLEScan *scan = BLEDevice::getScan();
  if (scan->isScanning()) {
    return;
  }

  if (!DashLinkProtocol::retryDue(nowMs, lastAttemptMs_,
                                  config_.retryIntervalMs)) {
    return;
  }

  lastAttemptMs_ = nowMs;
  attemptConnection();
#endif
}

bool DashLink::isEnabled() const { return config_.enabled; }

bool DashLink::isConnected() const { return connected_; }

const String &DashLink::status() const { return status_; }

void DashLink::attemptConnection() {
#if defined(ESP32)
  status_ = "scanning";
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scanComplete_ = false;
  if (!scan->start(config_.scanDurationSeconds, onScanComplete, false)) {
    status_ = "scan failed";
  }
#endif
}

#if defined(ESP32)
void DashLink::onScanComplete(BLEScanResults) {
  if (activeInstance_ != nullptr) {
    activeInstance_->scanComplete_ = true;
  }
}

void DashLink::finishScan() {
  BLEScan *scan = BLEDevice::getScan();
  BLEScanResults *results = scan->getResults();
  if (results == nullptr) {
    status_ = "scan failed";
    return;
  }

  BLEAdvertisedDevice dash;
  bool found = false;
  for (int index = 0; index < results->getCount(); ++index) {
    BLEAdvertisedDevice candidate = results->getDevice(index);
    if (candidate.isAdvertisingService(
            BLEUUID(DashLinkProtocol::kServiceUuid))) {
      dash = candidate;
      found = true;
      break;
    }
  }

  if (!found) {
    status_ = "not found";
    scan->clearResults();
    return;
  }

  status_ = "connecting";
  telemetry_ = nullptr;
  if (!client_->connectTimeout(&dash, 2000)) {
    status_ = "connect failed";
    scan->clearResults();
    return;
  }

  BLERemoteService *service =
      client_->getService(DashLinkProtocol::kServiceUuid);
  BLERemoteCharacteristic *handshake =
      service == nullptr
          ? nullptr
          : service->getCharacteristic(
                DashLinkProtocol::kHandshakeCharacteristicUuid);
  if (handshake == nullptr ||
      !handshake->writeValue(String(DashLinkProtocol::kHandshake), true)) {
    status_ = "handshake failed";
    client_->disconnect();
    scan->clearResults();
    return;
  }

  connected_ = true;
  telemetry_ = service->getCharacteristic(DashTelemetry::kUuid);
  events_ = service->getCharacteristic(SystemEvents::kUuid);
  portENTER_CRITICAL(&eventMux_); eventReady_=false; eventReceiver_={}; portEXIT_CRITICAL(&eventMux_);
  if(events_ && events_->canNotify()) events_->registerForNotify(onEvent);
  dashIdentity_=dash.getAddress().toString().c_str(); dashIdentity_.replace(":", "");
  sensorIndex_ = 0;
  frameKind_ = DashTelemetry::Id;
  status_ = "connected";
  scan->clearResults();
}
void DashLink::onEvent(BLERemoteCharacteristic *,uint8_t *data,size_t length,bool) {
  if(!activeInstance_) return;
  auto &self=*activeInstance_;
  portENTER_CRITICAL(&self.eventMux_);
  SystemEvents::Event event{};
  if(self.eventReceiver_.accept(data,length,event)) { self.receivedEvent_=event; self.eventReady_=true; }
  portEXIT_CRITICAL(&self.eventMux_);
}
#endif

void DashLink::publish(const std::array<SensorSnapshot, AppConfig::kSensorCount> &sensors,
                       uint32_t nowMs) {
#if defined(ESP32)
  if(connected_ && events_ && uint32_t(nowMs-lastEventMs_)>=1000) {
    lastEventMs_=nowMs;
    SystemEvents::Event event{}; bool ready;
    portENTER_CRITICAL(&eventMux_); ready=eventReady_; event=receivedEvent_; eventReady_=false; portEXIT_CRITICAL(&eventMux_);
    if(ready) {
      event.code[31]=0;
      if(systemLog.acceptDash(event,dashIdentity_)) {
        uint32_t ack[2]={event.boot,event.seq}; events_->writeValue(reinterpret_cast<uint8_t *>(ack),sizeof(ack),false);
      }
    }
  }
#endif
#if defined(ESP32)
  static_assert(AppConfig::kSensorCount <= DashTelemetry::kMaxSensors, "Dash supports up to 8 sensors");
  if (!connected_ || !client_ || !client_->isConnected() || !telemetry_) return;
  // Send one small unacknowledged frame per call, without waiting for BLE responses.
  // Repeat metadata with each cycle so packet loss cannot strand the catalog.
  constexpr uint32_t spacing = DashTelemetry::kPublishMs / (AppConfig::kSensorCount * 4);
  if (uint32_t(nowMs-lastFrameMs_) < spacing) return;
  lastFrameMs_ = nowMs;
  const auto &s = sensors[sensorIndex_];
  if (std::strlen(s.id) > 15) return;  // Never silently truncate stable identifiers.
  DashTelemetry::Frame frame;
  if (frameKind_ == DashTelemetry::Sample) {
    frame = DashTelemetry::sample(sensorIndex_, sensors.size(), s.filteredValue,
      s.hasValidSample, s.activeFault, s.filteredValue < s.warnLow || s.filteredValue > s.warnHigh);
  } else {
    frame = DashTelemetry::text(static_cast<DashTelemetry::Kind>(frameKind_), sensorIndex_,
      sensors.size(), frameKind_ == DashTelemetry::Id ? s.id : frameKind_ == DashTelemetry::Name ? s.name : s.units);
  }
  if (!telemetry_->writeValue(frame.data(), frame.size(), false)) return;
  if (++frameKind_ > DashTelemetry::Sample) {
    frameKind_ = DashTelemetry::Id;
    sensorIndex_ = (sensorIndex_ + 1) % sensors.size();
  }
#else
  (void)sensors; (void)nowMs;
#endif
}
