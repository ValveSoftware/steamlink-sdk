// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_APPCACHE_APPCACHE_DISK_CACHE_H_
#define WEBKIT_BROWSER_APPCACHE_APPCACHE_DISK_CACHE_H_

#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/disk_cache/disk_cache.h"
#include "webkit/browser/appcache/appcache_response.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace appcache {

// An implementation of AppCacheDiskCacheInterface that
// uses net::DiskCache as the backing store.
class WEBKIT_STORAGE_BROWSER_EXPORT AppCacheDiskCache
    : public AppCacheDiskCacheInterface {
 public:
  AppCacheDiskCache();
  virtual ~AppCacheDiskCache();

  // Initializes the object to use disk backed storage.
  int InitWithDiskBackend(const base::FilePath& disk_cache_directory,
                          int disk_cache_size, bool force,
                          base::MessageLoopProxy* cache_thread,
                          const net::CompletionCallback& callback);

  // Initializes the object to use memory only storage.
  // This is used for Chrome's incognito browsing.
  int InitWithMemBackend(int disk_cache_size,
                         const net::CompletionCallback& callback);

  void Disable();
  bool is_disabled() const { return is_disabled_; }

  virtual int CreateEntry(int64 key, Entry** entry,
                          const net::CompletionCallback& callback) OVERRIDE;
  virtual int OpenEntry(int64 key, Entry** entry,
                        const net::CompletionCallback& callback) OVERRIDE;
  virtual int DoomEntry(int64 key,
                        const net::CompletionCallback& callback) OVERRIDE;

 private:
  class CreateBackendCallbackShim;
  class EntryImpl;

  // PendingCalls allow CreateEntry, OpenEntry, and DoomEntry to be called
  // immediately after construction, without waiting for the
  // underlying disk_cache::Backend to be fully constructed. Early
  // calls are queued up and serviced once the disk_cache::Backend is
  // really ready to go.
  enum PendingCallType {
    CREATE,
    OPEN,
    DOOM
  };
  struct PendingCall {
    PendingCallType call_type;
    int64 key;
    Entry** entry;
    net::CompletionCallback callback;

    PendingCall();

    PendingCall(PendingCallType call_type, int64 key,
                Entry** entry, const net::CompletionCallback& callback);

    ~PendingCall();
  };
  typedef std::vector<PendingCall> PendingCalls;

  class ActiveCall;
  typedef std::set<ActiveCall*> ActiveCalls;
  typedef std::set<EntryImpl*> OpenEntries;

  bool is_initializing() const {
    return create_backend_callback_.get() != NULL;
  }
  disk_cache::Backend* disk_cache() { return disk_cache_.get(); }
  int Init(net::CacheType cache_type, const base::FilePath& directory,
           int cache_size, bool force, base::MessageLoopProxy* cache_thread,
           const net::CompletionCallback& callback);
  void OnCreateBackendComplete(int rv);
  void AddActiveCall(ActiveCall* call) { active_calls_.insert(call); }
  void RemoveActiveCall(ActiveCall* call) { active_calls_.erase(call); }
  void AddOpenEntry(EntryImpl* entry) { open_entries_.insert(entry); }
  void RemoveOpenEntry(EntryImpl* entry) { open_entries_.erase(entry); }

  bool is_disabled_;
  net::CompletionCallback init_callback_;
  scoped_refptr<CreateBackendCallbackShim> create_backend_callback_;
  PendingCalls pending_calls_;
  ActiveCalls active_calls_;
  OpenEntries open_entries_;
  scoped_ptr<disk_cache::Backend> disk_cache_;
};

}  // namespace appcache

#endif  // WEBKIT_BROWSER_APPCACHE_APPCACHE_DISK_CACHE_H_
