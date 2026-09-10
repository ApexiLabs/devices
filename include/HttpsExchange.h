#pragma once

#include <atomic>
#include <cstring>
#include <cstdint>

// Single producer (Arduino loop), single consumer (HTTP worker). Buffer
// ownership transfers only through release/acquire state transitions. Neither
// side waits for the other; requests and responses have fixed size limits.
class HttpsExchange {
 public:
  enum class State { Idle, Requested, Complete };
  static constexpr size_t kBodyLimit = 6144;
  struct Request {
    bool get = false;
    char url[513]{};
    char payload[kBodyLimit + 1]{};
    char bearer[2049]{};
    char accessId[257]{};
    char accessSecret[513]{};
  };
  struct Result {
    // Keep cold storage zero-initialized so the shared worker buffers occupy
    // BSS, not a flash-backed data image. No result is visible until complete();
    // HttpsWorker resets status to -1 before attempting each HTTP transaction.
    int status = 0;
    int tlsError = 0;
    uint32_t durationMs = 0;
    bool reused = false;
    char body[kBodyLimit + 1]{};
  };

  bool submit(const char *url, const char *payload, const char *bearer,
              const char *accessId, const char *accessSecret, bool get = false) {
    if (state_.load(std::memory_order_acquire) != State::Idle) return false;
    if (!fits(url, request_.url) || !fits(payload, request_.payload) ||
        !fits(bearer, request_.bearer) || !fits(accessId, request_.accessId) ||
        !fits(accessSecret, request_.accessSecret)) return false;
    std::strcpy(request_.url, url);
    std::strcpy(request_.payload, payload);
    std::strcpy(request_.bearer, bearer);
    std::strcpy(request_.accessId, accessId);
    std::strcpy(request_.accessSecret, accessSecret);
    request_.get = get;
    state_.store(State::Requested, std::memory_order_release);
    return true;
  }
  const Request *request() const {
    return state_.load(std::memory_order_acquire) == State::Requested ? &request_ : nullptr;
  }
  // Worker only, before complete(). Never publish partially written responses.
  Result &workerResult() { return result_; }
  void complete() { state_.store(State::Complete, std::memory_order_release); }
  const Result *result() const {
    return state_.load(std::memory_order_acquire) == State::Complete ? &result_ : nullptr;
  }
  void release() {
    // Main owns both buffers after observing Complete. Do not retain secrets
    // longer than the transaction (no request/response buffers in diagnostics).
    std::memset(request_.url, 0, sizeof(request_.url));
    std::memset(request_.payload, 0, sizeof(request_.payload));
    std::memset(request_.bearer, 0, sizeof(request_.bearer));
    std::memset(request_.accessId, 0, sizeof(request_.accessId));
    std::memset(request_.accessSecret, 0, sizeof(request_.accessSecret));
    std::memset(result_.body, 0, sizeof(result_.body));
    state_.store(State::Idle, std::memory_order_release);
  }
  bool idle() const { return state_.load(std::memory_order_acquire) == State::Idle; }

 private:
  template <size_t N> static bool fits(const char *value, const char (&)[N]) {
    return value != nullptr && std::strlen(value) < N;
  }
  Request request_{};
  Result result_{};
  std::atomic<State> state_{State::Idle};
};
