// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/shader_disk_cache.h"

#include "base/threading/thread_checker.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/common/constants.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace content {

namespace {

static const base::FilePath::CharType kGpuCachePath[] =
    FILE_PATH_LITERAL("GPUCache");

void EntryCloser(disk_cache::Entry* entry) {
  entry->Close();
}

}  // namespace

// ShaderDiskCacheEntry handles the work of caching/updating the cached
// shaders.
class ShaderDiskCacheEntry
    : public base::ThreadChecker,
      public base::RefCounted<ShaderDiskCacheEntry> {
 public:
  ShaderDiskCacheEntry(base::WeakPtr<ShaderDiskCache> cache,
                       const std::string& key,
                       const std::string& shader);
  void Cache();

 private:
  friend class base::RefCounted<ShaderDiskCacheEntry>;

  enum OpType {
    TERMINATE,
    OPEN_ENTRY,
    WRITE_DATA,
    CREATE_ENTRY,
  };

  ~ShaderDiskCacheEntry();

  void OnOpComplete(int rv);

  int OpenCallback(int rv);
  int WriteCallback(int rv);
  int IOComplete(int rv);

  base::WeakPtr<ShaderDiskCache> cache_;
  OpType op_type_;
  std::string key_;
  std::string shader_;
  disk_cache::Entry* entry_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskCacheEntry);
};

// ShaderDiskReadHelper is used to load all of the cached shaders from the
// disk cache and send to the memory cache.
class ShaderDiskReadHelper
    : public base::ThreadChecker,
      public base::RefCounted<ShaderDiskReadHelper> {
 public:
  ShaderDiskReadHelper(base::WeakPtr<ShaderDiskCache> cache, int host_id);
  void LoadCache();

 private:
  friend class base::RefCounted<ShaderDiskReadHelper>;

  enum OpType {
    TERMINATE,
    OPEN_NEXT,
    OPEN_NEXT_COMPLETE,
    READ_COMPLETE,
    ITERATION_FINISHED
  };


  ~ShaderDiskReadHelper();

  void OnOpComplete(int rv);

  int OpenNextEntry();
  int OpenNextEntryComplete(int rv);
  int ReadComplete(int rv);
  int IterationComplete(int rv);

  base::WeakPtr<ShaderDiskCache> cache_;
  OpType op_type_;
  void* iter_;
  scoped_refptr<net::IOBufferWithSize> buf_;
  int host_id_;
  disk_cache::Entry* entry_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskReadHelper);
};

class ShaderClearHelper
    : public base::RefCounted<ShaderClearHelper>,
      public base::SupportsWeakPtr<ShaderClearHelper> {
 public:
  ShaderClearHelper(scoped_refptr<ShaderDiskCache> cache,
                    const base::FilePath& path,
                    const base::Time& delete_begin,
                    const base::Time& delete_end,
                    const base::Closure& callback);
  void Clear();

 private:
  friend class base::RefCounted<ShaderClearHelper>;

  enum OpType {
    TERMINATE,
    VERIFY_CACHE_SETUP,
    DELETE_CACHE
  };

  ~ShaderClearHelper();

  void DoClearShaderCache(int rv);

  scoped_refptr<ShaderDiskCache> cache_;
  OpType op_type_;
  base::FilePath path_;
  base::Time delete_begin_;
  base::Time delete_end_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(ShaderClearHelper);
};

ShaderDiskCacheEntry::ShaderDiskCacheEntry(base::WeakPtr<ShaderDiskCache> cache,
                                           const std::string& key,
                                           const std::string& shader)
  : cache_(cache),
    op_type_(OPEN_ENTRY),
    key_(key),
    shader_(shader),
    entry_(NULL) {
}

ShaderDiskCacheEntry::~ShaderDiskCacheEntry() {
  if (entry_)
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&EntryCloser, entry_));
}

void ShaderDiskCacheEntry::Cache() {
  DCHECK(CalledOnValidThread());
  if (!cache_.get())
    return;

  int rv = cache_->backend()->OpenEntry(
      key_,
      &entry_,
      base::Bind(&ShaderDiskCacheEntry::OnOpComplete, this));
  if (rv != net::ERR_IO_PENDING)
    OnOpComplete(rv);
}

