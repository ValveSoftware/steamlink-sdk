// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_AREA_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_AREA_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/dom_storage/dom_storage_map.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "url/gurl.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace content {

class DOMStorageDatabaseAdapter;
class DOMStorageTaskRunner;
class SessionStorageDatabase;

// Container for a per-origin Map of key/value pairs potentially
// backed by storage on disk and lazily commits changes to disk.
// See class comments for DOMStorageContextImpl for a larger overview.
class CONTENT_EXPORT DOMStorageArea
    : public base::RefCountedThreadSafe<DOMStorageArea> {

 public:
  static const base::FilePath::CharType kDatabaseFileExtension[];
  static base::FilePath DatabaseFileNameFromOrigin(const GURL& origin);
  static GURL OriginFromDatabaseFileName(const base::FilePath& file_name);

  // Commence aggressive flushing. This should be called early in the startup -
  // before any localStorage writing. Currently scheduled writes will not be
  // rescheduled and will be flushed at the scheduled time after which
  // aggressive flushing will commence.
  static void EnableAggressiveCommitDelay();

  // Local storage. Backed on disk if directory is nonempty.
  DOMStorageArea(const GURL& origin,
                 const base::FilePath& directory,
                 DOMStorageTaskRunner* task_runner);

  // Session storage. Backed on disk if |session_storage_backing| is not NULL.
  DOMStorageArea(int64_t namespace_id,
                 const std::string& persistent_namespace_id,
                 const GURL& origin,
                 SessionStorageDatabase* session_storage_backing,
                 DOMStorageTaskRunner* task_runner);

  const GURL& origin() const { return origin_; }
  int64_t namespace_id() const { return namespace_id_; }
  size_t map_usage_in_bytes() const { return map_ ? map_->bytes_used() : 0; }

  // Writes a copy of the current set of values in the area to the |map|.
  void ExtractValues(DOMStorageValuesMap* map);

  unsigned Length();
  base::NullableString16 Key(unsigned index);
  base::NullableString16 GetItem(const base::string16& key);
  bool SetItem(const base::string16& key, const base::string16& value,
               base::NullableString16* old_value);
  bool RemoveItem(const base::string16& key, base::string16* old_value);
  bool Clear();
  void FastClear();

  DOMStorageArea* ShallowCopy(
      int64_t destination_namespace_id,
      const std::string& destination_persistent_namespace_id);

  bool HasUncommittedChanges() const;
  void ScheduleImmediateCommit();

  // Similar to Clear() but more optimized for just deleting
  // without raising events.
  void DeleteOrigin();

  // Frees up memory when possible. Typically, this method returns
  // the object to its just constructed state, however if uncommitted
  // changes are pending, it does nothing.
  void PurgeMemory();

  // Schedules the commit of any unsaved changes and enters a
  // shutdown state such that the value getters and setters will
  // no longer do anything.
  void Shutdown();

  // Returns true if the data is loaded in memory.
  bool IsLoadedInMemory() const { return is_initial_import_done_; }

  // Adds memory statistics to |pmd| for chrome://tracing.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd);

 private:
  friend class DOMStorageAreaTest;
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, DOMStorageAreaBasics);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, BackingDatabaseOpened);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, TestDatabaseFilePath);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, CommitTasks);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, CommitChangesAtShutdown);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, DeleteOrigin);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, PurgeMemory);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageAreaTest, RateLimiter);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageContextImplTest, PersistentIds);
  FRIEND_TEST_ALL_PREFIXES(DOMStorageContextImplTest, PurgeMemory);
  friend class base::RefCountedThreadSafe<DOMStorageArea>;

  // Used to rate limit commits.
  class CONTENT_EXPORT RateLimiter {
   public:
    RateLimiter(size_t desired_rate, base::TimeDelta time_quantum);

    void add_samples(size_t samples) {
      samples_ += samples;
    }

    // Computes the total time needed to process the total samples seen
    // at the desired rate.
    base::TimeDelta ComputeTimeNeeded() const;

    // Given the elapsed time since the start of the rate limiting session,
    // computes the delay needed to mimic having processed the total samples
    // seen at the desired rate.
    base::TimeDelta ComputeDelayNeeded(
        const base::TimeDelta elapsed_time) const;

   private:
    float rate_;
    float samples_;
    base::TimeDelta time_quantum_;
  };

  struct CONTENT_EXPORT CommitBatch {
    bool clear_all_first;
    DOMStorageValuesMap changed_values;

    CommitBatch();
    ~CommitBatch();
    size_t GetDataSize() const;
  };

  ~DOMStorageArea();

  // If we haven't done so already and this is a local storage area,
  // will attempt to read any values for this origin currently
  // stored on disk.
  void InitialImportIfNeeded();

  // Post tasks to defer writing a batch of changed values to
  // disk on the commit sequence, and to call back on the primary
  // task sequence when complete.
  CommitBatch* CreateCommitBatchIfNeeded();
  void StartCommitTimer();
  void OnCommitTimer();
  void PostCommitTask();
  void CommitChanges(const CommitBatch* commit_batch);
  void OnCommitComplete();
  base::TimeDelta ComputeCommitDelay() const;

  void ShutdownInCommitSequence();

  static bool s_aggressive_flushing_enabled_;

  int64_t namespace_id_;
  std::string persistent_namespace_id_;
  GURL origin_;
  base::FilePath directory_;
  scoped_refptr<DOMStorageTaskRunner> task_runner_;
  scoped_refptr<DOMStorageMap> map_;
  std::unique_ptr<DOMStorageDatabaseAdapter> backing_;
  scoped_refptr<SessionStorageDatabase> session_storage_backing_;
  bool is_initial_import_done_;
  bool is_shutdown_;
  std::unique_ptr<CommitBatch> commit_batch_;
  int commit_batches_in_flight_;
  base::TimeTicks start_time_;
  RateLimiter data_rate_limiter_;
  RateLimiter commit_rate_limiter_;

  DISALLOW_COPY_AND_ASSIGN(DOMStorageArea);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_AREA_H_
