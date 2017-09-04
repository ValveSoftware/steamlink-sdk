// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection.h"

#include "base/logging.h"

namespace content {

namespace {

static int32_t next_id;

}  // namespace

IndexedDBConnection::IndexedDBConnection(
    scoped_refptr<IndexedDBDatabase> database,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks)
    : id_(next_id++),
      database_(database),
      callbacks_(callbacks),
      weak_factory_(this) {}

IndexedDBConnection::~IndexedDBConnection() {}

void IndexedDBConnection::Close() {
  if (!callbacks_.get())
    return;
  base::WeakPtr<IndexedDBConnection> this_obj = weak_factory_.GetWeakPtr();
  database_->Close(this, false /* forced */);
  if (this_obj) {
    database_ = nullptr;
    callbacks_ = nullptr;
    active_observers_.clear();
  }
}

void IndexedDBConnection::ForceClose() {
  if (!callbacks_.get())
    return;

  // IndexedDBDatabase::Close() can delete this instance.
  base::WeakPtr<IndexedDBConnection> this_obj = weak_factory_.GetWeakPtr();
  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks(callbacks_);
  database_->Close(this, true /* forced */);
  if (this_obj) {
    database_ = nullptr;
    callbacks_ = nullptr;
    active_observers_.clear();
  }
  callbacks->OnForcedClose();
}

void IndexedDBConnection::VersionChangeIgnored() {
  if (!database_.get())
    return;
  database_->VersionChangeIgnored();
}

bool IndexedDBConnection::IsConnected() {
  return database_.get() != NULL;
}

// The observers begin listening to changes only once they are activated.
void IndexedDBConnection::ActivatePendingObservers(
    std::vector<std::unique_ptr<IndexedDBObserver>> pending_observers) {
  for (auto& observer : pending_observers) {
    active_observers_.push_back(std::move(observer));
  }
  pending_observers.clear();
}

void IndexedDBConnection::RemoveObservers(
    const std::vector<int32_t>& observer_ids_to_remove) {
  std::vector<int32_t> pending_observer_ids;
  for (int32_t id_to_remove : observer_ids_to_remove) {
    const auto& it = std::find_if(
        active_observers_.begin(), active_observers_.end(),
        [&id_to_remove](const std::unique_ptr<IndexedDBObserver>& o) {
          return o->id() == id_to_remove;
        });
    if (it != active_observers_.end())
      active_observers_.erase(it);
    else
      pending_observer_ids.push_back(id_to_remove);
  }
  if (!pending_observer_ids.empty())
    database_->RemovePendingObservers(this, pending_observer_ids);
}

}  // namespace content
