// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_cursor.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"

namespace content {

IndexedDBCursor::IndexedDBCursor(
    std::unique_ptr<IndexedDBBackingStore::Cursor> cursor,
    indexed_db::CursorType cursor_type,
    blink::WebIDBTaskType task_type,
    IndexedDBTransaction* transaction)
    : task_type_(task_type),
      cursor_type_(cursor_type),
      transaction_(transaction),
      cursor_(std::move(cursor)),
      closed_(false) {
  transaction_->RegisterOpenCursor(this);
}

IndexedDBCursor::~IndexedDBCursor() {
  transaction_->UnregisterOpenCursor(this);
}

void IndexedDBCursor::Continue(std::unique_ptr<IndexedDBKey> key,
                               std::unique_ptr<IndexedDBKey> primary_key,
                               scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::Continue");

  transaction_->ScheduleTask(
      task_type_,
      base::Bind(&IndexedDBCursor::CursorIterationOperation,
                 this,
                 base::Passed(&key),
                 base::Passed(&primary_key),
                 callbacks));
}

void IndexedDBCursor::Advance(uint32_t count,
                              scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::Advance");

  transaction_->ScheduleTask(
      task_type_,
      base::Bind(
          &IndexedDBCursor::CursorAdvanceOperation, this, count, callbacks));
}

void IndexedDBCursor::CursorAdvanceOperation(
    uint32_t count,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("IndexedDBCursor::CursorAdvanceOperation");
  leveldb::Status s;
  // TODO(cmumford): Handle this error (crbug.com/363397). Although this will
  //                 properly fail, caller will not know why, and any corruption
  //                 will be ignored.
  if (!cursor_ || !cursor_->Advance(count, &s)) {
    cursor_.reset();
    callbacks->OnSuccess(nullptr);
    return;
  }

  callbacks->OnSuccess(key(), primary_key(), Value());
}

void IndexedDBCursor::CursorIterationOperation(
    std::unique_ptr<IndexedDBKey> key,
    std::unique_ptr<IndexedDBKey> primary_key,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("IndexedDBCursor::CursorIterationOperation");
  leveldb::Status s;
  // TODO(cmumford): Handle this error (crbug.com/363397). Although this will
  //                 properly fail, caller will not know why, and any corruption
  //                 will be ignored.
  if (!cursor_ || !cursor_->Continue(key.get(),
                                     primary_key.get(),
                                     IndexedDBBackingStore::Cursor::SEEK,
                                     &s) || !s.ok()) {
    cursor_.reset();
    callbacks->OnSuccess(nullptr);
    return;
  }

  callbacks->OnSuccess(this->key(), this->primary_key(), Value());
}

void IndexedDBCursor::PrefetchContinue(
    int number_to_fetch,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  IDB_TRACE("IndexedDBCursor::PrefetchContinue");

  transaction_->ScheduleTask(
      task_type_,
      base::Bind(&IndexedDBCursor::CursorPrefetchIterationOperation,
                 this,
                 number_to_fetch,
                 callbacks));
}

void IndexedDBCursor::CursorPrefetchIterationOperation(
    int number_to_fetch,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* /*transaction*/) {
  IDB_TRACE("IndexedDBCursor::CursorPrefetchIterationOperation");

  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBKey> found_primary_keys;
  std::vector<IndexedDBValue> found_values;

  saved_cursor_.reset();
  // TODO(cmumford): Use IPC::Channel::kMaximumMessageSize
  const size_t max_size_estimate = 10 * 1024 * 1024;
  size_t size_estimate = 0;
  leveldb::Status s;

  // TODO(cmumford): Handle this error (crbug.com/363397). Although this will
  //                 properly fail, caller will not know why, and any corruption
  //                 will be ignored.
  for (int i = 0; i < number_to_fetch; ++i) {
    if (!cursor_ || !cursor_->Continue(&s)) {
      cursor_.reset();
      break;
    }

    if (i == 0) {
      // First prefetched result is always used, so that's the position
      // a cursor should be reset to if the prefetch is invalidated.
      saved_cursor_.reset(cursor_->Clone());
    }

    found_keys.push_back(cursor_->key());
    found_primary_keys.push_back(cursor_->primary_key());

    switch (cursor_type_) {
      case indexed_db::CURSOR_KEY_ONLY:
        found_values.push_back(IndexedDBValue());
        break;
      case indexed_db::CURSOR_KEY_AND_VALUE: {
        IndexedDBValue value;
        value.swap(*cursor_->value());
        size_estimate += value.SizeEstimate();
        found_values.push_back(value);
        break;
      }
      default:
        NOTREACHED();
    }
    size_estimate += cursor_->key().size_estimate();
    size_estimate += cursor_->primary_key().size_estimate();

    if (size_estimate > max_size_estimate)
      break;
  }

  if (!found_keys.size()) {
    callbacks->OnSuccess(nullptr);
    return;
  }

  callbacks->OnSuccessWithPrefetch(
      found_keys, found_primary_keys, &found_values);
}

leveldb::Status IndexedDBCursor::PrefetchReset(int used_prefetches,
                                               int /* unused_prefetches */) {
  IDB_TRACE("IndexedDBCursor::PrefetchReset");
  cursor_.swap(saved_cursor_);
  saved_cursor_.reset();
  leveldb::Status s;

  if (closed_)
    return s;
  if (cursor_) {
    // First prefetched result is always used.
    DCHECK_GT(used_prefetches, 0);
    for (int i = 0; i < used_prefetches - 1; ++i) {
      bool ok = cursor_->Continue(&s);
      DCHECK(ok);
    }
  }

  return s;
}

void IndexedDBCursor::Close() {
  IDB_TRACE("IndexedDBCursor::Close");
  closed_ = true;
  cursor_.reset();
  saved_cursor_.reset();
}

}  // namespace content