void ShaderDiskCacheEntry::OnOpComplete(int rv) {
  DCHECK(CalledOnValidThread());
  if (!cache_.get())
    return;

  do {
    switch (op_type_) {
      case OPEN_ENTRY:
        rv = OpenCallback(rv);
        break;
      case CREATE_ENTRY:
        rv = WriteCallback(rv);
        break;
      case WRITE_DATA:
        rv = IOComplete(rv);
        break;
      case TERMINATE:
        rv = net::ERR_IO_PENDING;  // break the loop.
        break;
      default:
        NOTREACHED();  // Invalid op_type_ provided.
        break;
    }
  } while (rv != net::ERR_IO_PENDING);
}

int ShaderDiskCacheEntry::OpenCallback(int rv) {
  DCHECK(CalledOnValidThread());
  // Called through OnOpComplete, so we know |cache_| is valid.
  if (rv == net::OK) {
    cache_->backend()->OnExternalCacheHit(key_);
    cache_->EntryComplete(this);
    op_type_ = TERMINATE;
    return rv;
  }

  op_type_ = CREATE_ENTRY;
  return cache_->backend()->CreateEntry(
      key_,
      &entry_,
      base::Bind(&ShaderDiskCacheEntry::OnOpComplete, this));
}

int ShaderDiskCacheEntry::WriteCallback(int rv) {
  DCHECK(CalledOnValidThread());
  // Called through OnOpComplete, so we know |cache_| is valid.
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to create shader cache entry: " << rv;
    cache_->EntryComplete(this);
    op_type_ = TERMINATE;
    return rv;
  }

  op_type_ = WRITE_DATA;
  scoped_refptr<net::StringIOBuffer> io_buf = new net::StringIOBuffer(shader_);
  return entry_->WriteData(
      1,
      0,
      io_buf.get(),
      shader_.length(),
      base::Bind(&ShaderDiskCacheEntry::OnOpComplete, this),
      false);
}

int ShaderDiskCacheEntry::IOComplete(int rv) {
  DCHECK(CalledOnValidThread());
  // Called through OnOpComplete, so we know |cache_| is valid.
  cache_->EntryComplete(this);
  op_type_ = TERMINATE;
  return rv;
}

ShaderDiskReadHelper::ShaderDiskReadHelper(
    base::WeakPtr<ShaderDiskCache> cache,
    int host_id)
    : cache_(cache),
      op_type_(OPEN_NEXT),
      iter_(NULL),
      buf_(NULL),
      host_id_(host_id),
      entry_(NULL) {
}

void ShaderDiskReadHelper::LoadCache() {
  DCHECK(CalledOnValidThread());
  if (!cache_.get())
    return;
  OnOpComplete(net::OK);
}

void ShaderDiskReadHelper::OnOpComplete(int rv) {
  DCHECK(CalledOnValidThread());
  if (!cache_.get())
    return;

  do {
    switch (op_type_) {
      case OPEN_NEXT:
        rv = OpenNextEntry();
        break;
      case OPEN_NEXT_COMPLETE:
        rv = OpenNextEntryComplete(rv);
        break;
      case READ_COMPLETE:
        rv = ReadComplete(rv);
        break;
      case ITERATION_FINISHED:
        rv = IterationComplete(rv);
        break;
      case TERMINATE:
        cache_->ReadComplete();
        rv = net::ERR_IO_PENDING;  // break the loop
        break;
      default:
        NOTREACHED();  // Invalid state for read helper
        rv = net::ERR_FAILED;
        break;
    }
  } while (rv != net::ERR_IO_PENDING);
}

int ShaderDiskReadHelper::OpenNextEntry() {
  DCHECK(CalledOnValidThread());
  // Called through OnOpComplete, so we know |cache_| is valid.
  op_type_ = OPEN_NEXT_COMPLETE;
  return cache_->backend()->OpenNextEntry(
      &iter_,
      &entry_,
      base::Bind(&ShaderDiskReadHelper::OnOpComplete, this));
}

int ShaderDiskReadHelper::OpenNextEntryComplete(int rv) {
  DCHECK(CalledOnValidThread());
  // Called through OnOpComplete, so we know |cache_| is valid.
  if (rv == net::ERR_FAILED) {
    op_type_ = ITERATION_FINISHED;
    return net::OK;
  }

  if (rv < 0)
    return rv;

  op_type_ = READ_COMPLETE;
  buf_ = new net::IOBufferWithSize(entry_->GetDataSize(1));
  return entry_->ReadData(
      1,
      0,
      buf_.get(),
      buf_->size(),
      base::Bind(&ShaderDiskReadHelper::OnOpComplete, this));
}

