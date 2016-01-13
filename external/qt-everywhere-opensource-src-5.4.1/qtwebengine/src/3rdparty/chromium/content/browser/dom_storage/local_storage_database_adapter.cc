// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/local_storage_database_adapter.h"

#include "base/file_util.h"
#include "content/browser/dom_storage/dom_storage_database.h"

namespace content {

LocalStorageDatabaseAdapter::LocalStorageDatabaseAdapter(
    const base::FilePath& path)
    : db_(new DOMStorageDatabase(path)) {
}

LocalStorageDatabaseAdapter::~LocalStorageDatabaseAdapter() { }

void LocalStorageDatabaseAdapter::ReadAllValues(DOMStorageValuesMap* result) {
  db_->ReadAllValues(result);
}

bool LocalStorageDatabaseAdapter::CommitChanges(
    bool clear_all_first, const DOMStorageValuesMap& changes) {
  return db_->CommitChanges(clear_all_first, changes);
}

void LocalStorageDatabaseAdapter::DeleteFiles() {
  sql::Connection::Delete(db_->file_path());
}

void LocalStorageDatabaseAdapter::Reset() {
  db_.reset(new DOMStorageDatabase(db_->file_path()));
}

LocalStorageDatabaseAdapter::LocalStorageDatabaseAdapter()
    : db_(new DOMStorageDatabase()) {
}

}  // namespace content
