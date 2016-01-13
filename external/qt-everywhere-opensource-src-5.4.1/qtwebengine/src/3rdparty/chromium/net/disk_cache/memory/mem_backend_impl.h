// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_MEMORY_MEM_BACKEND_IMPL_H_
#define NET_DISK_CACHE_MEMORY_MEM_BACKEND_IMPL_H_

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_rankings.h"

namespace net {
class NetLog;
}  // namespace net

namespace disk_cache {

class MemEntryImpl;

// This class implements the Backend interface. An object of this class handles
// the operations of the cache without writing to disk.
class NET_EXPORT_PRIVATE MemBackendImpl : public Backend {
 public:
  explicit MemBackendImpl(net::NetLog* net_log);
  virtual ~MemBackendImpl();

  // Returns an instance of a Backend implemented only in memory. The returned
  // object should be deleted when not needed anymore. max_bytes is the maximum
  // size the cache can grow to. If zero is passed in as max_bytes, the cache
  // will determine the value to use based on the available memory. The returned
  // pointer can be NULL if a fatal error is found.
  static scoped_ptr<Backend> CreateBackend(int max_bytes, net::NetLog* net_log);

  // Performs general initialization for this current instance of the cache.
  bool Init();

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int max_bytes);

  // Permanently deletes an entry.
  void InternalDoomEntry(MemEntryImpl* entry);

  // Updates the ranking information for an entry.
  void UpdateRank(MemEntryImpl* node);

  // A user data block is being created, extended or truncated.
  void ModifyStorageSize(int32 old_size, int32 new_size);

  // Returns the maximum size for a file to reside on the cache.
  int MaxFileSize() const;

  // Insert an MemEntryImpl into the ranking list. This method is only called
  // from MemEntryImpl to insert child entries. The reference can be removed
  // by calling RemoveFromRankingList(|entry|).
  void InsertIntoRankingList(MemEntryImpl* entry);

  // Remove |entry| from ranking list. This method is only called from
  // MemEntryImpl to remove a child entry from the ranking list.
  void RemoveFromRankingList(MemEntryImpl* entry);

  // Backend interface.
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
  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) OVERRIDE {}
  virtual void OnExternalCacheHit(const std::string& key) OVERRIDE;

 private:
  typedef base::hash_map<std::string, MemEntryImpl*> EntryMap;

  // Old Backend interface.
  bool OpenEntry(const std::string& key, Entry** entry);
  bool CreateEntry(const std::string& key, Entry** entry);
  bool DoomEntry(const std::string& key);
  bool DoomAllEntries();
  bool DoomEntriesBetween(const base::Time initial_time,
                          const base::Time end_time);
  bool DoomEntriesSince(const base::Time initial_time);
  bool OpenNextEntry(void** iter, Entry** next_entry);

  // Deletes entries from the cache until the current size is below the limit.
  // If empty is true, the whole cache will be trimmed, regardless of being in
  // use.
  void TrimCache(bool empty);

  // Handles the used storage count.
  void AddStorageSize(int32 bytes);
  void SubstractStorageSize(int32 bytes);

  EntryMap entries_;
  MemRankings rankings_;  // Rankings to be able to trim the cache.
  int32 max_size_;        // Maximum data size for this instance.
  int32 current_size_;

  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(MemBackendImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEMORY_MEM_BACKEND_IMPL_H_