int ShaderDiskReadHelper::ReadComplete(int rv) {
  DCHECK(CalledOnValidThread());
  // Called through OnOpComplete, so we know |cache_| is valid.
  if (rv && rv == buf_->size()) {
    GpuProcessHost* host = GpuProcessHost::FromID(host_id_);
    if (host)
      host->LoadedShader(entry_->GetKey(), std::string(buf_->data(),
                                                       buf_->size()));
  }

  buf_ = NULL;
  entry_->Close();
  entry_ = NULL;

  op_type_ = OPEN_NEXT;
  return net::OK;
}

int ShaderDiskReadHelper::IterationComplete(int rv) {
  DCHECK(CalledOnValidThread());
  // Called through OnOpComplete, so we know |cache_| is valid.
  cache_->backend()->EndEnumeration(&iter_);
  iter_ = NULL;
  op_type_ = TERMINATE;
  return net::OK;
}

ShaderDiskReadHelper::~ShaderDiskReadHelper() {
  if (entry_)
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&EntryCloser, entry_));
}

ShaderClearHelper::ShaderClearHelper(scoped_refptr<ShaderDiskCache> cache,
                    const base::FilePath& path,
                    const base::Time& delete_begin,
                    const base::Time& delete_end,
                    const base::Closure& callback)
    : cache_(cache),
      op_type_(VERIFY_CACHE_SETUP),
      path_(path),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      callback_(callback) {
}

ShaderClearHelper::~ShaderClearHelper() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ShaderClearHelper::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DoClearShaderCache(net::OK);
}

void ShaderClearHelper::DoClearShaderCache(int rv) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Hold a ref to ourselves so when we do the CacheCleared call we don't get
  // auto-deleted when our ref count drops to zero.
  scoped_refptr<ShaderClearHelper> helper = this;

  while (rv != net::ERR_IO_PENDING) {
    switch (op_type_) {
      case VERIFY_CACHE_SETUP:
        rv = cache_->SetAvailableCallback(
            base::Bind(&ShaderClearHelper::DoClearShaderCache, AsWeakPtr()));
        op_type_ = DELETE_CACHE;
        break;
      case DELETE_CACHE:
        rv = cache_->Clear(
            delete_begin_, delete_end_,
            base::Bind(&ShaderClearHelper::DoClearShaderCache, AsWeakPtr()));
        op_type_ = TERMINATE;
        break;
      case TERMINATE:
        ShaderCacheFactory::GetInstance()->CacheCleared(path_);
        callback_.Run();
        rv = net::ERR_IO_PENDING;  // Break the loop.
        break;
      default:
        NOTREACHED();  // Invalid state provided.
        op_type_ = TERMINATE;
        break;
    }
  }
}

// static
ShaderCacheFactory* ShaderCacheFactory::GetInstance() {
  return Singleton<ShaderCacheFactory,
      LeakySingletonTraits<ShaderCacheFactory> >::get();
}

ShaderCacheFactory::ShaderCacheFactory() {
}

ShaderCacheFactory::~ShaderCacheFactory() {
}

void ShaderCacheFactory::SetCacheInfo(int32 client_id,
                                      const base::FilePath& path) {
  client_id_to_path_map_[client_id] = path;
}

void ShaderCacheFactory::RemoveCacheInfo(int32 client_id) {
  client_id_to_path_map_.erase(client_id);
}

scoped_refptr<ShaderDiskCache> ShaderCacheFactory::Get(int32 client_id) {
  ClientIdToPathMap::iterator iter =
      client_id_to_path_map_.find(client_id);
  if (iter == client_id_to_path_map_.end())
    return NULL;
  return ShaderCacheFactory::GetByPath(iter->second);
}

scoped_refptr<ShaderDiskCache> ShaderCacheFactory::GetByPath(
    const base::FilePath& path) {
  ShaderCacheMap::iterator iter = shader_cache_map_.find(path);
  if (iter != shader_cache_map_.end())
    return iter->second;

  ShaderDiskCache* cache = new ShaderDiskCache(path);
  cache->Init();
  return cache;
}

void ShaderCacheFactory::AddToCache(const base::FilePath& key,
                                    ShaderDiskCache* cache) {
  shader_cache_map_[key] = cache;
}

void ShaderCacheFactory::RemoveFromCache(const base::FilePath& key) {
  shader_cache_map_.erase(key);
}

