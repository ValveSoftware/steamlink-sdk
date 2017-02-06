// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {
class IndexedDBDatabaseError;
class IndexedDBDispatcherHost;

class CONTENT_EXPORT IndexedDBDatabaseCallbacks
    : public base::RefCounted<IndexedDBDatabaseCallbacks> {
 public:
  IndexedDBDatabaseCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                             int ipc_thread_id,
                             int ipc_database_callbacks_id);

  virtual void OnForcedClose();
  virtual void OnVersionChange(int64_t old_version, int64_t new_version);

  virtual void OnAbort(int64_t host_transaction_id,
                       const IndexedDBDatabaseError& error);
  virtual void OnComplete(int64_t host_transaction_id);

 protected:
  virtual ~IndexedDBDatabaseCallbacks();

 private:
  friend class base::RefCounted<IndexedDBDatabaseCallbacks>;

  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int ipc_thread_id_;
  int ipc_database_callbacks_id_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabaseCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_
