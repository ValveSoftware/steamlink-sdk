// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/appcache/appcache_disk_cache.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"

namespace appcache {

// A callback shim that provides storage for the 'backend_ptr' value
// and will delete a resulting ptr if completion occurs after its
// been canceled.
class AppCacheDiskCache::CreateBackendCallbackShim
    : public base::RefCounted<CreateBackendCallbackShim> {
 public:
  explicit CreateBackendCallbackShim(AppCacheDiskCache* object)
      : appcache_diskcache_(object) {
  }

  void Cancel() {
    appcache_diskcache_ = NULL;
  }

  void Callback(int rv) {
    if (appcache_diskcache_)
      appcache_diskcache_->OnCreateBackendComplete(rv);
  }

  scoped_ptr<disk_cache::Backend> backend_ptr_;  // Accessed directly.

 private:
  friend class base::RefCounted<CreateBackendCallbackShim>;

  ~CreateBackendCallbackShim() {
  }

  AppCacheDiskCache* appcache_diskcache_;  // Unowned pointer.
};

// An implementation of AppCacheDiskCacheInterface::Entry that's a thin
// wrapper around disk_cache::Entry.
class AppCacheDiskCache::EntryImpl : public Entry {
 public:
  EntryImpl(disk_cache::Entry* disk_cache_entry,
            AppCacheDiskCache* owner)
      : disk_cache_entry_(disk_cache_entry), owner_(owner) {
    DCHECK(disk_cache_entry);
    DCHECK(owner);
    owner_->AddOpenEntry(this);
  }

