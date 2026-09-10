#include "OwnerReset.h"
#include <array>
#include <cassert>
#include <iostream>

namespace {
struct Store {
  bool pending = false;
  bool failMark = false;
  bool failUnmark = false;
  bool ignoreWrite = false;
  unsigned failReadAt = 0;
  unsigned reads = 0;
  unsigned writes = 0;
  bool getPending(bool &value) {
    if (++reads == failReadAt) return false;
    value = pending;
    return true;
  }
  bool markPending(bool value) {
    ++writes;
    if ((value && failMark) || (!value && failUnmark)) return false;
    if (!ignoreWrite) pending = value;
    return true;
  }
};

struct Owners {
  Store &store;
  std::array<bool, 4> credentials{{true, true, true, true}};
  std::array<unsigned, 4> calls{};
  int failing = -1;
  // Identity is deliberately separate from each owner record.
  unsigned installationIdentity = 42;
  bool clear(unsigned i) {
    assert(store.pending); // Every callback requires an already durable latch.
    ++calls[i];
    if (static_cast<int>(i) == failing) return false;
    credentials[i] = false;
    return true;
  }
};

template<class Operation>
OwnerReset::Result withOwners(Owners &owners, Operation operation) {
  return operation([&] { return owners.clear(0); }, [&] { return owners.clear(1); },
                   [&] { return owners.clear(2); }, [&] { return owners.clear(3); });
}
}

int main() {
  using OwnerReset::Coordinator;
  using OwnerReset::Result;
  {
    Store store; Coordinator reset; unsigned calls = 0;
    assert(!reset.networkAllowed());
    assert(reset.resume(store, [&] { ++calls; return true; }) == Result::NoResetNeeded);
    assert(reset.networkAllowed() && calls == 0 && store.writes == 0);
  }
  for (int failure = -1; failure < 4; ++failure) {
    Store store; Owners owners{store}; owners.failing = failure; Coordinator reset;
    const auto outcome = withOwners(owners, [&](auto... clear) { return reset.request(store, clear...); });
    for (unsigned calls : owners.calls) assert(calls == 1); // No short circuit.
    assert(owners.installationIdentity == 42);
    assert(outcome == (failure < 0 ? Result::Completed : Result::Pending));
    assert(store.pending == (failure >= 0));
    assert(reset.networkAllowed() == (failure < 0));
    if (failure >= 0) {
      // A reboot cannot restore access while any retained owner remains.
      Coordinator reboot;
      assert(withOwners(owners, [&](auto... clear) { return reboot.resume(store, clear...); }) == Result::Pending);
      assert(!reboot.networkAllowed() && store.pending);
      owners.failing = -1;
      assert(withOwners(owners, [&](auto... clear) { return reboot.resume(store, clear...); }) == Result::Completed);
      assert(reboot.networkAllowed() && !store.pending);
      for (bool credential : owners.credentials) assert(!credential);
      assert(owners.installationIdentity == 42);
    }
  }
  {
    Store store; store.failMark = true; Coordinator reset; unsigned calls = 0;
    assert(reset.request(store, [&] { ++calls; return true; }) == Result::StorageError);
    assert(!reset.networkAllowed() && calls == 0 && !store.pending);
    assert(reset.resume(store, [&] { ++calls; return true; }) == Result::StorageError);
    assert(!reset.networkAllowed() && calls == 0);
    // An initial write failure cannot promise a latch survived a power cut.
    // Integration must keep this boot offline/retry instead of blind restart.
  }
  {
    Store store; store.ignoreWrite = true; Coordinator reset; unsigned calls = 0;
    assert(reset.request(store, [&] { ++calls; return true; }) == Result::StorageError);
    assert(!reset.networkAllowed() && calls == 0); // False write acknowledgement.
  }
  for (unsigned failedRead : {1U, 2U}) {
    Store store; store.failReadAt = failedRead; Coordinator reset; unsigned calls = 0;
    assert(reset.request(store, [&] { ++calls; return true; }) == Result::StorageError);
    assert(!reset.networkAllowed());
    assert(calls == (failedRead == 1 ? 0U : 1U));
  }
  {
    Store store; store.pending = true; store.failReadAt = 1; Coordinator reset; unsigned calls = 0;
    assert(reset.resume(store, [&] { ++calls; return true; }) == Result::StorageError);
    assert(!reset.networkAllowed() && calls == 0 && store.pending);
  }
  {
    Store store; store.failUnmark = true; Coordinator reset; unsigned calls = 0;
    auto clear = [&] { ++calls; return true; };
    assert(reset.request(store, clear) == Result::StorageError);
    assert(!reset.networkAllowed() && store.pending && calls == 1);
    Coordinator reboot;
    assert(reboot.resume(store, clear) == Result::StorageError);
    assert(!reboot.networkAllowed() && store.pending && calls == 2);
    store.failUnmark = false;
    assert(reboot.resume(store, clear) == Result::Completed);
    assert(reboot.networkAllowed() && !store.pending && calls == 3);
  }
  {
    // Simulated power loss after each partial owner clear, including before
    // the first clear and after the last clear but before clearing the latch.
    for (unsigned completed = 0; completed <= 4; ++completed) {
      Store store; store.pending = true; Owners owners{store};
      for (unsigned i = 0; i < completed; ++i) owners.credentials[i] = false;
      Coordinator reboot;
      assert(!reboot.networkAllowed());
      assert(withOwners(owners, [&](auto... clear) { return reboot.resume(store, clear...); }) == Result::Completed);
      for (bool credential : owners.credentials) assert(!credential);
      assert(reboot.networkAllowed() && !store.pending && owners.installationIdentity == 42);
    }
  }
  {
    Store store; Coordinator reset;
    assert(reset.resume(store, [] { return true; }) == Result::NoResetNeeded);
    store.failMark = true;
    assert(reset.request(store, [] { return true; }) == Result::StorageError);
    assert(!reset.networkAllowed()); // Never retain a prior allowed result.
  }
  std::cout << "Owner reset coordinator tests passed\n";
}
