// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/common/content_export.h"

namespace content {

class IndexedDBCallbacks;
class IndexedDBDatabaseCallbacks;

struct CONTENT_EXPORT IndexedDBPendingConnection {
  IndexedDBPendingConnection(
      scoped_refptr<IndexedDBCallbacks> callbacks_in,
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks_in,
      int child_process_id_in,
      int64_t transaction_id_in,
      int64_t version_in);
  IndexedDBPendingConnection(const IndexedDBPendingConnection& other);
  ~IndexedDBPendingConnection();
  scoped_refptr<IndexedDBCallbacks> callbacks;
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks;
  int child_process_id;
  int64_t transaction_id;
  int64_t version;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_
