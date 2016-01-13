// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_backend_impl.h"

#include <algorithm>
#include <cstdlib>
#include <functional>

#if defined(OS_POSIX)
#include <sys/resource.h>
#endif

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_entry_impl.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/disk_cache/simple/simple_version_upgrade.h"

using base::Callback;
using base::Closure;
using base::FilePath;
using base::MessageLoopProxy;
using base::SequencedWorkerPool;
using base::SingleThreadTaskRunner;
using base::Time;
using base::DirectoryExists;
using base::CreateDirectory;

namespace disk_cache {

namespace {

// Maximum number of concurrent worker pool threads, which also is the limit
// on concurrent IO (as we use one thread per IO request).
const int kDefaultMaxWorkerThreads = 50;

const char kThreadNamePrefix[] = "SimpleCache";

// Maximum fraction of the cache that one entry can consume.
const int kMaxFileRatio = 8;

// A global sequenced worker pool to use for launching all tasks.
SequencedWorkerPool* g_sequenced_worker_pool = NULL;

void MaybeCreateSequencedWorkerPool() {
  if (!g_sequenced_worker_pool) {
    int max_worker_threads = kDefaultMaxWorkerThreads;

    const std::string thread_count_field_trial =
        base::FieldTrialList::FindFullName("SimpleCacheMaxThreads");
    if (!thread_count_field_trial.empty()) {
      max_worker_threads =
          std::max(1, std::atoi(thread_count_field_trial.c_str()));
    }

    g_sequenced_worker_pool = new SequencedWorkerPool(max_worker_threads,
                                                      kThreadNamePrefix);
    g_sequenced_worker_pool->AddRef();  // Leak it.
  }
}

bool g_fd_limit_histogram_has_been_populated = false;

void MaybeHistogramFdLimit(net::CacheType cache_type) {
  if (g_fd_limit_histogram_has_been_populated)
    return;

  // Used in histograms; add new entries at end.
  enum FdLimitStatus {
    FD_LIMIT_STATUS_UNSUPPORTED = 0,
    FD_LIMIT_STATUS_FAILED      = 1,
    FD_LIMIT_STATUS_SUCCEEDED   = 2,
    FD_LIMIT_STATUS_MAX         = 3
  };
  FdLimitStatus fd_limit_status = FD_LIMIT_STATUS_UNSUPPORTED;
  int soft_fd_limit = 0;
  int hard_fd_limit = 0;

#if defined(OS_POSIX)
  struct rlimit nofile;
  if (!getrlimit(RLIMIT_NOFILE, &nofile)) {
    soft_fd_limit = nofile.rlim_cur;
    hard_fd_limit = nofile.rlim_max;
    fd_limit_status = FD_LIMIT_STATUS_SUCCEEDED;
  } else {
    fd_limit_status = FD_LIMIT_STATUS_FAILED;
  }
#endif

  SIMPLE_CACHE_UMA(ENUMERATION,
                   "FileDescriptorLimitStatus", cache_type,
                   fd_limit_status, FD_LIMIT_STATUS_MAX);
  if (fd_limit_status == FD_LIMIT_STATUS_SUCCEEDED) {
    SIMPLE_CACHE_UMA(SPARSE_SLOWLY,
                     "FileDescriptorLimitSoft", cache_type, soft_fd_limit);
    SIMPLE_CACHE_UMA(SPARSE_SLOWLY,
                     "FileDescriptorLimitHard", cache_type, hard_fd_limit);
  }

  g_fd_limit_histogram_has_been_populated = true;
}

// Detects if the files in the cache directory match the current disk cache
// backend type and version. If the directory contains no cache, occupies it
// with the fresh structure.
bool FileStructureConsistent(const base::FilePath& path) {
  if (!base::PathExists(path) && !base::CreateDirectory(path)) {
    LOG(ERROR) << "Failed to create directory: " << path.LossyDisplayName();
    return false;
  }
  return disk_cache::UpgradeSimpleCacheOnDisk(path);
}

// A context used by a BarrierCompletionCallback to track state.
struct BarrierContext {
  BarrierContext(int expected)
      : expected(expected),
        count(0),
        had_error(false) {}

