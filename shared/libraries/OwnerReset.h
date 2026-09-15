#pragma once

namespace OwnerReset {

enum class Result { Unchecked, NoResetNeeded, Completed, Pending, StorageError };

// Store contract:
//   bool getPending(bool &value): false on any storage/read/integrity error.
//   bool markPending(bool value): true only after a durable write.
// A genuinely absent latch on first boot may read as false; an unreadable store
// must not be treated as absent. Keep the latch outside all cleared namespaces.
// Clear callbacks must be idempotent, return true only after durable removal of
// owner credentials/proofs, and preserve hardware/factory/installation identity.
class Coordinator {
 public:
  Result result() const { return result_; }
  bool networkAllowed() const {
    return result_ == Result::NoResetNeeded || result_ == Result::Completed;
  }

  // Call before initializing credential consumers or any network transport.
  template<class Store, class... Clear>
  Result resume(Store &store, Clear &&...clear) {
    static_assert(sizeof...(Clear) > 0, "Owner reset needs credential clear callbacks");
    result_ = Result::Unchecked;
    bool pending = true;
    if (!store.getPending(pending)) return result_ = Result::StorageError;
    if (!pending) {
      // A failed initial write must not be bypassed by calling resume again
      // within this boot. Retry establishing the requested durable boundary.
      if (requested_) return request(store, clear...);
      return result_ = Result::NoResetNeeded;
    }
    return finish(store, clear...);
  }

  // Request only while networking is stopped. If persisting the initial latch
  // fails, keep this boot offline and retry; do not restart and assume that a
  // failed write persisted the user's reset request across power loss.
  template<class Store, class... Clear>
  Result request(Store &store, Clear &&...clear) {
    static_assert(sizeof...(Clear) > 0, "Owner reset needs credential clear callbacks");
    result_ = Result::Pending;
    requested_ = true;
    if (!store.markPending(true)) return result_ = Result::StorageError;
    bool pending = false;
    if (!store.getPending(pending) || !pending) return result_ = Result::StorageError;
    return finish(store, clear...);
  }

 private:
  template<class Store, class... Clear>
  Result finish(Store &store, Clear &...clear) {
    result_ = Result::Pending;
    bool cleared = true;
    // Attempt every store even when an earlier clear failed. The durable latch
    // stays set until all succeed; a new boot repeats the complete operation.
    ((cleared = static_cast<bool>(clear()) && cleared), ...);
    if (!cleared) return result_;
    if (!store.markPending(false)) return result_ = Result::StorageError;
    bool pending = true;
    if (!store.getPending(pending) || pending) return result_ = Result::StorageError;
    requested_ = false;
    return result_ = Result::Completed;
  }

  Result result_ = Result::Unchecked;
  bool requested_ = false;
};

}  // namespace OwnerReset
