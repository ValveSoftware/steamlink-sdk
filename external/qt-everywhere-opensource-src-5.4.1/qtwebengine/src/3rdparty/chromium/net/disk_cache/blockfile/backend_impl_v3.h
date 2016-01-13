// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_BACKEND_IMPL_V3_H_
#define NET_DISK_CACHE_BLOCKFILE_BACKEND_IMPL_V3_H_

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/timer/timer.h"
#include "net/disk_cache/blockfile/block_bitmaps_v3.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/blockfile/eviction_v3.h"
#include "net/disk_cache/blockfile/index_table_v3.h"
#include "net/disk_cache/blockfile/stats.h"
#include "net/disk_cache/blockfile/stress_support.h"
#include "net/disk_cache/blockfile/trace.h"
#include "net/disk_cache/disk_cache.h"

namespace net {
class NetLog;
}  // namespace net

namespace disk_cache {

class EntryImplV3;

// This class implements the Backend interface. An object of this
// class handles the operations of the cache for a particular profile.
class NET_EXPORT_PRIVATE BackendImplV3 : public Backend {
 public:
  enum BackendFlags {
    MAX_SIZE = 1 << 1,            // A maximum size was provided.
    UNIT_TEST_MODE = 1 << 2,      // We are modifying the behavior for testing.
    UPGRADE_MODE = 1 << 3,        // This is the upgrade tool (dump).
    EVICTION_V2 = 1 << 4,         // Use of new eviction was specified.
    BASIC_UNIT_TEST = 1 << 5,     // Identifies almost all unit tests.
    NO_LOAD_PROTECTION = 1 << 6,  // Don't act conservatively under load.
    NO_BUFFERING = 1 << 7,        // Disable extended IO buffering.
    NO_CLEAN_ON_EXIT = 1 << 8     // Avoid saving data at exit time.
  };

  BackendImplV3(const base::FilePath& path,
                base::MessageLoopProxy* cache_thread,
                net::NetLog* net_log);
  virtual ~BackendImplV3();

  // Performs general initialization for this current instance of the cache.
  int Init(const CompletionCallback& callback);

  // Same behavior as OpenNextEntry but walks the list from back to front.
  int OpenPrevEntry(void** iter, Entry** prev_entry,
                    const CompletionCallback& callback);

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int max_bytes);

  // Sets the cache type for this backend.
  void SetType(net::CacheType type);

  // Creates a new storage block of size block_count.
  bool CreateBlock(FileType block_type, int block_count,
                   Addr* block_address);

  // Updates the ranking information for an entry.
  void UpdateRank(EntryImplV3* entry, bool modified);

  // Permanently deletes an entry, but still keeps track of it.
  void InternalDoomEntry(EntryImplV3* entry);

  // This method must be called when an entry is released for the last time, so
  // the entry should not be used anymore. |address| is the cache address of the
  // entry.
  void OnEntryDestroyBegin(Addr address);

  // This method must be called after all resources for an entry have been
  // released.
  void OnEntryDestroyEnd();

  // If the |address| corresponds to an open entry, returns a pointer to that
  // entry, otherwise returns NULL. Note that this method does not increase the
  // ref counter for the entry.
  EntryImplV3* GetOpenEntry(Addr address) const;

  // Returns the id being used on this run of the cache.
  int32 GetCurrentEntryId() const;

  // Returns the maximum size for a file to reside on the cache.
  int MaxFileSize() const;

  // A user data block is being created, extended or truncated.
  void ModifyStorageSize(int32 old_size, int32 new_size);

  // Logs requests that are denied due to being too big.
  void TooMuchStorageRequested(int32 size);

  // Returns true if a temporary buffer is allowed to be extended.
  bool IsAllocAllowed(int current_size, int new_size);

  // Tracks the release of |size| bytes by an entry buffer.
  void BufferDeleted(int size);

  // Only intended for testing the two previous methods.
  int GetTotalBuffersSize() const {
    return buffer_bytes_;
  }

  // Returns true if this instance seems to be under heavy load.
  bool IsLoaded() const;

  // Returns the full histogram name, for the given base |name| and the current
  // cache type. The name will be "DiskCache3.name_type".
  std::string HistogramName(const char* name) const;

  net::CacheType cache_type() const {
    return cache_type_;
  }

  bool read_only() const {
    return read_only_;
  }

  // Returns a weak pointer to this object.
  base::WeakPtr<BackendImplV3> GetWeakPtr();

  // Returns true if we should send histograms for this user again. The caller
  // must call this function only once per run (because it returns always the
  // same thing on a given run).
  bool ShouldReportAgain();

  // Reports some data when we filled up the cache.
  void FirstEviction();

  // Called when an interesting event should be logged (counted).
  void OnEvent(Stats::Counters an_event);

  // Keeps track of payload access (doesn't include metadata).
  void OnRead(int bytes);
  void OnWrite(int bytes);

  // Timer callback to calculate usage statistics and perform backups.
  void OnTimerTick();

  // Sets internal parameters to enable unit testing mode.
  void SetUnitTestMode();

  // Sets internal parameters to enable upgrade mode (for internal tools).
  void SetUpgradeMode();

