// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/common/indexed_db/indexed_db_key_range.h"

namespace content {

class IndexedDBTransaction;

class CONTENT_EXPORT IndexedDBCursor
    : NON_EXPORTED_BASE(public base::RefCounted<IndexedDBCursor>) {
 public:
  IndexedDBCursor(scoped_ptr<IndexedDBBackingStore::Cursor> cursor,
                  indexed_db::CursorType cursor_type,
                  IndexedDBDatabase::TaskType task_type,
                  IndexedDBTransaction* transaction);

  void Advance(uint32 count, scoped_refptr<IndexedDBCallbacks> callbacks);
  void Continue(scoped_ptr<IndexedDBKey> key,
                scoped_ptr<IndexedDBKey> primary_key,
                scoped_refptr<IndexedDBCallbacks> callbacks);
  void PrefetchContinue(int number_to_fetch,
                        scoped_refptr<IndexedDBCallbacks> callbacks);
  leveldb::Status PrefetchReset(int used_prefetches, int unused_prefetches);

  const IndexedDBKey& key() const { return cursor_->key(); }
  const IndexedDBKey& primary_key() const { return cursor_->primary_key(); }
  IndexedDBValue* Value() const {
    return (cursor_type_ == indexed_db::CURSOR_KEY_ONLY) ? NULL
                                                         : cursor_->value();
  }
  void Close();

  void CursorIterationOperation(scoped_ptr<IndexedDBKey> key,
                                scoped_ptr<IndexedDBKey> primary_key,
                                scoped_refptr<IndexedDBCallbacks> callbacks,
                                IndexedDBTransaction* transaction);
  void CursorAdvanceOperation(uint32 count,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
                              IndexedDBTransaction* transaction);
  void CursorPrefetchIterationOperation(
      int number_to_fetch,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);

 private:
  friend class base::RefCounted<IndexedDBCursor>;

  ~IndexedDBCursor();

  IndexedDBDatabase::TaskType task_type_;
  indexed_db::CursorType cursor_type_;
  const scoped_refptr<IndexedDBTransaction> transaction_;

  // Must be destroyed before transaction_.
  scoped_ptr<IndexedDBBackingStore::Cursor> cursor_;
  // Must be destroyed before transaction_.
  scoped_ptr<IndexedDBBackingStore::Cursor> saved_cursor_;

  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBCursor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CURSOR_H_
