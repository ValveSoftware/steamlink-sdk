// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ACTIVE_BLOB_REGISTRY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ACTIVE_BLOB_REGISTRY_H_

#include <map>
#include <set>
#include <utility>
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace content {

class IndexedDBBackingStore;

class CONTENT_EXPORT IndexedDBActiveBlobRegistry {
 public:
  explicit IndexedDBActiveBlobRegistry(IndexedDBBackingStore* backing_store);
  ~IndexedDBActiveBlobRegistry();

  // Most methods of this class, and the closure returned by
  // GetAddBlobRefCallback, should only be called on the backing_store's task
  // runner.  The exception is the closure returned by GetFinalReleaseCallback,
  // which just calls ReleaseBlobRefThreadSafe.

  // Use DatabaseMetaDataKey::AllBlobsKey for "the whole database".
  bool MarkDeletedCheckIfUsed(int64 database_id, int64 blob_key);

  webkit_blob::ShareableFileReference::FinalReleaseCallback
      GetFinalReleaseCallback(int64 database_id, int64 blob_key);
  // This closure holds a raw pointer to the IndexedDBActiveBlobRegistry,
  // and may not be called after it is deleted.
  base::Closure GetAddBlobRefCallback(int64 database_id, int64 blob_key);
  // Call this to force the registry to drop its use counts, permitting the
  // factory to drop any blob-related refcount for the backing store.
  // This will also turn any outstanding callbacks into no-ops.
  void ForceShutdown();

 private:
  void AddBlobRef(int64 database_id, int64 blob_key);
  void ReleaseBlobRef(int64 database_id, int64 blob_key);
  static void ReleaseBlobRefThreadSafe(
      scoped_refptr<base::TaskRunner> task_runner,
      base::WeakPtr<IndexedDBActiveBlobRegistry> weak_ptr,
      int64 database_id,
      int64 blob_key,
      const base::FilePath& unused);

  // Maps blob_key -> IsDeleted; if the record's absent, it's not in active use
  // and we don't care if it's deleted.
  typedef std::map<int64, bool> SingleDBMap;
  // Maps DB ID -> SingleDBMap
  typedef std::map<int64, SingleDBMap> AllDBsMap;
  typedef std::set<int64> DeletedDBSet;

  AllDBsMap use_tracker_;
  DeletedDBSet deleted_dbs_;
  // As long as we've got blobs registered in use_tracker_,
  // backing_store_->factory() will keep backing_store_ alive for us.  And
  // backing_store_ owns us, so we'll stay alive as long as we're needed.
  IndexedDBBackingStore* backing_store_;
  base::WeakPtrFactory<IndexedDBActiveBlobRegistry> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBActiveBlobRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ACTIVE_BLOB_REGISTRY_H_
