// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/cache_type.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

class Pickle;
class PickleIterator;

namespace disk_cache {

class SimpleIndexDelegate;
class SimpleIndexFile;
struct SimpleIndexLoadResult;

class NET_EXPORT_PRIVATE EntryMetadata {
 public:
  EntryMetadata();
  EntryMetadata(base::Time last_used_time, int entry_size);

  base::Time GetLastUsedTime() const;
  void SetLastUsedTime(const base::Time& last_used_time);

  int GetEntrySize() const { return entry_size_; }
  void SetEntrySize(int entry_size) { entry_size_ = entry_size; }

  // Serialize the data into the provided pickle.
  void Serialize(Pickle* pickle) const;
  bool Deserialize(PickleIterator* it);

  static base::TimeDelta GetLowerEpsilonForTimeComparisons() {
    return base::TimeDelta::FromSeconds(1);
  }
  static base::TimeDelta GetUpperEpsilonForTimeComparisons() {
    return base::TimeDelta();
  }

 private:
  friend class SimpleIndexFileTest;

  // When adding new members here, you should update the Serialize() and
  // Deserialize() methods.

  uint32 last_used_time_seconds_since_epoch_;

  int32 entry_size_;  // Storage size in bytes.
};
COMPILE_ASSERT(sizeof(EntryMetadata) == 8, metadata_size);

// This class is not Thread-safe.
class NET_EXPORT_PRIVATE SimpleIndex
    : public base::SupportsWeakPtr<SimpleIndex> {
 public:
  typedef std::vector<uint64> HashList;

  SimpleIndex(base::SingleThreadTaskRunner* io_thread,
              SimpleIndexDelegate* delegate,
              net::CacheType cache_type,
              scoped_ptr<SimpleIndexFile> simple_index_file);

  virtual ~SimpleIndex();

  void Initialize(base::Time cache_mtime);

  bool SetMaxSize(int max_bytes);
  int max_size() const { return max_size_; }

  void Insert(uint64 entry_hash);
  void Remove(uint64 entry_hash);

  // Check whether the index has the entry given the hash of its key.
  bool Has(uint64 entry_hash) const;

  // Update the last used time of the entry with the given key and return true
  // iff the entry exist in the index.
  bool UseIfExists(uint64 entry_hash);

  void WriteToDisk();

  // Update the size (in bytes) of an entry, in the metadata stored in the
  // index. This should be the total disk-file size including all streams of the
  // entry.
  bool UpdateEntrySize(uint64 entry_hash, int entry_size);

  typedef base::hash_map<uint64, EntryMetadata> EntrySet;

  static void InsertInEntrySet(uint64 entry_hash,
                               const EntryMetadata& entry_metadata,
                               EntrySet* entry_set);

  // Executes the |callback| when the index is ready. Allows multiple callbacks.
  int ExecuteWhenReady(const net::CompletionCallback& callback);

  // Returns entries from the index that have last accessed time matching the
  // range between |initial_time| and |end_time| where open intervals are
  // possible according to the definition given in |DoomEntriesBetween()| in the
  // disk cache backend interface.
  scoped_ptr<HashList> GetEntriesBetween(const base::Time initial_time,
                                         const base::Time end_time);

  // Returns the list of all entries key hash.
  scoped_ptr<HashList> GetAllHashes();

  // Returns number of indexed entries.
  int32 GetEntryCount() const;

  // Returns whether the index has been initialized yet.
  bool initialized() const { return initialized_; }

 private:
  friend class SimpleIndexTest;
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, IndexSizeCorrectOnMerge);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, DiskWriteQueued);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, DiskWriteExecuted);
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexTest, DiskWritePostponed);

  void StartEvictionIfNeeded();
  void EvictionDone(int result);

  void PostponeWritingToDisk();

  void UpdateEntryIteratorSize(EntrySet::iterator* it, int entry_size);

  // Must run on IO Thread.
  void MergeInitializingSet(scoped_ptr<SimpleIndexLoadResult> load_result);

#if defined(OS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);

  scoped_ptr<base::android::ApplicationStatusListener> app_status_listener_;
#endif

  // The owner of |this| must ensure the |delegate_| outlives |this|.
  SimpleIndexDelegate* delegate_;

  EntrySet entries_set_;

  const net::CacheType cache_type_;
  uint64 cache_size_;  // Total cache storage size in bytes.
  uint64 max_size_;
  uint64 high_watermark_;
  uint64 low_watermark_;
  bool eviction_in_progress_;
  base::TimeTicks eviction_start_time_;

  // This stores all the entry_hash of entries that are removed during
  // initialization.
  base::hash_set<uint64> removed_entries_;
  bool initialized_;

  scoped_ptr<SimpleIndexFile> index_file_;

  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;

  // All nonstatic SimpleEntryImpl methods should always be called on the IO
  // thread, in all cases. |io_thread_checker_| documents and enforces this.
  base::ThreadChecker io_thread_checker_;

  // Timestamp of the last time we wrote the index to disk.
  // PostponeWritingToDisk() may give up postponing and allow the write if it
  // has been a while since last time we wrote.
  base::TimeTicks last_write_to_disk_;

  base::OneShotTimer<SimpleIndex> write_to_disk_timer_;
  base::Closure write_to_disk_cb_;

  typedef std::list<net::CompletionCallback> CallbackList;
  CallbackList to_run_when_initialized_;

  // Set to true when the app is on the background. When the app is in the
  // background we can write the index much more frequently, to insure fresh
  // index on next startup.
  bool app_on_background_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_H_
