#include "OwnerReset.h"
#include "OwnerResetNvs.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
struct Fake {
  esp_err_t openError = ESP_OK, readError = ESP_OK;
  esp_err_t setError = ESP_OK, commitError = ESP_OK;
  bool open = false, exists = true, staged = false;
  bool persistDespiteCommitError = false;
  uint8_t durable = 1, next = 0;
  nvs_open_mode_t mode = NVS_READONLY;
  std::vector<char> calls;
} fake;
void fresh() { fake = Fake{}; }
void handle(nvs_handle_t value) { assert(fake.open && value == 7); }
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *out) {
  assert(std::strcmp(name, "owner_reset") == 0);
  assert(!fake.open);
  fake.calls.push_back('o');
  if (fake.openError != ESP_OK) return fake.openError;
  fake.open = true; fake.mode = mode; *out = 7;
  return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *value) {
  handle(h); assert(std::strcmp(key, "pending") == 0);
  fake.calls.push_back('g');
  if (fake.readError != ESP_OK) return fake.readError;
  if (!fake.exists) return ESP_ERR_NVS_NOT_FOUND;
  *value = fake.durable;
  return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t value) {
  handle(h); assert(fake.mode == NVS_READWRITE);
  assert(std::strcmp(key, "pending") == 0 && value <= 1);
  fake.calls.push_back('s');
  if (fake.setError != ESP_OK) return fake.setError;
  fake.next = value; fake.staged = true;
  return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
  handle(h); assert(fake.staged);
  fake.calls.push_back('c');
  if (fake.commitError == ESP_OK || fake.persistDespiteCommitError) {
    fake.durable = fake.next; fake.exists = true;
  }
  return fake.commitError;
}
void nvs_close(nvs_handle_t h) {
  handle(h); fake.calls.push_back('x'); fake.open = false; fake.staged = false;
}

int main() {
  OwnerReset::NvsStore store;
  for (uint8_t value : {0, 1}) {
    fresh(); fake.durable = value; bool pending = value == 0;
    assert(store.getPending(pending) && pending == (value == 1));
    assert((fake.calls == std::vector<char>{'o','g','x'}));
  }
  fresh(); fake.openError = ESP_ERR_NVS_NOT_FOUND; bool pending = true;
  assert(store.getPending(pending) && !pending);
  assert((fake.calls == std::vector<char>{'o'}));
  fresh(); fake.exists = false; pending = true;
  assert(store.getPending(pending) && !pending && !fake.open);
  for (esp_err_t error : {ESP_FAIL, ESP_ERR_NVS_NOT_INITIALIZED, ESP_ERR_NVS_INVALID_HANDLE}) {
    fresh(); fake.openError = error; pending = true;
    assert(!store.getPending(pending) && pending && !fake.open);
    assert(!store.markPending(true));
  }
  for (esp_err_t error : {ESP_FAIL, ESP_ERR_NVS_TYPE_MISMATCH, ESP_ERR_NVS_INVALID_HANDLE}) {
    fresh(); fake.readError = error; pending = true;
    assert(!store.getPending(pending) && pending && !fake.open);
    OwnerReset::Coordinator reset; unsigned clears = 0;
    assert(reset.resume(store, [&] { ++clears; return true; }) == OwnerReset::Result::StorageError);
    assert(!reset.networkAllowed() && clears == 0 && fake.durable == 1);
  }
  for (uint8_t value : {2, 127, 255}) {
    fresh(); fake.durable = value; pending = true;
    assert(!store.getPending(pending) && pending && !fake.open);
  }
  for (bool value : {false, true}) {
    fresh(); fake.durable = value ? 0 : 1;
    assert(store.markPending(value));
    assert((fake.calls == std::vector<char>{'o','s','c','x'}));
    assert(store.getPending(pending) && pending == value);
  }
  fresh(); fake.setError = ESP_FAIL;
  assert(!store.markPending(false) && fake.durable == 1);
  assert((fake.calls == std::vector<char>{'o','s','x'})); // No commit after failed set.
  for (bool ambiguous : {false, true}) {
    fresh(); fake.commitError = ESP_FAIL; fake.persistDespiteCommitError = ambiguous;
    assert(!store.markPending(false) && !fake.open);
    assert((fake.calls == std::vector<char>{'o','s','c','x'}));
    // A failed commit is an error even if storage actually changed.
    fresh(); fake.durable = 0; fake.commitError = ESP_FAIL;
    fake.persistDespiteCommitError = ambiguous;
    OwnerReset::Coordinator reset; unsigned clears = 0;
    assert(reset.request(store, [&] { ++clears; return true; }) == OwnerReset::Result::StorageError);
    assert(!reset.networkAllowed() && clears == 0);
  }
  fresh(); fake.durable = 0;
  OwnerReset::Coordinator reset; unsigned clears = 0;
  assert(reset.request(store, [&] {
    assert(fake.durable == 1 && !reset.networkAllowed()); ++clears; return false;
  }) == OwnerReset::Result::Pending);
  assert(!reset.networkAllowed() && fake.durable == 1);
  OwnerReset::Coordinator reboot;
  assert(reboot.resume(store, [&] { ++clears; return true; }) == OwnerReset::Result::Completed);
  assert(reboot.networkAllowed() && clears == 2 && fake.durable == 0);
  std::cout << "Owner reset NVS adapter tests passed\n";
}
