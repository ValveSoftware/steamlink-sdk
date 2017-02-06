// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_DATABASE_ADAPTER_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_DATABASE_ADAPTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/dom_storage/dom_storage_database_adapter.h"
#include "url/gurl.h"

namespace content {

class SessionStorageDatabase;

class SessionStorageDatabaseAdapter : public DOMStorageDatabaseAdapter {
 public:
  SessionStorageDatabaseAdapter(SessionStorageDatabase* db,
                                const std::string& permanent_namespace_id,
                                const GURL& origin);
  ~SessionStorageDatabaseAdapter() override;
  void ReadAllValues(DOMStorageValuesMap* result) override;
  bool CommitChanges(bool clear_all_first,
                     const DOMStorageValuesMap& changes) override;

 private:
  scoped_refptr<SessionStorageDatabase> db_;
  std::string permanent_namespace_id_;
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageDatabaseAdapter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_DATABASE_ADAPTER_H_
