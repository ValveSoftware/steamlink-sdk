// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection.h"

namespace content {

IndexedDBConnection::IndexedDBConnection(
    scoped_refptr<IndexedDBDatabase> database,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks)
    : database_(database), callbacks_(callbacks), weak_factory_(this) {}

IndexedDBConnection::~IndexedDBConnection() {}

void IndexedDBConnection::Close() {
  if (!callbacks_.get())
    return;
  base::WeakPtr<IndexedDBConnection> this_obj = weak_factory_.GetWeakPtr();
  database_->Close(this, false /* forced */);
  if (this_obj) {
    database_ = nullptr;
    callbacks_ = nullptr;
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

}  // namespace content
