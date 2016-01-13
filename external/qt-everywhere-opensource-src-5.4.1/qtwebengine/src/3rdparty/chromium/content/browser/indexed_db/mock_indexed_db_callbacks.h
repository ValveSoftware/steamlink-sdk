// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_CALLBACKS_H_

#include <vector>

#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"

namespace content {

class MockIndexedDBCallbacks : public IndexedDBCallbacks {
 public:
  MockIndexedDBCallbacks();
  explicit MockIndexedDBCallbacks(bool expect_connection);

  virtual void OnSuccess() OVERRIDE;
  virtual void OnSuccess(int64 result) OVERRIDE;
  virtual void OnSuccess(const std::vector<base::string16>& result) OVERRIDE;
  virtual void OnSuccess(const IndexedDBKey& key) OVERRIDE;
  virtual void OnSuccess(scoped_ptr<IndexedDBConnection> connection,
                         const IndexedDBDatabaseMetadata& metadata) OVERRIDE;
  IndexedDBConnection* connection() { return connection_.get(); }

 protected:
  virtual ~MockIndexedDBCallbacks();

  scoped_ptr<IndexedDBConnection> connection_;

 private:
  bool expect_connection_;

  DISALLOW_COPY_AND_ASSIGN(MockIndexedDBCallbacks);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_CALLBACKS_H_
