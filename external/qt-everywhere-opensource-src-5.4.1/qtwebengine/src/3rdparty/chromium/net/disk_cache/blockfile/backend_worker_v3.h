// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_BACKEND_WORKER_V3_H_
#define NET_DISK_CACHE_BLOCKFILE_BACKEND_WORKER_V3_H_

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/blockfile/backend_impl_v3.h"
#include "net/disk_cache/blockfile/block_files.h"

namespace disk_cache {

class BackendImplV3::Worker : public base::RefCountedThreadSafe<Worker> {
 public:
  Worker(const base::FilePath& path, base::MessageLoopProxy* main_thread);

  // Performs general initialization for this current instance of the cache.
  int Init(const CompletionCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<Worker>;

  ~Worker();
  void CleanupCache();

  // Returns the full name for an external storage file.
  base::FilePath GetFileName(Addr address) const;

  // Creates a new backing file for the cache index.
  bool CreateBackingStore(disk_cache::File* file);
  bool InitBackingStore(bool* file_created);

  // Performs basic checks on the index file. Returns false on failure.
  bool CheckIndex();

  base::FilePath path_;  // Path to the folder used as backing storage.
  BlockFiles block_files_;  // Set of files used to store all data.
  bool init_;  // controls the initialization of the system.

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_BACKEND_WORKER_V3_H_
