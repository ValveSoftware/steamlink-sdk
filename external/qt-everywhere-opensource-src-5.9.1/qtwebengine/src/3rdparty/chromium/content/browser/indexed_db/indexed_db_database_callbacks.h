// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db.mojom.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class IndexedDBDatabaseError;
class IndexedDBDispatcherHost;
class IndexedDBObserverChanges;

class CONTENT_EXPORT IndexedDBDatabaseCallbacks
    : public base::RefCounted<IndexedDBDatabaseCallbacks> {
 public:
  IndexedDBDatabaseCallbacks(
      scoped_refptr<IndexedDBDispatcherHost> dispatcher_host,
      int32_t ipc_thread_id,
      ::indexed_db::mojom::DatabaseCallbacksAssociatedPtrInfo callbacks_info);

  virtual void OnForcedClose();
  virtual void OnVersionChange(int64_t old_version, int64_t new_version);

  virtual void OnAbort(int64_t host_transaction_id,
                       const IndexedDBDatabaseError& error);
  virtual void OnComplete(int64_t host_transaction_id);
  virtual void OnDatabaseChange(
      std::unique_ptr<IndexedDBObserverChanges> changes);

 protected:
  virtual ~IndexedDBDatabaseCallbacks();

 private:
  friend class base::RefCounted<IndexedDBDatabaseCallbacks>;

  class IOThreadHelper;

  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int32_t ipc_thread_id_;
  std::unique_ptr<IOThreadHelper, BrowserThread::DeleteOnIOThread> io_helper_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabaseCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_CALLBACKS_H_
