// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
#define CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace content {

// This is a ref-counted class that represents a SessionStorageNamespace.
// On destruction it ensures that the storage namespace is destroyed.
class SessionStorageNamespace
    : public base::RefCountedThreadSafe<SessionStorageNamespace> {
 public:
  // Returns the ID of the |SessionStorageNamespace|. The ID is unique among all
  // SessionStorageNamespace objects, but not unique across browser runs.
  virtual int64 id() const = 0;

  // Returns the persistent ID for the |SessionStorageNamespace|. The ID is
  // unique across browser runs.
  virtual const std::string& persistent_id() const = 0;

  // For marking that the sessionStorage will be needed or won't be needed by
  // session restore.
  virtual void SetShouldPersist(bool should_persist) = 0;

  virtual bool should_persist() const = 0;

  // SessionStorageNamespaces can be merged. These merges happen based on
  // a transaction log of operations on the session storage namespace since
  // this function has been called. Transaction logging will be restricted
  // to the processes indicated.
  virtual void AddTransactionLogProcessId(int process_id) = 0;

  // When transaction logging for a process is no longer required, the log
  // can be removed to save space.
  virtual void RemoveTransactionLogProcessId(int process_id) = 0;

  // Creates a new session storage namespace which is an alias of the current
  // instance.
  virtual SessionStorageNamespace* CreateAlias() = 0;

  enum MergeResult {
    MERGE_RESULT_NAMESPACE_NOT_FOUND,
    MERGE_RESULT_NAMESPACE_NOT_ALIAS,
    MERGE_RESULT_NOT_LOGGING,
    MERGE_RESULT_NO_TRANSACTIONS,
    MERGE_RESULT_TOO_MANY_TRANSACTIONS,
    MERGE_RESULT_NOT_MERGEABLE,
    MERGE_RESULT_MERGEABLE,
    MERGE_RESULT_MAX_VALUE
  };

  typedef base::Callback<void(MergeResult)> MergeResultCallback;

  // Determines whether the transaction log for the process specified can
  // be merged into the other session storage namespace supplied.
  // If actually_merge is set to true, the merge will actually be performed,
  // if possible, and the result of the merge will be returned.
  // If actually_merge is set to false, the result of whether a merge would be
  // possible is returned.
  virtual void Merge(bool actually_merge,
                     int process_id,
                     SessionStorageNamespace* other,
                     const MergeResultCallback& callback) = 0;

  // Indicates whether this SessionStorageNamespace is an alias of |other|,
  // i.e. whether they point to the same underlying data.
  virtual bool IsAliasOf(SessionStorageNamespace* other) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SessionStorageNamespace>;
  virtual ~SessionStorageNamespace() {}
};

// Used to store mappings of StoragePartition id to SessionStorageNamespace.
typedef std::map<std::string, scoped_refptr<SessionStorageNamespace> >
    SessionStorageNamespaceMap;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_NAMESPACE_H_