void ShaderCacheFactory::ClearByPath(const base::FilePath& path,
                                     const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  scoped_refptr<ShaderClearHelper> helper = new ShaderClearHelper(
      GetByPath(path), path, delete_begin, delete_end, callback);

  // We could receive requests to clear the same path with different
  // begin/end times. So, we keep a list of requests. If we haven't seen this
  // path before we kick off the clear and add it to the list. If we have see it
  // already, then we already have a clear running. We add this clear to the
  // list and wait for any previous clears to finish.
  ShaderClearMap::iterator iter = shader_clear_map_.find(path);
  if (iter != shader_clear_map_.end()) {
    iter->second.push(helper);
    return;
  }

  shader_clear_map_.insert(
      std::pair<base::FilePath, ShaderClearQueue>(path, ShaderClearQueue()));
  shader_clear_map_[path].push(helper);
  helper->Clear();
}

void ShaderCacheFactory::CacheCleared(const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ShaderClearMap::iterator iter = shader_clear_map_.find(path);
  if (iter == shader_clear_map_.end()) {
    LOG(ERROR) << "Completed clear but missing clear helper.";
    return;
  }

  iter->second.pop();

  // If there are remaining items in the list we trigger the Clear on the
  // next one.
  if (!iter->second.empty()) {
    iter->second.front()->Clear();
    return;
  }

  shader_clear_map_.erase(path);
}

ShaderDiskCache::ShaderDiskCache(const base::FilePath& cache_path)
    : cache_available_(false),
      host_id_(0),
      cache_path_(cache_path),
      is_initialized_(false) {
  ShaderCacheFactory::GetInstance()->AddToCache(cache_path_, this);
}

ShaderDiskCache::~ShaderDiskCache() {
  ShaderCacheFactory::GetInstance()->RemoveFromCache(cache_path_);
}

void ShaderDiskCache::Init() {
  if (is_initialized_) {
    NOTREACHED();  // can't initialize disk cache twice.
    return;
  }
  is_initialized_ = true;

  int rv = disk_cache::CreateCacheBackend(
      net::SHADER_CACHE,
      net::CACHE_BACKEND_DEFAULT,
      cache_path_.Append(kGpuCachePath),
      gpu::kDefaultMaxProgramCacheMemoryBytes,
      true,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE).get(),
      NULL,
      &backend_,
      base::Bind(&ShaderDiskCache::CacheCreatedCallback, this));

  if (rv == net::OK)
    cache_available_ = true;
}

void ShaderDiskCache::Cache(const std::string& key, const std::string& shader) {
  if (!cache_available_)
    return;

  scoped_refptr<ShaderDiskCacheEntry> shim =
      new ShaderDiskCacheEntry(AsWeakPtr(), key, shader);
  shim->Cache();

  entry_map_[shim] = shim;
}

int ShaderDiskCache::Clear(
    const base::Time begin_time, const base::Time end_time,
    const net::CompletionCallback& completion_callback) {
  int rv;
  if (begin_time.is_null()) {
    rv = backend_->DoomAllEntries(completion_callback);
  } else {
    rv = backend_->DoomEntriesBetween(begin_time, end_time,
                                      completion_callback);
  }
  return rv;
}

int32 ShaderDiskCache::Size() {
  if (!cache_available_)
    return -1;
  return backend_->GetEntryCount();
}

int ShaderDiskCache::SetAvailableCallback(
    const net::CompletionCallback& callback) {
  if (cache_available_)
    return net::OK;
  available_callback_ = callback;
  return net::ERR_IO_PENDING;
}

void ShaderDiskCache::CacheCreatedCallback(int rv) {
  if (rv != net::OK) {
    LOG(ERROR) << "Shader Cache Creation failed: " << rv;
    return;
  }
  helper_ = new ShaderDiskReadHelper(AsWeakPtr(), host_id_);
  helper_->LoadCache();
}

void ShaderDiskCache::EntryComplete(void* entry) {
  entry_map_.erase(entry);

  if (entry_map_.empty() && !cache_complete_callback_.is_null())
    cache_complete_callback_.Run(net::OK);
}

void ShaderDiskCache::ReadComplete() {
  helper_ = NULL;

  // The cache is considered available after we have finished reading any
  // of the old cache values off disk. This prevents a potential race where we
  // are reading from disk and execute a cache clear at the same time.
  cache_available_ = true;
  if (!available_callback_.is_null()) {
    available_callback_.Run(net::OK);
    available_callback_.Reset();
  }
}

int ShaderDiskCache::SetCacheCompleteCallback(
    const net::CompletionCallback& callback) {
  if (entry_map_.empty()) {
    return net::OK;
  }
  cache_complete_callback_ = callback;
  return net::ERR_IO_PENDING;
}

}  // namespace content

