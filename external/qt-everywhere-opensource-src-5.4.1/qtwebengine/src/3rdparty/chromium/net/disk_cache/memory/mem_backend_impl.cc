// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/memory/mem_backend_impl.h"

#include "base/logging.h"
#include "base/sys_info.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/memory/mem_entry_impl.h"

using base::Time;

namespace {

const int kDefaultInMemoryCacheSize = 10 * 1024 * 1024;
const int kCleanUpMargin = 1024 * 1024;

int LowWaterAdjust(int high_water) {
  if (high_water < kCleanUpMargin)
    return 0;

  return high_water - kCleanUpMargin;
}

}  // namespace

namespace disk_cache {

MemBackendImpl::MemBackendImpl(net::NetLog* net_log)
    : max_size_(0), current_size_(0), net_log_(net_log) {}

MemBackendImpl::~MemBackendImpl() {
  EntryMap::iterator it = entries_.begin();
  while (it != entries_.end()) {
    it->second->Doom();
    it = entries_.begin();
  }
  DCHECK(!current_size_);
}

// Static.
scoped_ptr<Backend> MemBackendImpl::CreateBackend(int max_bytes,
                                                  net::NetLog* net_log) {
  scoped_ptr<MemBackendImpl> cache(new MemBackendImpl(net_log));
  cache->SetMaxSize(max_bytes);
  if (cache->Init())
    return cache.PassAs<Backend>();

  LOG(ERROR) << "Unable to create cache";
  return scoped_ptr<Backend>();
}

bool MemBackendImpl::Init() {
  if (max_size_)
    return true;

  int64 total_memory = base::SysInfo::AmountOfPhysicalMemory();

  if (total_memory <= 0) {
    max_size_ = kDefaultInMemoryCacheSize;
    return true;
  }

  // We want to use up to 2% of the computer's memory, with a limit of 50 MB,
  // reached on systemd with more than 2.5 GB of RAM.
  total_memory = total_memory * 2 / 100;
  if (total_memory > kDefaultInMemoryCacheSize * 5)
    max_size_ = kDefaultInMemoryCacheSize * 5;
  else
    max_size_ = static_cast<int32>(total_memory);

  return true;
}

bool MemBackendImpl::SetMaxSize(int max_bytes) {
  COMPILE_ASSERT(sizeof(max_bytes) == sizeof(max_size_), unsupported_int_model);
  if (max_bytes < 0)
    return false;

  // Zero size means use the default.
  if (!max_bytes)
    return true;

  max_size_ = max_bytes;
  return true;
}

void MemBackendImpl::InternalDoomEntry(MemEntryImpl* entry) {
  // Only parent entries can be passed into this method.
  DCHECK(entry->type() == MemEntryImpl::kParentEntry);

  rankings_.Remove(entry);
  EntryMap::iterator it = entries_.find(entry->GetKey());
  if (it != entries_.end())
    entries_.erase(it);
  else
    NOTREACHED();

  entry->InternalDoom();
}

void MemBackendImpl::UpdateRank(MemEntryImpl* node) {
  rankings_.UpdateRank(node);
}

void MemBackendImpl::ModifyStorageSize(int32 old_size, int32 new_size) {
  if (old_size >= new_size)
    SubstractStorageSize(old_size - new_size);
  else
    AddStorageSize(new_size - old_size);
}

int MemBackendImpl::MaxFileSize() const {
  return max_size_ / 8;
}

void MemBackendImpl::InsertIntoRankingList(MemEntryImpl* entry) {
  rankings_.Insert(entry);
}

void MemBackendImpl::RemoveFromRankingList(MemEntryImpl* entry) {
  rankings_.Remove(entry);
}

net::CacheType MemBackendImpl::GetCacheType() const {
  return net::MEMORY_CACHE;
}

int32 MemBackendImpl::GetEntryCount() const {
  return static_cast<int32>(entries_.size());
}

int MemBackendImpl::OpenEntry(const std::string& key, Entry** entry,
                              const CompletionCallback& callback) {
  if (OpenEntry(key, entry))
    return net::OK;

  return net::ERR_FAILED;
}

int MemBackendImpl::CreateEntry(const std::string& key, Entry** entry,
                                const CompletionCallback& callback) {
  if (CreateEntry(key, entry))
    return net::OK;

  return net::ERR_FAILED;
}

int MemBackendImpl::DoomEntry(const std::string& key,
                              const CompletionCallback& callback) {
  if (DoomEntry(key))
    return net::OK;

  return net::ERR_FAILED;
}

int MemBackendImpl::DoomAllEntries(const CompletionCallback& callback) {
  if (DoomAllEntries())
    return net::OK;

  return net::ERR_FAILED;
}

int MemBackendImpl::DoomEntriesBetween(const base::Time initial_time,
                                       const base::Time end_time,
                                       const CompletionCallback& callback) {
  if (DoomEntriesBetween(initial_time, end_time))
    return net::OK;

  return net::ERR_FAILED;
}

int MemBackendImpl::DoomEntriesSince(const base::Time initial_time,
                                     const CompletionCallback& callback) {
  if (DoomEntriesSince(initial_time))
    return net::OK;

  return net::ERR_FAILED;
}

int MemBackendImpl::OpenNextEntry(void** iter, Entry** next_entry,
                                  const CompletionCallback& callback) {
  if (OpenNextEntry(iter, next_entry))
    return net::OK;

  return net::ERR_FAILED;
}

void MemBackendImpl::EndEnumeration(void** iter) {
  *iter = NULL;
}

void MemBackendImpl::OnExternalCacheHit(const std::string& key) {
  EntryMap::iterator it = entries_.find(key);
  if (it != entries_.end()) {
    UpdateRank(it->second);
  }
}

bool MemBackendImpl::OpenEntry(const std::string& key, Entry** entry) {
  EntryMap::iterator it = entries_.find(key);
  if (it == entries_.end())
    return false;

  it->second->Open();

  *entry = it->second;
  return true;
}

bool MemBackendImpl::CreateEntry(const std::string& key, Entry** entry) {
  EntryMap::iterator it = entries_.find(key);
  if (it != entries_.end())
    return false;

  MemEntryImpl* cache_entry = new MemEntryImpl(this);
  if (!cache_entry->CreateEntry(key, net_log_)) {
    delete entry;
    return false;
  }

  rankings_.Insert(cache_entry);
  entries_[key] = cache_entry;

  *entry = cache_entry;
  return true;
}

bool MemBackendImpl::DoomEntry(const std::string& key) {
  Entry* entry;
  if (!OpenEntry(key, &entry))
    return false;

  entry->Doom();
  entry->Close();
  return true;
}

bool MemBackendImpl::DoomAllEntries() {
  TrimCache(true);
  return true;
}

bool MemBackendImpl::DoomEntriesBetween(const Time initial_time,
                                        const Time end_time) {
  if (end_time.is_null())
    return DoomEntriesSince(initial_time);

  DCHECK(end_time >= initial_time);

  MemEntryImpl* node = rankings_.GetNext(NULL);
  // Last valid entry before |node|.
  // Note, that entries after |node| may become invalid during |node| doom in
  // case when they are child entries of it. It is guaranteed that
  // parent node will go prior to it childs in ranking list (see
  // InternalReadSparseData and InternalWriteSparseData).
  MemEntryImpl* last_valid = NULL;

  // rankings_ is ordered by last used, this will descend through the cache
  // and start dooming items before the end_time, and will stop once it reaches
  // an item used before the initial time.
  while (node) {
    if (node->GetLastUsed() < initial_time)
      break;

    if (node->GetLastUsed() < end_time)
      node->Doom();
    else
      last_valid = node;
    node = rankings_.GetNext(last_valid);
  }

  return true;
}

bool MemBackendImpl::DoomEntriesSince(const Time initial_time) {
  for (;;) {
    // Get the entry in the front.
    Entry* entry = rankings_.GetNext(NULL);

    // Break the loop when there are no more entries or the entry is too old.
    if (!entry || entry->GetLastUsed() < initial_time)
      return true;
    entry->Doom();
  }
}

bool MemBackendImpl::OpenNextEntry(void** iter, Entry** next_entry) {
  MemEntryImpl* current = reinterpret_cast<MemEntryImpl*>(*iter);
  MemEntryImpl* node = rankings_.GetNext(current);
  // We should never return a child entry so iterate until we hit a parent
  // entry.
  while (node && node->type() != MemEntryImpl::kParentEntry) {
    node = rankings_.GetNext(node);
  }
  *next_entry = node;
  *iter = node;

  if (node)
    node->Open();

  return NULL != node;
}

void MemBackendImpl::TrimCache(bool empty) {
  MemEntryImpl* next = rankings_.GetPrev(NULL);
  if (!next)
    return;

  int target_size = empty ? 0 : LowWaterAdjust(max_size_);
  while (current_size_ > target_size && next) {
    MemEntryImpl* node = next;
    next = rankings_.GetPrev(next);
    if (!node->InUse() || empty) {
      node->Doom();
    }
  }

  return;
}

void MemBackendImpl::AddStorageSize(int32 bytes) {
  current_size_ += bytes;
  DCHECK_GE(current_size_, 0);

  if (current_size_ > max_size_)
    TrimCache(false);
}

void MemBackendImpl::SubstractStorageSize(int32 bytes) {
  current_size_ -= bytes;
  DCHECK_GE(current_size_, 0);
}

}  // namespace disk_cache
