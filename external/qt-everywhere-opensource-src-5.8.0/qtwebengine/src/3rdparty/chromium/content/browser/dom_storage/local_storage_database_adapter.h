// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_DATABASE_ADAPTER_H_
#define CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_DATABASE_ADAPTER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/browser/dom_storage/dom_storage_database_adapter.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
}

namespace content {

class DOMStorageDatabase;

class CONTENT_EXPORT LocalStorageDatabaseAdapter :
      public DOMStorageDatabaseAdapter {
 public:
  explicit LocalStorageDatabaseAdapter(const base::FilePath& path);
  ~LocalStorageDatabaseAdapter() override;
  void ReadAllValues(DOMStorageValuesMap* result) override;
  bool CommitChanges(bool clear_all_first,
                     const DOMStorageValuesMap& changes) override;
  void DeleteFiles() override;
  void Reset() override;

 protected:
  // Constructor that uses an in-memory sqlite database, for testing.
  LocalStorageDatabaseAdapter();

 private:
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, BackingDatabaseOpened);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, CommitChangesAtShutdown);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, CommitTasks);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, DeleteOrigin);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, PurgeMemory);

  std::unique_ptr<DOMStorageDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageDatabaseAdapter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_DATABASE_ADAPTER_H_
