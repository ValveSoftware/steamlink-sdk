// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_observer.h"

namespace content {

class CONTENT_EXPORT IndexedDBConnection {
 public:
  IndexedDBConnection(scoped_refptr<IndexedDBDatabase> db,
                      scoped_refptr<IndexedDBDatabaseCallbacks> callbacks);
  virtual ~IndexedDBConnection();

  // These methods are virtual to allow subclassing in unit tests.
  virtual void ForceClose();
  virtual void Close();
  virtual bool IsConnected();

  void VersionChangeIgnored();

  virtual void ActivatePendingObservers(
      std::vector<std::unique_ptr<IndexedDBObserver>> pending_observers);
  // Removes observer listed in |remove_observer_ids| from active_observer of
  // connection or pending_observer of transactions associated with this
  // connection.
  virtual void RemoveObservers(const std::vector<int32_t>& remove_observer_ids);

  int32_t id() const { return id_; }

  IndexedDBDatabase* database() const { return database_.get(); }
  IndexedDBDatabaseCallbacks* callbacks() const { return callbacks_.get(); }
  const std::vector<std::unique_ptr<IndexedDBObserver>>& active_observers()
      const {
    return active_observers_;
  }
  base::WeakPtr<IndexedDBConnection> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const int32_t id_;

  // NULL in some unit tests, and after the connection is closed.
  scoped_refptr<IndexedDBDatabase> database_;

  // The callbacks_ member is cleared when the connection is closed.
  // May be NULL in unit tests.
  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks_;
  std::vector<std::unique_ptr<IndexedDBObserver>> active_observers_;
  base::WeakPtrFactory<IndexedDBConnection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
