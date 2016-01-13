// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_TRACING_TRACING_CACHE_BACKEND_H_
#define NET_DISK_CACHE_TRACING_TRACING_CACHE_BACKEND_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/disk_cache/blockfile/stats.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

class EntryProxy;

// The TracingCacheBackend implements the Cache Backend interface. It intercepts
// all backend operations from the IO thread and records the time from the start
// of the operation until the result is delivered.
class NET_EXPORT TracingCacheBackend : public Backend,
    public base::SupportsWeakPtr<TracingCacheBackend> {
 public:
  explicit TracingCacheBackend(scoped_ptr<Backend> backend);

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
  friend class EntryProxy;
  enum Operation {
    OP_OPEN,
    OP_CREATE,
    OP_DOOM_ENTRY,
    OP_READ,
    OP_WRITE
  };

  virtual ~TracingCacheBackend();

  EntryProxy* FindOrCreateEntryProxy(Entry* entry);

  void OnDeleteEntry(Entry* e);

  void RecordEvent(base::TimeTicks start_time, Operation op, std::string key,
                   Entry* entry, int result);

  void BackendOpComplete(base::TimeTicks start_time, Operation op,
                         std::string key, Entry** entry,
                         const CompletionCallback& callback, int result);

  net::CompletionCallback BindCompletion(Operation op,
                                         base::TimeTicks start_time,
                                         const std::string& key, Entry **entry,
                                         const net::CompletionCallback& cb);

  scoped_ptr<Backend> backend_;
  typedef std::map<Entry*, EntryProxy*> EntryToProxyMap;
  EntryToProxyMap open_entries_;

  DISALLOW_COPY_AND_ASSIGN(TracingCacheBackend);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_TRACING_TRACING_CACHE_BACKEND_H_
