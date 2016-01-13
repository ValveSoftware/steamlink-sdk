// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_

#include "base/basictypes.h"
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
      int64 transaction_id_in,
      int64 version_in);
  ~IndexedDBPendingConnection();
  scoped_refptr<IndexedDBCallbacks> callbacks;
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks;
  int child_process_id;
  int64 transaction_id;
  int64 version;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_PENDING_CONNECTION_H_
