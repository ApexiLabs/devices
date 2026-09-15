#include "HttpsWorker.h"
#if defined(ESP32)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {
// HTTPClient decodes framing into this bounded sink. A server cannot allocate
// an unbounded response on the device. Oversize/partial bodies are failures.
class ResponseSink : public Stream {
 public:
  explicit ResponseSink(char *buffer) : buffer_(buffer) { buffer_[0] = '\0'; }
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t *data, size_t size) override {
    if (size > HttpsExchange::kBodyLimit - used_) return 0;
    memcpy(buffer_ + used_, data, size);
    used_ += size;
    buffer_[used_] = '\0';
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
 private:
  char *buffer_;
  size_t used_ = 0;
};
}

HttpsWorker &HttpsWorker::shared(){static HttpsWorker worker;return worker;}

bool HttpsWorker::begin(const char *rootCertificate) {
  if (task_ != nullptr) return true;
  rootCertificate_ = rootCertificate;
  // Keep HTTP/TLS off Arduino's sampling task. On dual-core ESP32 Arduino
  // runs on core 1; on single-core variants RTOS preemption still applies.
  return xTaskCreatePinnedToCore(run, "apexi-https", 12288, this, 1, &task_, 0) == pdPASS;
}

bool HttpsWorker::submit(Owner owner, const char *url, const char *payload, const char *bearer,
                         const char *accessId, const char *accessSecret, bool get) {
  if(owner==Owner::None || task_==nullptr || !arbiter_.request(owner,exchange_.idle()))return false;
  if(!exchange_.submit(url,payload,bearer,accessId,accessSecret,get)){arbiter_.cancel(owner);return false;}
  arbiter_.cancel(owner);
  owner_=owner;
  xTaskNotifyGive(task_);
  return true;
}

void HttpsWorker::run(void *context) {
  auto &self = *static_cast<HttpsWorker *>(context);
  WiFiClientSecure client;
  client.setCACert(self.rootCertificate_);
  client.setTimeout(4000);
  client.setHandshakeTimeout(15);
  HTTPClient http;
  String previousOrigin;
  uint32_t completedAt=millis();
  for (;;) {
    if(!ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000))){
      if(uint32_t(millis()-completedAt)>15000)client.stop();
      continue;
    }
    const auto *request = self.exchange_.request();
    if (request == nullptr) continue;
    auto &result = self.exchange_.workerResult();
    result.status = -1;
    result.tlsError=0;
    result.body[0] = '\0';
    const uint32_t started=millis();
    const String url=request->url;
    const int path=url.indexOf('/',8);
    const String origin=path<0?url:url.substring(0,path);
    if(previousOrigin!=origin)client.stop();
    previousOrigin=origin;
    result.reused=client.connected();
    http.setConnectTimeout(4000);
    http.setTimeout(4000);
    http.setReuse(true);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setUserAgent("ApexiLabs-Logger/1.0");
    if (http.begin(client, request->url)) {
      http.addHeader("Content-Type", "application/json");
      if(request->bearer[0])http.addHeader("Authorization", "Bearer " + String(request->bearer));
      http.addHeader("CF-Access-Client-Id", request->accessId);
      http.addHeader("CF-Access-Client-Secret", request->accessSecret);
      result.status = request->get?http.GET():http.POST(String(request->payload));
      if (result.status > 0) {
        ResponseSink sink(result.body);
        if (http.writeToStream(&sink) < 0) {
          result.status = -1;
          result.body[0] = '\0';
        }
      }
    }
    if(result.status<=0){char detail[128]{};result.tlsError=client.lastError(detail,sizeof(detail));client.stop();}
    http.end();
    result.durationMs=millis()-started;completedAt=millis();
    self.exchange_.complete();
  }
}
#endif