  // Entry implementation.
  virtual int Read(int index, int64 offset, net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE {
    if (offset < 0 || offset > kint32max)
      return net::ERR_INVALID_ARGUMENT;
    if (!disk_cache_entry_)
      return net::ERR_ABORTED;
    return disk_cache_entry_->ReadData(
        index, static_cast<int>(offset), buf, buf_len, callback);
  }

  virtual int Write(int index, int64 offset, net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE {
    if (offset < 0 || offset > kint32max)
      return net::ERR_INVALID_ARGUMENT;
    if (!disk_cache_entry_)
      return net::ERR_ABORTED;
    const bool kTruncate = true;
    return disk_cache_entry_->WriteData(
        index, static_cast<int>(offset), buf, buf_len, callback, kTruncate);
  }

  virtual int64 GetSize(int index) OVERRIDE {
    return disk_cache_entry_ ? disk_cache_entry_->GetDataSize(index) : 0L;
  }

  virtual void Close() OVERRIDE {
    if (disk_cache_entry_)
      disk_cache_entry_->Close();
    delete this;
  }

  void Abandon() {
    owner_ = NULL;
    disk_cache_entry_->Close();
    disk_cache_entry_ = NULL;
  }

 private:
  virtual ~EntryImpl() {
    if (owner_)
      owner_->RemoveOpenEntry(this);
  }

  disk_cache::Entry* disk_cache_entry_;
  AppCacheDiskCache* owner_;
};

// Separate object to hold state for each Create, Delete, or Doom call
// while the call is in-flight and to produce an EntryImpl upon completion.
class AppCacheDiskCache::ActiveCall {
 public:
  explicit ActiveCall(AppCacheDiskCache* owner)
      : entry_(NULL),
        owner_(owner),
        entry_ptr_(NULL) {
  }

  int CreateEntry(int64 key, Entry** entry,
                  const net::CompletionCallback& callback) {
    int rv = owner_->disk_cache()->CreateEntry(
        base::Int64ToString(key), &entry_ptr_,
        base::Bind(&ActiveCall::OnAsyncCompletion, base::Unretained(this)));
    return HandleImmediateReturnValue(rv, entry, callback);
  }

  int OpenEntry(int64 key, Entry** entry,
                const net::CompletionCallback& callback) {
    int rv = owner_->disk_cache()->OpenEntry(
        base::Int64ToString(key), &entry_ptr_,
        base::Bind(&ActiveCall::OnAsyncCompletion, base::Unretained(this)));
    return HandleImmediateReturnValue(rv, entry, callback);
  }

  int DoomEntry(int64 key, const net::CompletionCallback& callback) {
    int rv = owner_->disk_cache()->DoomEntry(
        base::Int64ToString(key),
        base::Bind(&ActiveCall::OnAsyncCompletion, base::Unretained(this)));
    return HandleImmediateReturnValue(rv, NULL, callback);
  }

 private:
  int HandleImmediateReturnValue(int rv, Entry** entry,
                                 const net::CompletionCallback& callback) {
    if (rv == net::ERR_IO_PENDING) {
      // OnAsyncCompletion will be called later.
      callback_ = callback;
      entry_ = entry;
      owner_->AddActiveCall(this);
      return net::ERR_IO_PENDING;
    }
    if (rv == net::OK && entry)
      *entry = new EntryImpl(entry_ptr_, owner_);
    delete this;
    return rv;
  }

  void OnAsyncCompletion(int rv) {
    owner_->RemoveActiveCall(this);
    if (rv == net::OK && entry_)
      *entry_ = new EntryImpl(entry_ptr_, owner_);
    callback_.Run(rv);
    callback_.Reset();
    delete this;
  }

  Entry** entry_;
  net::CompletionCallback callback_;
  AppCacheDiskCache* owner_;
  disk_cache::Entry* entry_ptr_;
};

AppCacheDiskCache::AppCacheDiskCache()
    : is_disabled_(false) {
}

AppCacheDiskCache::~AppCacheDiskCache() {
  Disable();
}

int AppCacheDiskCache::InitWithDiskBackend(
    const base::FilePath& disk_cache_directory, int disk_cache_size, bool force,
    base::MessageLoopProxy* cache_thread,
    const net::CompletionCallback& callback) {
  return Init(net::APP_CACHE, disk_cache_directory,
              disk_cache_size, force, cache_thread, callback);
}

int AppCacheDiskCache::InitWithMemBackend(
    int mem_cache_size, const net::CompletionCallback& callback) {
  return Init(net::MEMORY_CACHE, base::FilePath(), mem_cache_size, false, NULL,
              callback);
}

void AppCacheDiskCache::Disable() {
  if (is_disabled_)
    return;

  is_disabled_ = true;

  if (create_backend_callback_.get()) {
    create_backend_callback_->Cancel();
    create_backend_callback_ = NULL;
    OnCreateBackendComplete(net::ERR_ABORTED);
  }

  // We need to close open file handles in order to reinitalize the
  // appcache system on the fly. File handles held in both entries and in
  // the main disk_cache::Backend class need to be released.
  for (OpenEntries::const_iterator iter = open_entries_.begin();
       iter != open_entries_.end(); ++iter) {
    (*iter)->Abandon();
  }
  open_entries_.clear();
  disk_cache_.reset();
  STLDeleteElements(&active_calls_);
}

int AppCacheDiskCache::CreateEntry(int64 key, Entry** entry,
                                   const net::CompletionCallback& callback) {
  DCHECK(entry);
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(CREATE, key, entry, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return (new ActiveCall(this))->CreateEntry(key, entry, callback);
}

int AppCacheDiskCache::OpenEntry(int64 key, Entry** entry,
                                 const net::CompletionCallback& callback) {
  DCHECK(entry);
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(OPEN, key, entry, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return (new ActiveCall(this))->OpenEntry(key, entry, callback);
}

int AppCacheDiskCache::DoomEntry(int64 key,
                                 const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  if (is_disabled_)
    return net::ERR_ABORTED;

  if (is_initializing()) {
    pending_calls_.push_back(PendingCall(DOOM, key, NULL, callback));
    return net::ERR_IO_PENDING;
  }

  if (!disk_cache_)
    return net::ERR_FAILED;

  return (new ActiveCall(this))->DoomEntry(key, callback);
}

AppCacheDiskCache::PendingCall::PendingCall()
    : call_type(CREATE),
      key(0),
      entry(NULL) {
}

AppCacheDiskCache::PendingCall::PendingCall(PendingCallType call_type,
    int64 key,
    Entry** entry,
    const net::CompletionCallback& callback)
    : call_type(call_type),
      key(key),
      entry(entry),
      callback(callback) {
}

AppCacheDiskCache::PendingCall::~PendingCall() {}

int AppCacheDiskCache::Init(net::CacheType cache_type,
                            const base::FilePath& cache_directory,
                            int cache_size, bool force,
                            base::MessageLoopProxy* cache_thread,
                            const net::CompletionCallback& callback) {
  DCHECK(!is_initializing() && !disk_cache_.get());
  is_disabled_ = false;
  create_backend_callback_ = new CreateBackendCallbackShim(this);

#if defined(APPCACHE_USE_SIMPLE_CACHE)
  const net::BackendType backend_type = net::CACHE_BACKEND_SIMPLE;
#else
  const net::BackendType backend_type = net::CACHE_BACKEND_DEFAULT;
#endif
  int rv = disk_cache::CreateCacheBackend(
      cache_type, backend_type, cache_directory, cache_size,
      force, cache_thread, NULL, &(create_backend_callback_->backend_ptr_),
      base::Bind(&CreateBackendCallbackShim::Callback,
                 create_backend_callback_));
  if (rv == net::ERR_IO_PENDING)
    init_callback_ = callback;
  else
    OnCreateBackendComplete(rv);
  return rv;
}

void AppCacheDiskCache::OnCreateBackendComplete(int rv) {
  if (rv == net::OK) {
    disk_cache_ = create_backend_callback_->backend_ptr_.Pass();
  }
  create_backend_callback_ = NULL;

  // Invoke our clients callback function.
  if (!init_callback_.is_null()) {
    init_callback_.Run(rv);
    init_callback_.Reset();
  }

  // Service pending calls that were queued up while we were initializing.
  for (PendingCalls::const_iterator iter = pending_calls_.begin();
       iter < pending_calls_.end(); ++iter) {
    int rv = net::ERR_FAILED;
    switch (iter->call_type) {
      case CREATE:
        rv = CreateEntry(iter->key, iter->entry, iter->callback);
        break;
      case OPEN:
        rv = OpenEntry(iter->key, iter->entry, iter->callback);
        break;
      case DOOM:
        rv = DoomEntry(iter->key, iter->callback);
        break;
      default:
        NOTREACHED();
        break;
    }
    if (rv != net::ERR_IO_PENDING)
      iter->callback.Run(rv);
  }
  pending_calls_.clear();
}

}  // namespace appcache
