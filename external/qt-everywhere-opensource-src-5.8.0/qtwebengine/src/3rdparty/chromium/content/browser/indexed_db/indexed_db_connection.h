// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"

namespace content {
class IndexedDBCallbacks;
class IndexedDBDatabaseError;

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

  IndexedDBDatabase* database() const { return database_.get(); }
  IndexedDBDatabaseCallbacks* callbacks() const { return callbacks_.get(); }

 private:
  // NULL in some unit tests, and after the connection is closed.
  scoped_refptr<IndexedDBDatabase> database_;

  // The callbacks_ member is cleared when the connection is closed.
  // May be NULL in unit tests.
  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks_;

  base::WeakPtrFactory<IndexedDBConnection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