  const int expected;
  int count;
  bool had_error;
};

void BarrierCompletionCallbackImpl(
    BarrierContext* context,
    const net::CompletionCallback& final_callback,
    int result) {
  DCHECK_GT(context->expected, context->count);
  if (context->had_error)
    return;
  if (result != net::OK) {
    context->had_error = true;
    final_callback.Run(result);
    return;
  }
  ++context->count;
  if (context->count == context->expected)
    final_callback.Run(net::OK);
}

// A barrier completion callback is a net::CompletionCallback that waits for
// |count| successful results before invoking |final_callback|. In the case of
// an error, the first error is passed to |final_callback| and all others
// are ignored.
net::CompletionCallback MakeBarrierCompletionCallback(
    int count,
    const net::CompletionCallback& final_callback) {
  BarrierContext* context = new BarrierContext(count);
  return base::Bind(&BarrierCompletionCallbackImpl,
                    base::Owned(context), final_callback);
}

// A short bindable thunk that ensures a completion callback is always called
// after running an operation asynchronously.
void RunOperationAndCallback(
    const Callback<int(const net::CompletionCallback&)>& operation,
    const net::CompletionCallback& operation_callback) {
  const int operation_result = operation.Run(operation_callback);
  if (operation_result != net::ERR_IO_PENDING)
    operation_callback.Run(operation_result);
}

void RecordIndexLoad(net::CacheType cache_type,
                     base::TimeTicks constructed_since,
                     int result) {
  const base::TimeDelta creation_to_index = base::TimeTicks::Now() -
                                            constructed_since;
  if (result == net::OK) {
    SIMPLE_CACHE_UMA(TIMES, "CreationToIndex", cache_type, creation_to_index);
  } else {
    SIMPLE_CACHE_UMA(TIMES,
                     "CreationToIndexFail", cache_type, creation_to_index);
  }
}

}  // namespace

SimpleBackendImpl::SimpleBackendImpl(const FilePath& path,
                                     int max_bytes,
                                     net::CacheType cache_type,
                                     base::SingleThreadTaskRunner* cache_thread,
                                     net::NetLog* net_log)
    : path_(path),
      cache_type_(cache_type),
      cache_thread_(cache_thread),
      orig_max_size_(max_bytes),
      entry_operations_mode_(
          cache_type == net::DISK_CACHE ?
              SimpleEntryImpl::OPTIMISTIC_OPERATIONS :
              SimpleEntryImpl::NON_OPTIMISTIC_OPERATIONS),
      net_log_(net_log) {
  MaybeHistogramFdLimit(cache_type_);
}

SimpleBackendImpl::~SimpleBackendImpl() {
  index_->WriteToDisk();
}

int SimpleBackendImpl::Init(const CompletionCallback& completion_callback) {
  MaybeCreateSequencedWorkerPool();

  worker_pool_ = g_sequenced_worker_pool->GetTaskRunnerWithShutdownBehavior(
      SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  index_.reset(new SimpleIndex(MessageLoopProxy::current(), this, cache_type_,
                               make_scoped_ptr(new SimpleIndexFile(
                                   cache_thread_.get(), worker_pool_.get(),
                                   cache_type_, path_))));
  index_->ExecuteWhenReady(
      base::Bind(&RecordIndexLoad, cache_type_, base::TimeTicks::Now()));

  PostTaskAndReplyWithResult(
      cache_thread_,
      FROM_HERE,
      base::Bind(&SimpleBackendImpl::InitCacheStructureOnDisk, path_,
                 orig_max_size_),
      base::Bind(&SimpleBackendImpl::InitializeIndex, AsWeakPtr(),
                 completion_callback));
  return net::ERR_IO_PENDING;
}

bool SimpleBackendImpl::SetMaxSize(int max_bytes) {
  orig_max_size_ = max_bytes;
  return index_->SetMaxSize(max_bytes);
}

int SimpleBackendImpl::GetMaxFileSize() const {
  return index_->max_size() / kMaxFileRatio;
}

void SimpleBackendImpl::OnDeactivated(const SimpleEntryImpl* entry) {
  active_entries_.erase(entry->entry_hash());
}

void SimpleBackendImpl::OnDoomStart(uint64 entry_hash) {
  // TODO(ttuttle): Revert to DCHECK once http://crbug.com/317138 is fixed.
  CHECK_EQ(0u, entries_pending_doom_.count(entry_hash));
  entries_pending_doom_.insert(
      std::make_pair(entry_hash, std::vector<Closure>()));
}

void SimpleBackendImpl::OnDoomComplete(uint64 entry_hash) {
  // TODO(ttuttle): Revert to DCHECK once http://crbug.com/317138 is fixed.
  CHECK_EQ(1u, entries_pending_doom_.count(entry_hash));
  base::hash_map<uint64, std::vector<Closure> >::iterator it =
      entries_pending_doom_.find(entry_hash);
  std::vector<Closure> to_run_closures;
  to_run_closures.swap(it->second);
  entries_pending_doom_.erase(it);

  std::for_each(to_run_closures.begin(), to_run_closures.end(),
                std::mem_fun_ref(&Closure::Run));
}

void SimpleBackendImpl::DoomEntries(std::vector<uint64>* entry_hashes,
                                    const net::CompletionCallback& callback) {
  scoped_ptr<std::vector<uint64> >
      mass_doom_entry_hashes(new std::vector<uint64>());
  mass_doom_entry_hashes->swap(*entry_hashes);

  std::vector<uint64> to_doom_individually_hashes;

  // For each of the entry hashes, there are two cases:
  // 1. The entry is either open or pending doom, and so it should be doomed
  //    individually to avoid flakes.
  // 2. The entry is not in use at all, so we can call
  //    SimpleSynchronousEntry::DoomEntrySet and delete the files en masse.
  for (int i = mass_doom_entry_hashes->size() - 1; i >= 0; --i) {
    const uint64 entry_hash = (*mass_doom_entry_hashes)[i];
    // TODO(ttuttle): Revert to DCHECK once http://crbug.com/317138 is fixed.
    CHECK(active_entries_.count(entry_hash) == 0 ||
          entries_pending_doom_.count(entry_hash) == 0)
        << "The entry 0x" << std::hex << entry_hash
        << " is both active and pending doom.";
    if (!active_entries_.count(entry_hash) &&
        !entries_pending_doom_.count(entry_hash)) {
      continue;
    }

    to_doom_individually_hashes.push_back(entry_hash);

    (*mass_doom_entry_hashes)[i] = mass_doom_entry_hashes->back();
    mass_doom_entry_hashes->resize(mass_doom_entry_hashes->size() - 1);
  }

  net::CompletionCallback barrier_callback =
      MakeBarrierCompletionCallback(to_doom_individually_hashes.size() + 1,
                                    callback);
  for (std::vector<uint64>::const_iterator
           it = to_doom_individually_hashes.begin(),
           end = to_doom_individually_hashes.end(); it != end; ++it) {
    const int doom_result = DoomEntryFromHash(*it, barrier_callback);
    // TODO(ttuttle): Revert to DCHECK once http://crbug.com/317138 is fixed.
    CHECK_EQ(net::ERR_IO_PENDING, doom_result);
    index_->Remove(*it);
  }

  for (std::vector<uint64>::const_iterator it = mass_doom_entry_hashes->begin(),
                                           end = mass_doom_entry_hashes->end();
       it != end; ++it) {
    index_->Remove(*it);
    OnDoomStart(*it);
  }

  // Taking this pointer here avoids undefined behaviour from calling
  // base::Passed before mass_doom_entry_hashes.get().
  std::vector<uint64>* mass_doom_entry_hashes_ptr =
      mass_doom_entry_hashes.get();
  PostTaskAndReplyWithResult(
      worker_pool_, FROM_HERE,
      base::Bind(&SimpleSynchronousEntry::DoomEntrySet,
                 mass_doom_entry_hashes_ptr, path_),
      base::Bind(&SimpleBackendImpl::DoomEntriesComplete,
                 AsWeakPtr(), base::Passed(&mass_doom_entry_hashes),
                 barrier_callback));
}

net::CacheType SimpleBackendImpl::GetCacheType() const {
  return net::DISK_CACHE;
}

int32 SimpleBackendImpl::GetEntryCount() const {
  // TODO(pasko): Use directory file count when index is not ready.
  return index_->GetEntryCount();
}

int SimpleBackendImpl::OpenEntry(const std::string& key,
                                 Entry** entry,
                                 const CompletionCallback& callback) {
  const uint64 entry_hash = simple_util::GetEntryHashKey(key);

  // TODO(gavinp): Factor out this (not quite completely) repetitive code
  // block from OpenEntry/CreateEntry/DoomEntry.
  base::hash_map<uint64, std::vector<Closure> >::iterator it =
      entries_pending_doom_.find(entry_hash);
  if (it != entries_pending_doom_.end()) {
    Callback<int(const net::CompletionCallback&)> operation =
        base::Bind(&SimpleBackendImpl::OpenEntry,
                   base::Unretained(this), key, entry);
    it->second.push_back(base::Bind(&RunOperationAndCallback,
                                    operation, callback));
    return net::ERR_IO_PENDING;
  }
  scoped_refptr<SimpleEntryImpl> simple_entry =
      CreateOrFindActiveEntry(entry_hash, key);
  CompletionCallback backend_callback =
      base::Bind(&SimpleBackendImpl::OnEntryOpenedFromKey,
                 AsWeakPtr(),
                 key,
                 entry,
                 simple_entry,
                 callback);
  return simple_entry->OpenEntry(entry, backend_callback);
}

int SimpleBackendImpl::CreateEntry(const std::string& key,
                                   Entry** entry,
                                   const CompletionCallback& callback) {
  DCHECK_LT(0u, key.size());
  const uint64 entry_hash = simple_util::GetEntryHashKey(key);

  base::hash_map<uint64, std::vector<Closure> >::iterator it =
      entries_pending_doom_.find(entry_hash);
  if (it != entries_pending_doom_.end()) {
    Callback<int(const net::CompletionCallback&)> operation =
        base::Bind(&SimpleBackendImpl::CreateEntry,
                   base::Unretained(this), key, entry);
    it->second.push_back(base::Bind(&RunOperationAndCallback,
                                    operation, callback));
    return net::ERR_IO_PENDING;
  }
  scoped_refptr<SimpleEntryImpl> simple_entry =
      CreateOrFindActiveEntry(entry_hash, key);
  return simple_entry->CreateEntry(entry, callback);
}

int SimpleBackendImpl::DoomEntry(const std::string& key,
                                 const net::CompletionCallback& callback) {
  const uint64 entry_hash = simple_util::GetEntryHashKey(key);

  base::hash_map<uint64, std::vector<Closure> >::iterator it =
      entries_pending_doom_.find(entry_hash);
  if (it != entries_pending_doom_.end()) {
    Callback<int(const net::CompletionCallback&)> operation =
        base::Bind(&SimpleBackendImpl::DoomEntry, base::Unretained(this), key);
    it->second.push_back(base::Bind(&RunOperationAndCallback,
                                    operation, callback));
    return net::ERR_IO_PENDING;
  }
  scoped_refptr<SimpleEntryImpl> simple_entry =
      CreateOrFindActiveEntry(entry_hash, key);
  return simple_entry->DoomEntry(callback);
}

int SimpleBackendImpl::DoomAllEntries(const CompletionCallback& callback) {
  return DoomEntriesBetween(Time(), Time(), callback);
}

void SimpleBackendImpl::IndexReadyForDoom(Time initial_time,
                                          Time end_time,
                                          const CompletionCallback& callback,
                                          int result) {
  if (result != net::OK) {
    callback.Run(result);
    return;
  }
  scoped_ptr<std::vector<uint64> > removed_key_hashes(
      index_->GetEntriesBetween(initial_time, end_time).release());
  DoomEntries(removed_key_hashes.get(), callback);
}

int SimpleBackendImpl::DoomEntriesBetween(
    const Time initial_time,
    const Time end_time,
    const CompletionCallback& callback) {
  return index_->ExecuteWhenReady(
      base::Bind(&SimpleBackendImpl::IndexReadyForDoom, AsWeakPtr(),
                 initial_time, end_time, callback));
}

int SimpleBackendImpl::DoomEntriesSince(
    const Time initial_time,
    const CompletionCallback& callback) {
  return DoomEntriesBetween(initial_time, Time(), callback);
}

int SimpleBackendImpl::OpenNextEntry(void** iter,
                                     Entry** next_entry,
                                     const CompletionCallback& callback) {
  CompletionCallback get_next_entry =
      base::Bind(&SimpleBackendImpl::GetNextEntryInIterator, AsWeakPtr(), iter,
                 next_entry, callback);
  return index_->ExecuteWhenReady(get_next_entry);
}

void SimpleBackendImpl::EndEnumeration(void** iter) {
  SimpleIndex::HashList* entry_list =
      static_cast<SimpleIndex::HashList*>(*iter);
  delete entry_list;
  *iter = NULL;
}

void SimpleBackendImpl::GetStats(
    std::vector<std::pair<std::string, std::string> >* stats) {
  std::pair<std::string, std::string> item;
  item.first = "Cache type";
  item.second = "Simple Cache";
  stats->push_back(item);
}

void SimpleBackendImpl::OnExternalCacheHit(const std::string& key) {
  index_->UseIfExists(simple_util::GetEntryHashKey(key));
}

void SimpleBackendImpl::InitializeIndex(const CompletionCallback& callback,
                                        const DiskStatResult& result) {
  if (result.net_error == net::OK) {
    index_->SetMaxSize(result.max_size);
    index_->Initialize(result.cache_dir_mtime);
  }
  callback.Run(result.net_error);
}

SimpleBackendImpl::DiskStatResult SimpleBackendImpl::InitCacheStructureOnDisk(
    const base::FilePath& path,
    uint64 suggested_max_size) {
  DiskStatResult result;
  result.max_size = suggested_max_size;
  result.net_error = net::OK;
  if (!FileStructureConsistent(path)) {
    LOG(ERROR) << "Simple Cache Backend: wrong file structure on disk: "
               << path.LossyDisplayName();
    result.net_error = net::ERR_FAILED;
  } else {
    bool mtime_result =
        disk_cache::simple_util::GetMTime(path, &result.cache_dir_mtime);
    DCHECK(mtime_result);
    if (!result.max_size) {
      int64 available = base::SysInfo::AmountOfFreeDiskSpace(path);
      result.max_size = disk_cache::PreferredCacheSize(available);
    }
    DCHECK(result.max_size);
  }
  return result;
}

scoped_refptr<SimpleEntryImpl> SimpleBackendImpl::CreateOrFindActiveEntry(
    const uint64 entry_hash,
    const std::string& key) {
  DCHECK_EQ(entry_hash, simple_util::GetEntryHashKey(key));
  std::pair<EntryMap::iterator, bool> insert_result =
      active_entries_.insert(std::make_pair(entry_hash,
                                            base::WeakPtr<SimpleEntryImpl>()));
  EntryMap::iterator& it = insert_result.first;
  if (insert_result.second)
    DCHECK(!it->second.get());
  if (!it->second.get()) {
    SimpleEntryImpl* entry = new SimpleEntryImpl(
        cache_type_, path_, entry_hash, entry_operations_mode_, this, net_log_);
    entry->SetKey(key);
    it->second = entry->AsWeakPtr();
  }
  DCHECK(it->second.get());
  // It's possible, but unlikely, that we have an entry hash collision with a
  // currently active entry.
  if (key != it->second->key()) {
    it->second->Doom();
    DCHECK_EQ(0U, active_entries_.count(entry_hash));
    return CreateOrFindActiveEntry(entry_hash, key);
  }
  return make_scoped_refptr(it->second.get());
}

int SimpleBackendImpl::OpenEntryFromHash(uint64 entry_hash,
                                         Entry** entry,
                                         const CompletionCallback& callback) {
  base::hash_map<uint64, std::vector<Closure> >::iterator it =
      entries_pending_doom_.find(entry_hash);
  if (it != entries_pending_doom_.end()) {
    Callback<int(const net::CompletionCallback&)> operation =
        base::Bind(&SimpleBackendImpl::OpenEntryFromHash,
                   base::Unretained(this), entry_hash, entry);
    it->second.push_back(base::Bind(&RunOperationAndCallback,
                                    operation, callback));
    return net::ERR_IO_PENDING;
  }

  EntryMap::iterator has_active = active_entries_.find(entry_hash);
  if (has_active != active_entries_.end()) {
    return OpenEntry(has_active->second->key(), entry, callback);
  }

  scoped_refptr<SimpleEntryImpl> simple_entry = new SimpleEntryImpl(
      cache_type_, path_, entry_hash, entry_operations_mode_, this, net_log_);
  CompletionCallback backend_callback =
      base::Bind(&SimpleBackendImpl::OnEntryOpenedFromHash,
                 AsWeakPtr(), entry_hash, entry, simple_entry, callback);
  return simple_entry->OpenEntry(entry, backend_callback);
}

int SimpleBackendImpl::DoomEntryFromHash(uint64 entry_hash,
                                         const CompletionCallback& callback) {
  Entry** entry = new Entry*();
  scoped_ptr<Entry*> scoped_entry(entry);

  base::hash_map<uint64, std::vector<Closure> >::iterator pending_it =
      entries_pending_doom_.find(entry_hash);
  if (pending_it != entries_pending_doom_.end()) {
    Callback<int(const net::CompletionCallback&)> operation =
        base::Bind(&SimpleBackendImpl::DoomEntryFromHash,
                   base::Unretained(this), entry_hash);
    pending_it->second.push_back(base::Bind(&RunOperationAndCallback,
                                    operation, callback));
    return net::ERR_IO_PENDING;
  }

  EntryMap::iterator active_it = active_entries_.find(entry_hash);
  if (active_it != active_entries_.end())
    return active_it->second->DoomEntry(callback);

  // There's no pending dooms, nor any open entry. We can make a trivial
  // call to DoomEntries() to delete this entry.
  std::vector<uint64> entry_hash_vector;
  entry_hash_vector.push_back(entry_hash);
  DoomEntries(&entry_hash_vector, callback);
  return net::ERR_IO_PENDING;
}

void SimpleBackendImpl::GetNextEntryInIterator(
    void** iter,
    Entry** next_entry,
    const CompletionCallback& callback,
    int error_code) {
  if (error_code != net::OK) {
    callback.Run(error_code);
    return;
  }
  if (*iter == NULL) {
    *iter = index()->GetAllHashes().release();
  }
  SimpleIndex::HashList* entry_list =
      static_cast<SimpleIndex::HashList*>(*iter);
  while (entry_list->size() > 0) {
    uint64 entry_hash = entry_list->back();
    entry_list->pop_back();
    if (index()->Has(entry_hash)) {
      *next_entry = NULL;
      CompletionCallback continue_iteration = base::Bind(
          &SimpleBackendImpl::CheckIterationReturnValue,
          AsWeakPtr(),
          iter,
          next_entry,
          callback);
      int error_code_open = OpenEntryFromHash(entry_hash,
                                              next_entry,
                                              continue_iteration);
      if (error_code_open == net::ERR_IO_PENDING)
        return;
      if (error_code_open != net::ERR_FAILED) {
        callback.Run(error_code_open);
        return;
      }
    }
  }
  callback.Run(net::ERR_FAILED);
}

void SimpleBackendImpl::OnEntryOpenedFromHash(
    uint64 hash,
    Entry** entry,
    scoped_refptr<SimpleEntryImpl> simple_entry,
    const CompletionCallback& callback,
    int error_code) {
  if (error_code != net::OK) {
    callback.Run(error_code);
    return;
  }
  DCHECK(*entry);
  std::pair<EntryMap::iterator, bool> insert_result =
      active_entries_.insert(std::make_pair(hash,
                                            base::WeakPtr<SimpleEntryImpl>()));
  EntryMap::iterator& it = insert_result.first;
  const bool did_insert = insert_result.second;
  if (did_insert) {
    // There is no active entry corresponding to this hash. The entry created
    // is put in the map of active entries and returned to the caller.
    it->second = simple_entry->AsWeakPtr();
    callback.Run(error_code);
  } else {
    // The entry was made active with the key while the creation from hash
    // occurred. The entry created from hash needs to be closed, and the one
    // coming from the key returned to the caller.
    simple_entry->Close();
    it->second->OpenEntry(entry, callback);
  }
}

void SimpleBackendImpl::OnEntryOpenedFromKey(
    const std::string key,
    Entry** entry,
    scoped_refptr<SimpleEntryImpl> simple_entry,
    const CompletionCallback& callback,
    int error_code) {
  int final_code = error_code;
  if (final_code == net::OK) {
    bool key_matches = key.compare(simple_entry->key()) == 0;
    if (!key_matches) {
      // TODO(clamy): Add a unit test to check this code path.
      DLOG(WARNING) << "Key mismatch on open.";
      simple_entry->Doom();
      simple_entry->Close();
      final_code = net::ERR_FAILED;
    } else {
      DCHECK_EQ(simple_entry->entry_hash(), simple_util::GetEntryHashKey(key));
    }
    SIMPLE_CACHE_UMA(BOOLEAN, "KeyMatchedOnOpen", cache_type_, key_matches);
  }
  callback.Run(final_code);
}

void SimpleBackendImpl::CheckIterationReturnValue(
    void** iter,
    Entry** entry,
    const CompletionCallback& callback,
    int error_code) {
  if (error_code == net::ERR_FAILED) {
    OpenNextEntry(iter, entry, callback);
    return;
  }
  callback.Run(error_code);
}

void SimpleBackendImpl::DoomEntriesComplete(
    scoped_ptr<std::vector<uint64> > entry_hashes,
    const net::CompletionCallback& callback,
    int result) {
  std::for_each(
      entry_hashes->begin(), entry_hashes->end(),
      std::bind1st(std::mem_fun(&SimpleBackendImpl::OnDoomComplete),
                   this));
  callback.Run(result);
}

void SimpleBackendImpl::FlushWorkerPoolForTesting() {
  if (g_sequenced_worker_pool)
    g_sequenced_worker_pool->FlushForTesting();
}

}  // namespace disk_cache
