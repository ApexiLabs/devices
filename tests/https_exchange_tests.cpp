#include "HttpsExchange.h"
#include "QueueAge.h"
#include "HttpsPacing.h"
#include "HttpsArbiter.h"
#include <cassert>
#include <thread>
#include <string>
#include <iostream>

static void testColdResultLifecycle() {
  HttpsExchange exchange;
  assert(exchange.workerResult().status == 0);
  assert(exchange.result() == nullptr);
  assert(exchange.submit("https://example", "{}", "bearer", "id", "secret"));
  // A cold zero is never an externally visible HTTP result.
  assert(exchange.result() == nullptr);
  exchange.workerResult().status = -1;  // Worker prepares a failed attempt.
  assert(exchange.result() == nullptr);
  exchange.complete();
  assert(exchange.result()->status == -1);
  exchange.release();
  assert(exchange.result() == nullptr);

  assert(exchange.submit("https://example", "{}", "bearer", "id", "secret"));
  exchange.workerResult().status = 200;
  assert(exchange.result() == nullptr);
  exchange.complete();
  assert(exchange.result()->status == 200);
  exchange.release();

  assert(exchange.submit("https://example", "{}", "bearer", "id", "secret"));
  // Explicit per-request reset replaces the previous success when HTTP setup
  // fails; neither stale success nor intermediate failure is published early.
  assert(exchange.workerResult().status == 200);
  assert(exchange.result() == nullptr);
  exchange.workerResult().status = -1;
  assert(exchange.result() == nullptr);
  exchange.complete();
  assert(exchange.result()->status == -1);
  exchange.release();
  assert(exchange.result() == nullptr && exchange.idle());
}

int main() {
  testColdResultLifecycle();
  HttpsArbiter arbiter;using Owner=HttpsArbiter::Owner;
  assert(!arbiter.request(Owner::Authorization,false));
  assert(!arbiter.request(Owner::Logs,false));
  assert(!arbiter.request(Owner::Telemetry,true));
  assert(arbiter.request(Owner::Authorization,true));arbiter.cancel(Owner::Authorization);
  assert(!arbiter.request(Owner::Telemetry,true));
  assert(arbiter.request(Owner::Logs,true));arbiter.cancel(Owner::Logs);
  assert(arbiter.request(Owner::Telemetry,true));arbiter.cancel(Owner::Telemetry);
  HttpsPacing pacing;
  using Work = HttpsPacing::Work;
  assert(pacing.next(0, false, true, false, 0, 5000, false, true) == Work::Status);
  for (uint32_t completion = 0; completion < 300000; completion += 30000) {
    // Repeat rejected/unavailable status completions. Every attempt yields
    // snapshot work after backoff; next heartbeat is bounded to 30 seconds.
    pacing.statusCompleted(completion);
    assert(pacing.next(completion + 4999, false, true, true, completion, 5000, false, true) == Work::Wait);
    assert(pacing.next(completion + 5000, false, true, true, completion, 5000, false, true) == Work::Snapshot);
    assert(pacing.next(completion + 29999, false, true, false, completion, 5000, false, true) == Work::Snapshot);
    assert(pacing.next(completion + 30000, false, true, false, completion, 5000, false, true) == Work::Status);
    assert(pacing.next(completion + 5000, true, true, false, completion, 5000, false, true) == Work::Wait);
    assert(pacing.next(completion + 5000, false, false, false, completion, 5000, false, true) == Work::Wait);
    // One explicit rotation fallback can request status before the periodic
    // deadline but must still honor failure backoff.
    assert(pacing.next(completion + 5000, false, true, true, completion, 5000, true, true) == Work::Status);
  }
  pacing.statusCompleted(UINT32_MAX - 1000);
  assert(pacing.next(1000, false, true, false, 0, 5000, false, true) == Work::Snapshot);
  assert(pacing.next(30000, false, true, false, 0, 5000, false, true) == Work::Status);
  HttpsExchange exchange;
  assert(exchange.idle() && exchange.result() == nullptr && exchange.request() == nullptr);
  std::string oversize(HttpsExchange::kBodyLimit + 1, 'x');
  assert(!exchange.submit("https://example", oversize.c_str(), "bearer", "id", "secret"));
  assert(exchange.idle());
  const std::string longUrl(513, 'x'), longBearer(2049, 'x'), longId(257, 'x'), longSecret(513, 'x');
  assert(!exchange.submit(longUrl.c_str(), "{}", "bearer", "id", "secret"));
  assert(!exchange.submit("https://example", "{}", longBearer.c_str(), "id", "secret"));
  assert(!exchange.submit("https://example", "{}", "bearer", longId.c_str(), "secret"));
  assert(!exchange.submit("https://example", "{}", "bearer", "id", longSecret.c_str()));
  assert(exchange.idle());
  assert(!exchange.submit(nullptr, "{}", "bearer", "id", "secret"));
  std::string payload = "original snapshot";
  assert(exchange.submit("https://example", payload.c_str(), "bearer", "id", "secret"));
  payload = "new snapshot";
  assert(std::string(exchange.request()->payload) == "original snapshot");
  assert(!exchange.submit("https://other", "{}", "other bearer", "id", "secret"));
  assert(exchange.result() == nullptr);

  // A deliberately held worker cannot block producer polling/submission.
  // Simulate 400 sampling deadlines during a 4s network timeout without
  // sleeping in the host suite; release only after every deadline has run.
  std::atomic<bool> allowCompletion{false};
  std::thread worker([&] {
    assert(exchange.request() != nullptr);
    while (!allowCompletion.load()) std::this_thread::yield();
    auto &result = exchange.workerResult();
    result.status = 202;
    std::strcpy(result.body, "{\"accepted\":true}");
    exchange.complete();
  });
  for (int sample = 0; sample < 400; ++sample) {
    assert(exchange.result() == nullptr);
    assert(!exchange.idle());
    assert(!exchange.submit("https://example", "new", "bearer", "id", "secret"));
  }
  allowCompletion = true;
  worker.join();
  assert(exchange.result()->status == 202);
  assert(exchange.request() == nullptr);
  assert(!exchange.submit("https://example", "new", "bearer", "id", "secret"));
  exchange.release();
  assert(exchange.idle() && exchange.result() == nullptr);
  assert(exchange.submit("https://example", "new", "rotated bearer", "id", "secret"));
  assert(std::string(exchange.request()->bearer) == "rotated bearer");
  exchange.workerResult().status = -1;
  exchange.complete();
  assert(exchange.result()->status == -1);
  exchange.release();

  assert(QueueAge::epoch("2024-01-01T00:00:00Z") == 1704067200);
  assert(QueueAge::epoch("2024-02-29T12:34:56Z") > 0);
  for (const auto *invalid : {"2025-02-29T12:34:56Z", "2024-04-31T00:00:00Z",
                            "2024-01-01T24:00:00Z", "2024-01-01T00:00:60Z",
                            "2024-01-01 00:00:00Z", "2024-01-01T00:00:00+08:00", "uptime:100"}) {
    assert(QueueAge::epoch(invalid) == 0);
  }
  uint32_t age = 999;
  assert(!QueueAge::age(0, 1704067200, age));
  assert(!QueueAge::age(1704067200, 1704067199, age));
  assert(QueueAge::age(1704067200, 1704067200, age) && age == 0);
  assert(QueueAge::age(1704067200, 1704070800, age) && age == 3600);
  std::cout << "HTTPS exchange and queue age tests passed\n";
}