  // Sets the eviction algorithm to version 2.
  void SetNewEviction();

  // Sets an explicit set of BackendFlags.
  void SetFlags(uint32 flags);

  // Sends a dummy operation through the operation queue, for unit tests.
  int FlushQueueForTest(const CompletionCallback& callback);

  // Trims an entry (all if |empty| is true) from the list of deleted
  // entries. This method should be called directly on the cache thread.
  void TrimForTest(bool empty);

  // Trims an entry (all if |empty| is true) from the list of deleted
  // entries. This method should be called directly on the cache thread.
  void TrimDeletedListForTest(bool empty);

  // Performs a simple self-check, and returns the number of dirty items
  // or an error code (negative value).
  int SelfCheck();

  // Backend implementation.
  virtual net::CacheType GetCacheType() const OVERRIDE;
  virtual int32 GetEntryCount() const OVERRIDE;
  virtual int OpenEntry(const std::string& key, Entry** entry,
                        const CompletionCallback& callback) OVERRIDE;
  virtual int CreateEntry(const std::string& key, Entry** entry,
                          const CompletionCallback& callback) OVERRIDE;
  virtual int DoomEntry(const std::string& key,
                        const CompletionCallback& callback) OVERRIDE;
  virtual int DoomAllEntries(const CompletionCallback& callback) OVERRIDE;
  virtual int DoomEntriesBetween(base::Time initial_time,
                                 base::Time end_time,
                                 const CompletionCallback& callback) OVERRIDE;
  virtual int DoomEntriesSince(base::Time initial_time,
                               const CompletionCallback& callback) OVERRIDE;
  virtual int OpenNextEntry(void** iter, Entry** next_entry,
                            const CompletionCallback& callback) OVERRIDE;
  virtual void EndEnumeration(void** iter) OVERRIDE;
  virtual void GetStats(StatsItems* stats) OVERRIDE;
  virtual void OnExternalCacheHit(const std::string& key) OVERRIDE;

 private:
  friend class EvictionV3;
  typedef base::hash_map<CacheAddr, EntryImplV3*> EntriesMap;
  class Worker;

  void AdjustMaxCacheSize();
  bool InitStats(void* stats_data);
  void StoreStats();

  // Deletes the cache and starts again.
  void RestartCache(bool failure);
  void PrepareForRestart();

  // Performs final cleanup.
  void CleanupCache();

  // Creates a new entry object. Returns zero on success, or a disk_cache error
  // on failure.
  int NewEntry(Addr address, EntryImplV3** entry);

  // Opens the next or previous entry on a cache iteration.
  int OpenFollowingEntry(bool forward, void** iter, Entry** next_entry,
                         const CompletionCallback& callback);

  // Handles the used storage count.
  void AddStorageSize(int32 bytes);
  void SubstractStorageSize(int32 bytes);

  // Update the number of referenced cache entries.
  void IncreaseNumRefs();
  void DecreaseNumRefs();
  void IncreaseNumEntries();
  void DecreaseNumEntries();

  // Dumps current cache statistics to the log.
  void LogStats();

  // Send UMA stats.
  void ReportStats();

  // Reports an uncommon, recoverable error.
  void ReportError(int error);

  // Performs basic checks on the index file. Returns false on failure.
  bool CheckIndex();

  // Part of the self test. Returns the number or dirty entries, or an error.
  int CheckAllEntries();

  // Part of the self test. Returns false if the entry is corrupt.
  bool CheckEntry(EntryImplV3* cache_entry);

  // Returns the maximum total memory for the memory buffers.
  int MaxBuffersSize();

  IndexTable index_;
  base::FilePath path_;  // Path to the folder used as backing storage.
  BlockBitmaps block_files_;
  int32 max_size_;  // Maximum data size for this instance.
  EvictionV3 eviction_;  // Handler of the eviction algorithm.
  EntriesMap open_entries_;
  int num_refs_;  // Number of referenced cache entries.
  int max_refs_;  // Max number of referenced cache entries.
  int entry_count_;  // Number of entries accessed lately.
  int byte_count_;  // Number of bytes read/written lately.
  int buffer_bytes_;  // Total size of the temporary entries' buffers.
  int up_ticks_;  // The number of timer ticks received (OnTimerTick).
  net::CacheType cache_type_;
  int uma_report_;  // Controls transmission of UMA data.
  uint32 user_flags_;  // Flags set by the user.
  bool init_;  // controls the initialization of the system.
  bool restarted_;
  bool read_only_;  // Prevents updates of the rankings data (used by tools).
  bool disabled_;
  bool lru_eviction_;  // What eviction algorithm should be used.
  bool first_timer_;  // True if the timer has not been called.
  bool user_load_;  // True if we see a high load coming from the caller.

  net::NetLog* net_log_;

  Stats stats_;  // Usage statistics.
  scoped_ptr<base::RepeatingTimer<BackendImplV3> > timer_;  // Usage timer.
  scoped_refptr<TraceObject> trace_object_;  // Initializes internal tracing.
  base::WeakPtrFactory<BackendImplV3> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackendImplV3);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_BACKEND_IMPL_V3_H_
