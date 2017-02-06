// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_pending_connection.h"

namespace content {

IndexedDBPendingConnection::IndexedDBPendingConnection(
    scoped_refptr<IndexedDBCallbacks> callbacks_in,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks_in,
    int child_process_id_in,
    int64_t transaction_id_in,
    int64_t version_in)
    : callbacks(callbacks_in),
      database_callbacks(database_callbacks_in),
      child_process_id(child_process_id_in),
      transaction_id(transaction_id_in),
      version(version_in) {}

IndexedDBPendingConnection::IndexedDBPendingConnection(
    const IndexedDBPendingConnection& other) = default;

IndexedDBPendingConnection::~IndexedDBPendingConnection() {}

}  // namespace content
