// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockIndexedDBDatabaseCallbacks::MockIndexedDBDatabaseCallbacks()
    : IndexedDBDatabaseCallbacks(NULL, 0, 0),
      abort_called_(false),
      forced_close_called_(false) {}

void MockIndexedDBDatabaseCallbacks::OnForcedClose() {
  forced_close_called_ = true;
}

void MockIndexedDBDatabaseCallbacks::OnAbort(
    int64 transaction_id,
    const IndexedDBDatabaseError& error) {
  abort_called_ = true;
}

}  // namespace content
