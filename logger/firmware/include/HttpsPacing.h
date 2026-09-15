#pragma once
#include <cstdint>

// Heartbeat scheduling is based on completed attempts, not successful ones.
// An unavailable/rejecting status endpoint must not monopolize the worker.
class HttpsPacing {
 public:
  enum class Work { Wait, Status, Snapshot };
  Work next(uint32_t now, bool busy, bool connected, bool backoff,
            uint32_t lastCompletion, uint32_t retryMs, bool statusRequested,
            bool hasSnapshot) const {
    if (busy || !connected || (backoff && uint32_t(now - lastCompletion) < retryMs)) return Work::Wait;
    if (statusRequested || !attempted_ || uint32_t(now - lastStatusAttempt_) >= 30000) return Work::Status;
    return hasSnapshot ? Work::Snapshot : Work::Wait;
  }
  void statusCompleted(uint32_t now) { attempted_ = true; lastStatusAttempt_ = now; }
 private:
  bool attempted_ = false;
  uint32_t lastStatusAttempt_ = 0;
};
