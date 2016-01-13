// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/backend_impl_v3.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/hash.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/blockfile/disk_format_v3.h"
#include "net/disk_cache/blockfile/entry_impl_v3.h"
#include "net/disk_cache/blockfile/errors.h"
#include "net/disk_cache/blockfile/experiments.h"
#include "net/disk_cache/blockfile/file.h"
#include "net/disk_cache/blockfile/histogram_macros_v3.h"
#include "net/disk_cache/blockfile/index_table_v3.h"
#include "net/disk_cache/blockfile/storage_block-inl.h"
#include "net/disk_cache/cache_util.h"

// Provide a BackendImpl object to macros from histogram_macros.h.
#define CACHE_UMA_BACKEND_IMPL_OBJ this

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace {

#if defined(V3_NOT_JUST_YET_READY)
const int kDefaultCacheSize = 80 * 1024 * 1024;

// Avoid trimming the cache for the first 5 minutes (10 timer ticks).
const int kTrimDelay = 10;
#endif  // defined(V3_NOT_JUST_YET_READY).

}  // namespace

// ------------------------------------------------------------------------

namespace disk_cache {

BackendImplV3::BackendImplV3(const base::FilePath& path,
                             base::MessageLoopProxy* cache_thread,
                             net::NetLog* net_log)
    : index_(NULL),
      path_(path),
      block_files_(),
      max_size_(0),
      up_ticks_(0),
      cache_type_(net::DISK_CACHE),
      uma_report_(0),
      user_flags_(0),
      init_(false),
      restarted_(false),
      read_only_(false),
      disabled_(false),
      lru_eviction_(true),
      first_timer_(true),
      user_load_(false),
      net_log_(net_log),
      ptr_factory_(this) {
}

BackendImplV3::~BackendImplV3() {
  CleanupCache();
}

int BackendImplV3::Init(const CompletionCallback& callback) {
  DCHECK(!init_);
  if (init_)
    return net::ERR_FAILED;

  return net::ERR_IO_PENDING;
}

// ------------------------------------------------------------------------

#if defined(V3_NOT_JUST_YET_READY)
int BackendImplV3::OpenPrevEntry(void** iter, Entry** prev_entry,
                                 const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  return OpenFollowingEntry(true, iter, prev_entry, callback);
}
#endif  // defined(V3_NOT_JUST_YET_READY).

bool BackendImplV3::SetMaxSize(int max_bytes) {
  COMPILE_ASSERT(sizeof(max_bytes) == sizeof(max_size_), unsupported_int_model);
  if (max_bytes < 0)
    return false;

  // Zero size means use the default.
  if (!max_bytes)
    return true;

  // Avoid a DCHECK later on.
  if (max_bytes >= kint32max - kint32max / 10)
    max_bytes = kint32max - kint32max / 10 - 1;

  user_flags_ |= MAX_SIZE;
  max_size_ = max_bytes;
  return true;
}

void BackendImplV3::SetType(net::CacheType type) {
  DCHECK_NE(net::MEMORY_CACHE, type);
  cache_type_ = type;
}

bool BackendImplV3::CreateBlock(FileType block_type, int block_count,
                                Addr* block_address) {
  return block_files_.CreateBlock(block_type, block_count, block_address);
}

#if defined(V3_NOT_JUST_YET_READY)
void BackendImplV3::UpdateRank(EntryImplV3* entry, bool modified) {
  if (read_only_ || (!modified && cache_type() == net::SHADER_CACHE))
    return;
  eviction_.UpdateRank(entry, modified);
}

void BackendImplV3::InternalDoomEntry(EntryImplV3* entry) {
  uint32 hash = entry->GetHash();
  std::string key = entry->GetKey();
  Addr entry_addr = entry->entry()->address();
  bool error;
  EntryImpl* parent_entry = MatchEntry(key, hash, true, entry_addr, &error);
  CacheAddr child(entry->GetNextAddress());

  Trace("Doom entry 0x%p", entry);

  if (!entry->doomed()) {
    // We may have doomed this entry from within MatchEntry.
    eviction_.OnDoomEntry(entry);
    entry->InternalDoom();
    if (!new_eviction_) {
      DecreaseNumEntries();
    }
    stats_.OnEvent(Stats::DOOM_ENTRY);
  }

  if (parent_entry) {
    parent_entry->SetNextAddress(Addr(child));
    parent_entry->Release();
  } else if (!error) {
    data_->table[hash & mask_] = child;
  }

  FlushIndex();
}

void BackendImplV3::OnEntryDestroyBegin(Addr address) {
  EntriesMap::iterator it = open_entries_.find(address.value());
  if (it != open_entries_.end())
    open_entries_.erase(it);
}

void BackendImplV3::OnEntryDestroyEnd() {
  DecreaseNumRefs();
  if (data_->header.num_bytes > max_size_ && !read_only_ &&
      (up_ticks_ > kTrimDelay || user_flags_ & kNoRandom))
    eviction_.TrimCache(false);
}

EntryImplV3* BackendImplV3::GetOpenEntry(Addr address) const {
  DCHECK(rankings->HasData());
  EntriesMap::const_iterator it =
      open_entries_.find(rankings->Data()->contents);
  if (it != open_entries_.end()) {
    // We have this entry in memory.
    return it->second;
  }

  return NULL;
}

int BackendImplV3::MaxFileSize() const {
  return max_size_ / 8;
}

void BackendImplV3::ModifyStorageSize(int32 old_size, int32 new_size) {
  if (disabled_ || old_size == new_size)
    return;
  if (old_size > new_size)
    SubstractStorageSize(old_size - new_size);
  else
    AddStorageSize(new_size - old_size);

  // Update the usage statistics.
  stats_.ModifyStorageStats(old_size, new_size);
}

void BackendImplV3::TooMuchStorageRequested(int32 size) {
  stats_.ModifyStorageStats(0, size);
}

bool BackendImplV3::IsAllocAllowed(int current_size, int new_size) {
  DCHECK_GT(new_size, current_size);
  if (user_flags_ & NO_BUFFERING)
    return false;

  int to_add = new_size - current_size;
  if (buffer_bytes_ + to_add > MaxBuffersSize())
    return false;

  buffer_bytes_ += to_add;
  CACHE_UMA(COUNTS_50000, "BufferBytes", buffer_bytes_ / 1024);
  return true;
}
#endif  // defined(V3_NOT_JUST_YET_READY).

void BackendImplV3::BufferDeleted(int size) {
  DCHECK_GE(size, 0);
  buffer_bytes_ -= size;
  DCHECK_GE(buffer_bytes_, 0);
}

bool BackendImplV3::IsLoaded() const {
  if (user_flags_ & NO_LOAD_PROTECTION)
    return false;

  return user_load_;
}

std::string BackendImplV3::HistogramName(const char* name) const {
  static const char* names[] = { "Http", "", "Media", "AppCache", "Shader" };
  DCHECK_NE(cache_type_, net::MEMORY_CACHE);
  return base::StringPrintf("DiskCache3.%s_%s", name, names[cache_type_]);
}

base::WeakPtr<BackendImplV3> BackendImplV3::GetWeakPtr() {
  return ptr_factory_.GetWeakPtr();
}

#if defined(V3_NOT_JUST_YET_READY)
// We want to remove biases from some histograms so we only send data once per
// week.
bool BackendImplV3::ShouldReportAgain() {
  if (uma_report_)
    return uma_report_ == 2;

  uma_report_++;
  int64 last_report = stats_.GetCounter(Stats::LAST_REPORT);
  Time last_time = Time::FromInternalValue(last_report);
  if (!last_report || (Time::Now() - last_time).InDays() >= 7) {
    stats_.SetCounter(Stats::LAST_REPORT, Time::Now().ToInternalValue());
    uma_report_++;
    return true;
  }
  return false;
}

void BackendImplV3::FirstEviction() {
  IndexHeaderV3* header = index_.header();
  header->flags |= CACHE_EVICTED;
  DCHECK(header->create_time);
  if (!GetEntryCount())
    return;  // This is just for unit tests.

  Time create_time = Time::FromInternalValue(header->create_time);
  CACHE_UMA(AGE, "FillupAge", create_time);

  int64 use_time = stats_.GetCounter(Stats::TIMER);
  CACHE_UMA(HOURS, "FillupTime", static_cast<int>(use_time / 120));
  CACHE_UMA(PERCENTAGE, "FirstHitRatio", stats_.GetHitRatio());

  if (!use_time)
    use_time = 1;
  CACHE_UMA(COUNTS_10000, "FirstEntryAccessRate",
            static_cast<int>(header->num_entries / use_time));
  CACHE_UMA(COUNTS, "FirstByteIORate",
            static_cast<int>((header->num_bytes / 1024) / use_time));

  int avg_size = header->num_bytes / GetEntryCount();
  CACHE_UMA(COUNTS, "FirstEntrySize", avg_size);

  int large_entries_bytes = stats_.GetLargeEntriesSize();
  int large_ratio = large_entries_bytes * 100 / header->num_bytes;
  CACHE_UMA(PERCENTAGE, "FirstLargeEntriesRatio", large_ratio);

  if (!lru_eviction_) {
    CACHE_UMA(PERCENTAGE, "FirstResurrectRatio", stats_.GetResurrectRatio());
    CACHE_UMA(PERCENTAGE, "FirstNoUseRatio",
              header->num_no_use_entries * 100 / header->num_entries);
    CACHE_UMA(PERCENTAGE, "FirstLowUseRatio",
              header->num_low_use_entries * 100 / header->num_entries);
    CACHE_UMA(PERCENTAGE, "FirstHighUseRatio",
              header->num_high_use_entries * 100 / header->num_entries);
  }

  stats_.ResetRatios();
}

void BackendImplV3::OnEvent(Stats::Counters an_event) {
  stats_.OnEvent(an_event);
}

void BackendImplV3::OnRead(int32 bytes) {
  DCHECK_GE(bytes, 0);
  byte_count_ += bytes;
  if (byte_count_ < 0)
    byte_count_ = kint32max;
}

void BackendImplV3::OnWrite(int32 bytes) {
  // We use the same implementation as OnRead... just log the number of bytes.
  OnRead(bytes);
}

void BackendImplV3::OnTimerTick() {
  stats_.OnEvent(Stats::TIMER);
  int64 time = stats_.GetCounter(Stats::TIMER);
  int64 current = stats_.GetCounter(Stats::OPEN_ENTRIES);

  // OPEN_ENTRIES is a sampled average of the number of open entries, avoiding
  // the bias towards 0.
  if (num_refs_ && (current != num_refs_)) {
    int64 diff = (num_refs_ - current) / 50;
    if (!diff)
      diff = num_refs_ > current ? 1 : -1;
    current = current + diff;
    stats_.SetCounter(Stats::OPEN_ENTRIES, current);
    stats_.SetCounter(Stats::MAX_ENTRIES, max_refs_);
  }

  CACHE_UMA(COUNTS, "NumberOfReferences", num_refs_);

  CACHE_UMA(COUNTS_10000, "EntryAccessRate", entry_count_);
  CACHE_UMA(COUNTS, "ByteIORate", byte_count_ / 1024);

  // These values cover about 99.5% of the population (Oct 2011).
  user_load_ = (entry_count_ > 300 || byte_count_ > 7 * 1024 * 1024);
  entry_count_ = 0;
  byte_count_ = 0;
  up_ticks_++;

  if (!data_)
    first_timer_ = false;
  if (first_timer_) {
    first_timer_ = false;
    if (ShouldReportAgain())
      ReportStats();
  }

  // Save stats to disk at 5 min intervals.
  if (time % 10 == 0)
    StoreStats();
}

void BackendImplV3::SetUnitTestMode() {
  user_flags_ |= UNIT_TEST_MODE;
}

void BackendImplV3::SetUpgradeMode() {
  user_flags_ |= UPGRADE_MODE;
  read_only_ = true;
}

void BackendImplV3::SetNewEviction() {
  user_flags_ |= EVICTION_V2;
  lru_eviction_ = false;
}

void BackendImplV3::SetFlags(uint32 flags) {
  user_flags_ |= flags;
}

int BackendImplV3::FlushQueueForTest(const CompletionCallback& callback) {
  background_queue_.FlushQueue(callback);
  return net::ERR_IO_PENDING;
}

void BackendImplV3::TrimForTest(bool empty) {
  eviction_.SetTestMode();
  eviction_.TrimCache(empty);
}

void BackendImplV3::TrimDeletedListForTest(bool empty) {
  eviction_.SetTestMode();
  eviction_.TrimDeletedList(empty);
}

int BackendImplV3::SelfCheck() {
  if (!init_) {
    LOG(ERROR) << "Init failed";
    return ERR_INIT_FAILED;
  }

  int num_entries = rankings_.SelfCheck();
  if (num_entries < 0) {
    LOG(ERROR) << "Invalid rankings list, error " << num_entries;
#if !defined(NET_BUILD_STRESS_CACHE)
    return num_entries;
#endif
  }

  if (num_entries != data_->header.num_entries) {
    LOG(ERROR) << "Number of entries mismatch";
#if !defined(NET_BUILD_STRESS_CACHE)
    return ERR_NUM_ENTRIES_MISMATCH;
#endif
  }

  return CheckAllEntries();
}

// ------------------------------------------------------------------------

net::CacheType BackendImplV3::GetCacheType() const {
  return cache_type_;
}

int32 BackendImplV3::GetEntryCount() const {
  if (disabled_)
    return 0;
  DCHECK(init_);
  return index_.header()->num_entries;
}

int BackendImplV3::OpenEntry(const std::string& key, Entry** entry,
                             const CompletionCallback& callback) {
  if (disabled_)
    return NULL;

  TimeTicks start = TimeTicks::Now();
  uint32 hash = base::Hash(key);
  Trace("Open hash 0x%x", hash);

  bool error;
  EntryImpl* cache_entry = MatchEntry(key, hash, false, Addr(), &error);
  if (cache_entry && ENTRY_NORMAL != cache_entry->entry()->Data()->state) {
    // The entry was already evicted.
    cache_entry->Release();
    cache_entry = NULL;
  }

  int current_size = data_->header.num_bytes / (1024 * 1024);
  int64 total_hours = stats_.GetCounter(Stats::TIMER) / 120;
  int64 no_use_hours = stats_.GetCounter(Stats::LAST_REPORT_TIMER) / 120;
  int64 use_hours = total_hours - no_use_hours;

  if (!cache_entry) {
    CACHE_UMA(AGE_MS, "OpenTime.Miss", 0, start);
    CACHE_UMA(COUNTS_10000, "AllOpenBySize.Miss", 0, current_size);
    CACHE_UMA(HOURS, "AllOpenByTotalHours.Miss", 0, total_hours);
    CACHE_UMA(HOURS, "AllOpenByUseHours.Miss", 0, use_hours);
    stats_.OnEvent(Stats::OPEN_MISS);
    return NULL;
  }

  eviction_.OnOpenEntry(cache_entry);
  entry_count_++;

  Trace("Open hash 0x%x end: 0x%x", hash,
        cache_entry->entry()->address().value());
  CACHE_UMA(AGE_MS, "OpenTime", 0, start);
  CACHE_UMA(COUNTS_10000, "AllOpenBySize.Hit", 0, current_size);
  CACHE_UMA(HOURS, "AllOpenByTotalHours.Hit", 0, total_hours);
  CACHE_UMA(HOURS, "AllOpenByUseHours.Hit", 0, use_hours);
  stats_.OnEvent(Stats::OPEN_HIT);
  SIMPLE_STATS_COUNTER("disk_cache.hit");
  return cache_entry;
}

int BackendImplV3::CreateEntry(const std::string& key, Entry** entry,
                               const CompletionCallback& callback) {
  if (disabled_ || key.empty())
    return NULL;

  TimeTicks start = TimeTicks::Now();
  Trace("Create hash 0x%x", hash);

  scoped_refptr<EntryImpl> parent;
  Addr entry_address(data_->table[hash & mask_]);
  if (entry_address.is_initialized()) {
    // We have an entry already. It could be the one we are looking for, or just
    // a hash conflict.
    bool error;
    EntryImpl* old_entry = MatchEntry(key, hash, false, Addr(), &error);
    if (old_entry)
      return ResurrectEntry(old_entry);

    EntryImpl* parent_entry = MatchEntry(key, hash, true, Addr(), &error);
    DCHECK(!error);
    if (parent_entry) {
      parent.swap(&parent_entry);
    } else if (data_->table[hash & mask_]) {
      // We should have corrected the problem.
      NOTREACHED();
      return NULL;
    }
  }

  // The general flow is to allocate disk space and initialize the entry data,
  // followed by saving that to disk, then linking the entry though the index
  // and finally through the lists. If there is a crash in this process, we may
  // end up with:
  // a. Used, unreferenced empty blocks on disk (basically just garbage).
  // b. Used, unreferenced but meaningful data on disk (more garbage).
  // c. A fully formed entry, reachable only through the index.
  // d. A fully formed entry, also reachable through the lists, but still dirty.
  //
  // Anything after (b) can be automatically cleaned up. We may consider saving
  // the current operation (as we do while manipulating the lists) so that we
  // can detect and cleanup (a) and (b).

  int num_blocks = EntryImpl::NumBlocksForEntry(key.size());
  if (!block_files_.CreateBlock(BLOCK_256, num_blocks, &entry_address)) {
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  Addr node_address(0);
  if (!block_files_.CreateBlock(RANKINGS, 1, &node_address)) {
    block_files_.DeleteBlock(entry_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  scoped_refptr<EntryImpl> cache_entry(
      new EntryImpl(this, entry_address, false));
  IncreaseNumRefs();

  if (!cache_entry->CreateEntry(node_address, key, hash)) {
    block_files_.DeleteBlock(entry_address, false);
    block_files_.DeleteBlock(node_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  cache_entry->BeginLogging(net_log_, true);

  // We are not failing the operation; let's add this to the map.
  open_entries_[entry_address.value()] = cache_entry.get();

  // Save the entry.
  cache_entry->entry()->Store();
  cache_entry->rankings()->Store();
  IncreaseNumEntries();
  entry_count_++;

  // Link this entry through the index.
  if (parent.get()) {
    parent->SetNextAddress(entry_address);
  } else {
    data_->table[hash & mask_] = entry_address.value();
  }

  // Link this entry through the lists.
  eviction_.OnCreateEntry(cache_entry.get());

  CACHE_UMA(AGE_MS, "CreateTime", 0, start);
  stats_.OnEvent(Stats::CREATE_HIT);
  SIMPLE_STATS_COUNTER("disk_cache.miss");
  Trace("create entry hit ");
  FlushIndex();
  cache_entry->AddRef();
  return cache_entry.get();
}

int BackendImplV3::DoomEntry(const std::string& key,
                             const CompletionCallback& callback) {
  if (disabled_)
    return net::ERR_FAILED;

  EntryImpl* entry = OpenEntryImpl(key);
  if (!entry)
    return net::ERR_FAILED;

  entry->DoomImpl();
  entry->Release();
  return net::OK;
}

int BackendImplV3::DoomAllEntries(const CompletionCallback& callback) {
  // This is not really an error, but it is an interesting condition.
  ReportError(ERR_CACHE_DOOMED);
  stats_.OnEvent(Stats::DOOM_CACHE);
  if (!num_refs_) {
    RestartCache(false);
    return disabled_ ? net::ERR_FAILED : net::OK;
  } else {
    if (disabled_)
      return net::ERR_FAILED;

    eviction_.TrimCache(true);
    return net::OK;
  }
}

int BackendImplV3::DoomEntriesBetween(base::Time initial_time,
                                      base::Time end_time,
                                      const CompletionCallback& callback) {
  DCHECK_NE(net::APP_CACHE, cache_type_);
  if (end_time.is_null())
    return SyncDoomEntriesSince(initial_time);

  DCHECK(end_time >= initial_time);

  if (disabled_)
    return net::ERR_FAILED;

  EntryImpl* node;
  void* iter = NULL;
  EntryImpl* next = OpenNextEntryImpl(&iter);
  if (!next)
    return net::OK;

  while (next) {
    node = next;
    next = OpenNextEntryImpl(&iter);

    if (node->GetLastUsed() >= initial_time &&
        node->GetLastUsed() < end_time) {
      node->DoomImpl();
    } else if (node->GetLastUsed() < initial_time) {
      if (next)
        next->Release();
      next = NULL;
      SyncEndEnumeration(iter);
    }

    node->Release();
  }

  return net::OK;
}

int BackendImplV3::DoomEntriesSince(base::Time initial_time,
                                    const CompletionCallback& callback) {
  DCHECK_NE(net::APP_CACHE, cache_type_);
  if (disabled_)
    return net::ERR_FAILED;

  stats_.OnEvent(Stats::DOOM_RECENT);
  for (;;) {
    void* iter = NULL;
    EntryImpl* entry = OpenNextEntryImpl(&iter);
    if (!entry)
      return net::OK;

    if (initial_time > entry->GetLastUsed()) {
      entry->Release();
      SyncEndEnumeration(iter);
      return net::OK;
    }

    entry->DoomImpl();
    entry->Release();
    SyncEndEnumeration(iter);  // Dooming the entry invalidates the iterator.
  }
}

int BackendImplV3::OpenNextEntry(void** iter, Entry** next_entry,
                                 const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  background_queue_.OpenNextEntry(iter, next_entry, callback);
  return net::ERR_IO_PENDING;
}

void BackendImplV3::EndEnumeration(void** iter) {
  scoped_ptr<IndexIterator> iterator(
      reinterpret_cast<IndexIterator*>(*iter));
  *iter = NULL;
}

void BackendImplV3::GetStats(StatsItems* stats) {
  if (disabled_)
    return;

  std::pair<std::string, std::string> item;

  item.first = "Entries";
  item.second = base::StringPrintf("%d", data_->header.num_entries);
  stats->push_back(item);

  item.first = "Pending IO";
  item.second = base::StringPrintf("%d", num_pending_io_);
  stats->push_back(item);

  item.first = "Max size";
  item.second = base::StringPrintf("%d", max_size_);
  stats->push_back(item);

  item.first = "Current size";
  item.second = base::StringPrintf("%d", data_->header.num_bytes);
  stats->push_back(item);

  item.first = "Cache type";
  item.second = "Blockfile Cache";
  stats->push_back(item);

  stats_.GetItems(stats);
}

void BackendImplV3::OnExternalCacheHit(const std::string& key) {
  if (disabled_)
    return;

  uint32 hash = base::Hash(key);
  bool error;
  EntryImpl* cache_entry = MatchEntry(key, hash, false, Addr(), &error);
  if (cache_entry) {
    if (ENTRY_NORMAL == cache_entry->entry()->Data()->state) {
      UpdateRank(cache_entry, cache_type() == net::SHADER_CACHE);
    }
    cache_entry->Release();
  }
}

// ------------------------------------------------------------------------

// The maximum cache size will be either set explicitly by the caller, or
// calculated by this code.
void BackendImplV3::AdjustMaxCacheSize(int table_len) {
  if (max_size_)
    return;

  // If table_len is provided, the index file exists.
  DCHECK(!table_len || data_->header.magic);

  // The user is not setting the size, let's figure it out.
  int64 available = base::SysInfo::AmountOfFreeDiskSpace(path_);
  if (available < 0) {
    max_size_ = kDefaultCacheSize;
    return;
  }

  if (table_len)
    available += data_->header.num_bytes;

  max_size_ = PreferedCacheSize(available);

  // Let's not use more than the default size while we tune-up the performance
  // of bigger caches. TODO(rvargas): remove this limit.
  if (max_size_ > kDefaultCacheSize * 4)
    max_size_ = kDefaultCacheSize * 4;

  if (!table_len)
    return;

  // If we already have a table, adjust the size to it.
  int current_max_size = MaxStorageSizeForTable(table_len);
  if (max_size_ > current_max_size)
    max_size_= current_max_size;
}

bool BackendImplV3::InitStats() {
  Addr address(data_->header.stats);
  int size = stats_.StorageSize();

  if (!address.is_initialized()) {
    FileType file_type = Addr::RequiredFileType(size);
    DCHECK_NE(file_type, EXTERNAL);
    int num_blocks = Addr::RequiredBlocks(size, file_type);

    if (!CreateBlock(file_type, num_blocks, &address))
      return false;
    return stats_.Init(NULL, 0, address);
  }

  if (!address.is_block_file()) {
    NOTREACHED();
    return false;
  }

  // Load the required data.
  size = address.num_blocks() * address.BlockSize();
  MappedFile* file = File(address);
  if (!file)
    return false;

  scoped_ptr<char[]> data(new char[size]);
  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  if (!file->Read(data.get(), size, offset))
    return false;

  if (!stats_.Init(data.get(), size, address))
    return false;
  if (cache_type_ == net::DISK_CACHE && ShouldReportAgain())
    stats_.InitSizeHistogram();
  return true;
}

void BackendImplV3::StoreStats() {
  int size = stats_.StorageSize();
  scoped_ptr<char[]> data(new char[size]);
  Addr address;
  size = stats_.SerializeStats(data.get(), size, &address);
  DCHECK(size);
  if (!address.is_initialized())
    return;

  MappedFile* file = File(address);
  if (!file)
    return;

  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  file->Write(data.get(), size, offset);  // ignore result.
}

void BackendImplV3::RestartCache(bool failure) {
  int64 errors = stats_.GetCounter(Stats::FATAL_ERROR);
  int64 full_dooms = stats_.GetCounter(Stats::DOOM_CACHE);
  int64 partial_dooms = stats_.GetCounter(Stats::DOOM_RECENT);
  int64 last_report = stats_.GetCounter(Stats::LAST_REPORT);

  PrepareForRestart();
  if (failure) {
    DCHECK(!num_refs_);
    DCHECK(!open_entries_.size());
    DelayedCacheCleanup(path_);
  } else {
    DeleteCache(path_, false);
  }

  // Don't call Init() if directed by the unit test: we are simulating a failure
  // trying to re-enable the cache.
  if (unit_test_)
    init_ = true;  // Let the destructor do proper cleanup.
  else if (SyncInit() == net::OK) {
    stats_.SetCounter(Stats::FATAL_ERROR, errors);
    stats_.SetCounter(Stats::DOOM_CACHE, full_dooms);
    stats_.SetCounter(Stats::DOOM_RECENT, partial_dooms);
    stats_.SetCounter(Stats::LAST_REPORT, last_report);
  }
}

void BackendImplV3::PrepareForRestart() {
  if (!(user_flags_ & EVICTION_V2))
    lru_eviction_ = true;

  disabled_ = true;
  data_->header.crash = 0;
  index_->Flush();
  index_ = NULL;
  data_ = NULL;
  block_files_.CloseFiles();
  rankings_.Reset();
  init_ = false;
  restarted_ = true;
}

void BackendImplV3::CleanupCache() {
  Trace("Backend Cleanup");
  eviction_.Stop();
  timer_.reset();

  if (init_) {
    StoreStats();
    if (data_)
      data_->header.crash = 0;

    if (user_flags_ & kNoRandom) {
      // This is a net_unittest, verify that we are not 'leaking' entries.
      File::WaitForPendingIO(&num_pending_io_);
      DCHECK(!num_refs_);
    } else {
      File::DropPendingIO();
    }
  }
  block_files_.CloseFiles();
  FlushIndex();
  index_ = NULL;
  ptr_factory_.InvalidateWeakPtrs();
  done_.Signal();
}

int BackendImplV3::NewEntry(Addr address, EntryImplV3** entry) {
  EntriesMap::iterator it = open_entries_.find(address.value());
  if (it != open_entries_.end()) {
    // Easy job. This entry is already in memory.
    EntryImpl* this_entry = it->second;
    this_entry->AddRef();
    *entry = this_entry;
    return 0;
  }

  STRESS_DCHECK(block_files_.IsValid(address));

  if (!address.SanityCheckForEntry()) {
    LOG(WARNING) << "Wrong entry address.";
    STRESS_NOTREACHED();
    return ERR_INVALID_ADDRESS;
  }

  scoped_refptr<EntryImpl> cache_entry(
      new EntryImpl(this, address, read_only_));
  IncreaseNumRefs();
  *entry = NULL;

  TimeTicks start = TimeTicks::Now();
  if (!cache_entry->entry()->Load())
    return ERR_READ_FAILURE;

  if (IsLoaded()) {
    CACHE_UMA(AGE_MS, "LoadTime", 0, start);
  }

  if (!cache_entry->SanityCheck()) {
    LOG(WARNING) << "Messed up entry found.";
    STRESS_NOTREACHED();
    return ERR_INVALID_ENTRY;
  }

  STRESS_DCHECK(block_files_.IsValid(
                    Addr(cache_entry->entry()->Data()->rankings_node)));

  if (!cache_entry->LoadNodeAddress())
    return ERR_READ_FAILURE;

  if (!rankings_.SanityCheck(cache_entry->rankings(), false)) {
    STRESS_NOTREACHED();
    cache_entry->SetDirtyFlag(0);
    // Don't remove this from the list (it is not linked properly). Instead,
    // break the link back to the entry because it is going away, and leave the
    // rankings node to be deleted if we find it through a list.
    rankings_.SetContents(cache_entry->rankings(), 0);
  } else if (!rankings_.DataSanityCheck(cache_entry->rankings(), false)) {
    STRESS_NOTREACHED();
    cache_entry->SetDirtyFlag(0);
    rankings_.SetContents(cache_entry->rankings(), address.value());
  }

  if (!cache_entry->DataSanityCheck()) {
    LOG(WARNING) << "Messed up entry found.";
    cache_entry->SetDirtyFlag(0);
    cache_entry->FixForDelete();
  }

  // Prevent overwriting the dirty flag on the destructor.
  cache_entry->SetDirtyFlag(GetCurrentEntryId());

  if (cache_entry->dirty()) {
    Trace("Dirty entry 0x%p 0x%x", reinterpret_cast<void*>(cache_entry.get()),
          address.value());
  }

  open_entries_[address.value()] = cache_entry.get();

  cache_entry->BeginLogging(net_log_, false);
  cache_entry.swap(entry);
  return 0;
}

// This is the actual implementation for OpenNextEntry and OpenPrevEntry.
int BackendImplV3::OpenFollowingEntry(bool forward, void** iter,
                                      Entry** next_entry,
                                      const CompletionCallback& callback) {
  if (disabled_)
    return net::ERR_FAILED;

  DCHECK(iter);

  const int kListsToSearch = 3;
  scoped_refptr<EntryImpl> entries[kListsToSearch];
  scoped_ptr<Rankings::Iterator> iterator(
      reinterpret_cast<Rankings::Iterator*>(*iter));
  *iter = NULL;

  if (!iterator.get()) {
    iterator.reset(new Rankings::Iterator(&rankings_));
    bool ret = false;

    // Get an entry from each list.
    for (int i = 0; i < kListsToSearch; i++) {
      EntryImpl* temp = NULL;
      ret |= OpenFollowingEntryFromList(forward, static_cast<Rankings::List>(i),
                                        &iterator->nodes[i], &temp);
      entries[i].swap(&temp);  // The entry was already addref'd.
    }
    if (!ret)
      return NULL;
  } else {
    // Get the next entry from the last list, and the actual entries for the
    // elements on the other lists.
    for (int i = 0; i < kListsToSearch; i++) {
      EntryImpl* temp = NULL;
      if (iterator->list == i) {
          OpenFollowingEntryFromList(forward, iterator->list,
                                     &iterator->nodes[i], &temp);
      } else {
        temp = GetEnumeratedEntry(iterator->nodes[i],
                                  static_cast<Rankings::List>(i));
      }

      entries[i].swap(&temp);  // The entry was already addref'd.
    }
  }

  int newest = -1;
  int oldest = -1;
  Time access_times[kListsToSearch];
  for (int i = 0; i < kListsToSearch; i++) {
    if (entries[i].get()) {
      access_times[i] = entries[i]->GetLastUsed();
      if (newest < 0) {
        DCHECK_LT(oldest, 0);
        newest = oldest = i;
        continue;
      }
      if (access_times[i] > access_times[newest])
        newest = i;
      if (access_times[i] < access_times[oldest])
        oldest = i;
    }
  }

  if (newest < 0 || oldest < 0)
    return NULL;

  EntryImpl* next_entry;
  if (forward) {
    next_entry = entries[newest].get();
    iterator->list = static_cast<Rankings::List>(newest);
  } else {
    next_entry = entries[oldest].get();
    iterator->list = static_cast<Rankings::List>(oldest);
  }

  *iter = iterator.release();
  next_entry->AddRef();
  return next_entry;
}

void BackendImplV3::AddStorageSize(int32 bytes) {
  data_->header.num_bytes += bytes;
  DCHECK_GE(data_->header.num_bytes, 0);
}

void BackendImplV3::SubstractStorageSize(int32 bytes) {
  data_->header.num_bytes -= bytes;
  DCHECK_GE(data_->header.num_bytes, 0);
}

void BackendImplV3::IncreaseNumRefs() {
  num_refs_++;
  if (max_refs_ < num_refs_)
    max_refs_ = num_refs_;
}

void BackendImplV3::DecreaseNumRefs() {
  DCHECK(num_refs_);
  num_refs_--;

  if (!num_refs_ && disabled_)
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&BackendImpl::RestartCache, GetWeakPtr(), true));
}

void BackendImplV3::IncreaseNumEntries() {
  index_.header()->num_entries++;
  DCHECK_GT(index_.header()->num_entries, 0);
}

void BackendImplV3::DecreaseNumEntries() {
  index_.header()->num_entries--;
  if (index_.header()->num_entries < 0) {
    NOTREACHED();
    index_.header()->num_entries = 0;
  }
}

int BackendImplV3::SyncInit() {
#if defined(NET_BUILD_STRESS_CACHE)
  // Start evictions right away.
  up_ticks_ = kTrimDelay * 2;
#endif
  DCHECK(!init_);
  if (init_)
    return net::ERR_FAILED;

  bool create_files = false;
  if (!InitBackingStore(&create_files)) {
    ReportError(ERR_STORAGE_ERROR);
    return net::ERR_FAILED;
  }

  num_refs_ = num_pending_io_ = max_refs_ = 0;
  entry_count_ = byte_count_ = 0;

  if (!restarted_) {
    buffer_bytes_ = 0;
    trace_object_ = TraceObject::GetTraceObject();
    // Create a recurrent timer of 30 secs.
    int timer_delay = unit_test_ ? 1000 : 30000;
    timer_.reset(new base::RepeatingTimer<BackendImplV3>());
    timer_->Start(FROM_HERE, TimeDelta::FromMilliseconds(timer_delay), this,
                  &BackendImplV3::OnStatsTimer);
  }

  init_ = true;
  Trace("Init");

  if (data_->header.experiment != NO_EXPERIMENT &&
      cache_type_ != net::DISK_CACHE) {
    // No experiment for other caches.
    return net::ERR_FAILED;
  }

  if (!(user_flags_ & kNoRandom)) {
    // The unit test controls directly what to test.
    new_eviction_ = (cache_type_ == net::DISK_CACHE);
  }

  if (!CheckIndex()) {
    ReportError(ERR_INIT_FAILED);
    return net::ERR_FAILED;
  }

  if (!restarted_ && (create_files || !data_->header.num_entries))
    ReportError(ERR_CACHE_CREATED);

  if (!(user_flags_ & kNoRandom) && cache_type_ == net::DISK_CACHE &&
      !InitExperiment(&data_->header, create_files)) {
    return net::ERR_FAILED;
  }

  // We don't care if the value overflows. The only thing we care about is that
  // the id cannot be zero, because that value is used as "not dirty".
  // Increasing the value once per second gives us many years before we start
  // having collisions.
  data_->header.this_id++;
  if (!data_->header.this_id)
    data_->header.this_id++;

  bool previous_crash = (data_->header.crash != 0);
  data_->header.crash = 1;

  if (!block_files_.Init(create_files))
    return net::ERR_FAILED;

  // We want to minimize the changes to cache for an AppCache.
  if (cache_type() == net::APP_CACHE) {
    DCHECK(!new_eviction_);
    read_only_ = true;
  } else if (cache_type() == net::SHADER_CACHE) {
    DCHECK(!new_eviction_);
  }

  eviction_.Init(this);

  // stats_ and rankings_ may end up calling back to us so we better be enabled.
  disabled_ = false;
  if (!InitStats())
    return net::ERR_FAILED;

  disabled_ = !rankings_.Init(this, new_eviction_);

#if defined(STRESS_CACHE_EXTENDED_VALIDATION)
  trace_object_->EnableTracing(false);
  int sc = SelfCheck();
  if (sc < 0 && sc != ERR_NUM_ENTRIES_MISMATCH)
    NOTREACHED();
  trace_object_->EnableTracing(true);
#endif

  if (previous_crash) {
    ReportError(ERR_PREVIOUS_CRASH);
  } else if (!restarted_) {
    ReportError(ERR_NO_ERROR);
  }

  FlushIndex();

  return disabled_ ? net::ERR_FAILED : net::OK;
}

EntryImpl* BackendImplV3::ResurrectEntry(EntryImpl* deleted_entry) {
  if (ENTRY_NORMAL == deleted_entry->entry()->Data()->state) {
    deleted_entry->Release();
    stats_.OnEvent(Stats::CREATE_MISS);
    Trace("create entry miss ");
    return NULL;
  }

  // We are attempting to create an entry and found out that the entry was
  // previously deleted.

  eviction_.OnCreateEntry(deleted_entry);
  entry_count_++;

  stats_.OnEvent(Stats::RESURRECT_HIT);
  Trace("Resurrect entry hit ");
  return deleted_entry;
}

EntryImpl* BackendImplV3::CreateEntryImpl(const std::string& key) {
  if (disabled_ || key.empty())
    return NULL;

  TimeTicks start = TimeTicks::Now();
  Trace("Create hash 0x%x", hash);

  scoped_refptr<EntryImpl> parent;
  Addr entry_address(data_->table[hash & mask_]);
  if (entry_address.is_initialized()) {
    // We have an entry already. It could be the one we are looking for, or just
    // a hash conflict.
    bool error;
    EntryImpl* old_entry = MatchEntry(key, hash, false, Addr(), &error);
    if (old_entry)
      return ResurrectEntry(old_entry);

    EntryImpl* parent_entry = MatchEntry(key, hash, true, Addr(), &error);
    DCHECK(!error);
    if (parent_entry) {
      parent.swap(&parent_entry);
    } else if (data_->table[hash & mask_]) {
      // We should have corrected the problem.
      NOTREACHED();
      return NULL;
    }
  }

  // The general flow is to allocate disk space and initialize the entry data,
  // followed by saving that to disk, then linking the entry though the index
  // and finally through the lists. If there is a crash in this process, we may
  // end up with:
  // a. Used, unreferenced empty blocks on disk (basically just garbage).
  // b. Used, unreferenced but meaningful data on disk (more garbage).
  // c. A fully formed entry, reachable only through the index.
  // d. A fully formed entry, also reachable through the lists, but still dirty.
  //
  // Anything after (b) can be automatically cleaned up. We may consider saving
  // the current operation (as we do while manipulating the lists) so that we
  // can detect and cleanup (a) and (b).

  int num_blocks = EntryImpl::NumBlocksForEntry(key.size());
  if (!block_files_.CreateBlock(BLOCK_256, num_blocks, &entry_address)) {
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  Addr node_address(0);
  if (!block_files_.CreateBlock(RANKINGS, 1, &node_address)) {
    block_files_.DeleteBlock(entry_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  scoped_refptr<EntryImpl> cache_entry(
      new EntryImpl(this, entry_address, false));
  IncreaseNumRefs();

  if (!cache_entry->CreateEntry(node_address, key, hash)) {
    block_files_.DeleteBlock(entry_address, false);
    block_files_.DeleteBlock(node_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  cache_entry->BeginLogging(net_log_, true);

  // We are not failing the operation; let's add this to the map.
  open_entries_[entry_address.value()] = cache_entry;

  // Save the entry.
  cache_entry->entry()->Store();
  cache_entry->rankings()->Store();
  IncreaseNumEntries();
  entry_count_++;

  // Link this entry through the index.
  if (parent.get()) {
    parent->SetNextAddress(entry_address);
  } else {
    data_->table[hash & mask_] = entry_address.value();
  }

  // Link this entry through the lists.
  eviction_.OnCreateEntry(cache_entry);

  CACHE_UMA(AGE_MS, "CreateTime", 0, start);
  stats_.OnEvent(Stats::CREATE_HIT);
  SIMPLE_STATS_COUNTER("disk_cache.miss");
  Trace("create entry hit ");
  FlushIndex();
  cache_entry->AddRef();
  return cache_entry.get();
}

void BackendImplV3::LogStats() {
  StatsItems stats;
  GetStats(&stats);

  for (size_t index = 0; index < stats.size(); index++)
    VLOG(1) << stats[index].first << ": " << stats[index].second;
}

void BackendImplV3::ReportStats() {
  IndexHeaderV3* header = index_.header();
  CACHE_UMA(COUNTS, "Entries", header->num_entries);

  int current_size = header->num_bytes / (1024 * 1024);
  int max_size = max_size_ / (1024 * 1024);

  CACHE_UMA(COUNTS_10000, "Size", current_size);
  CACHE_UMA(COUNTS_10000, "MaxSize", max_size);
  if (!max_size)
    max_size++;
  CACHE_UMA(PERCENTAGE, "UsedSpace", current_size * 100 / max_size);

  CACHE_UMA(COUNTS_10000, "AverageOpenEntries",
            static_cast<int>(stats_.GetCounter(Stats::OPEN_ENTRIES)));
  CACHE_UMA(COUNTS_10000, "MaxOpenEntries",
            static_cast<int>(stats_.GetCounter(Stats::MAX_ENTRIES)));
  stats_.SetCounter(Stats::MAX_ENTRIES, 0);

  CACHE_UMA(COUNTS_10000, "TotalFatalErrors",
            static_cast<int>(stats_.GetCounter(Stats::FATAL_ERROR)));
  CACHE_UMA(COUNTS_10000, "TotalDoomCache",
            static_cast<int>(stats_.GetCounter(Stats::DOOM_CACHE)));
  CACHE_UMA(COUNTS_10000, "TotalDoomRecentEntries",
            static_cast<int>(stats_.GetCounter(Stats::DOOM_RECENT)));
  stats_.SetCounter(Stats::FATAL_ERROR, 0);
  stats_.SetCounter(Stats::DOOM_CACHE, 0);
  stats_.SetCounter(Stats::DOOM_RECENT, 0);

  int64 total_hours = stats_.GetCounter(Stats::TIMER) / 120;
  if (!(header->flags & CACHE_EVICTED)) {
    CACHE_UMA(HOURS, "TotalTimeNotFull", static_cast<int>(total_hours));
    return;
  }

  // This is an up to date client that will report FirstEviction() data. After
  // that event, start reporting this:

  CACHE_UMA(HOURS, "TotalTime", static_cast<int>(total_hours));

  int64 use_hours = stats_.GetCounter(Stats::LAST_REPORT_TIMER) / 120;
  stats_.SetCounter(Stats::LAST_REPORT_TIMER, stats_.GetCounter(Stats::TIMER));

  // We may see users with no use_hours at this point if this is the first time
  // we are running this code.
  if (use_hours)
    use_hours = total_hours - use_hours;

  if (!use_hours || !GetEntryCount() || !header->num_bytes)
    return;

  CACHE_UMA(HOURS, "UseTime", static_cast<int>(use_hours));

  int64 trim_rate = stats_.GetCounter(Stats::TRIM_ENTRY) / use_hours;
  CACHE_UMA(COUNTS, "TrimRate", static_cast<int>(trim_rate));

  int avg_size = header->num_bytes / GetEntryCount();
  CACHE_UMA(COUNTS, "EntrySize", avg_size);
  CACHE_UMA(COUNTS, "EntriesFull", header->num_entries);

  int large_entries_bytes = stats_.GetLargeEntriesSize();
  int large_ratio = large_entries_bytes * 100 / header->num_bytes;
  CACHE_UMA(PERCENTAGE, "LargeEntriesRatio", large_ratio);

  if (!lru_eviction_) {
    CACHE_UMA(PERCENTAGE, "ResurrectRatio", stats_.GetResurrectRatio());
    CACHE_UMA(PERCENTAGE, "NoUseRatio",
              header->num_no_use_entries * 100 / header->num_entries);
    CACHE_UMA(PERCENTAGE, "LowUseRatio",
              header->num_low_use_entries * 100 / header->num_entries);
    CACHE_UMA(PERCENTAGE, "HighUseRatio",
              header->num_high_use_entries * 100 / header->num_entries);
    CACHE_UMA(PERCENTAGE, "DeletedRatio",
              header->num_evicted_entries * 100 / header->num_entries);
  }

  stats_.ResetRatios();
  stats_.SetCounter(Stats::TRIM_ENTRY, 0);

  if (cache_type_ == net::DISK_CACHE)
    block_files_.ReportStats();
}

void BackendImplV3::ReportError(int error) {
  STRESS_DCHECK(!error || error == ERR_PREVIOUS_CRASH ||
                error == ERR_CACHE_CREATED);

  // We transmit positive numbers, instead of direct error codes.
  DCHECK_LE(error, 0);
  CACHE_UMA(CACHE_ERROR, "Error", error * -1);
}

bool BackendImplV3::CheckIndex() {
  DCHECK(data_);

  size_t current_size = index_->GetLength();
  if (current_size < sizeof(Index)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  if (new_eviction_) {
    // We support versions 2.0 and 2.1, upgrading 2.0 to 2.1.
    if (kIndexMagic != data_->header.magic ||
        kCurrentVersion >> 16 != data_->header.version >> 16) {
      LOG(ERROR) << "Invalid file version or magic";
      return false;
    }
    if (kCurrentVersion == data_->header.version) {
      // We need file version 2.1 for the new eviction algorithm.
      UpgradeTo2_1();
    }
  } else {
    if (kIndexMagic != data_->header.magic ||
        kCurrentVersion != data_->header.version) {
      LOG(ERROR) << "Invalid file version or magic";
      return false;
    }
  }

  if (!data_->header.table_len) {
    LOG(ERROR) << "Invalid table size";
    return false;
  }

  if (current_size < GetIndexSize(data_->header.table_len) ||
      data_->header.table_len & (kBaseTableLen - 1)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  AdjustMaxCacheSize(data_->header.table_len);

#if !defined(NET_BUILD_STRESS_CACHE)
  if (data_->header.num_bytes < 0 ||
      (max_size_ < kint32max - kDefaultCacheSize &&
       data_->header.num_bytes > max_size_ + kDefaultCacheSize)) {
    LOG(ERROR) << "Invalid cache (current) size";
    return false;
  }
#endif

  if (data_->header.num_entries < 0) {
    LOG(ERROR) << "Invalid number of entries";
    return false;
  }

  if (!mask_)
    mask_ = data_->header.table_len - 1;

  // Load the table into memory with a single read.
  scoped_ptr<char[]> buf(new char[current_size]);
  return index_->Read(buf.get(), current_size, 0);
}

int BackendImplV3::CheckAllEntries() {
  int num_dirty = 0;
  int num_entries = 0;
  DCHECK(mask_ < kuint32max);
  for (unsigned int i = 0; i <= mask_; i++) {
    Addr address(data_->table[i]);
    if (!address.is_initialized())
      continue;
    for (;;) {
      EntryImpl* tmp;
      int ret = NewEntry(address, &tmp);
      if (ret) {
        STRESS_NOTREACHED();
        return ret;
      }
      scoped_refptr<EntryImpl> cache_entry;
      cache_entry.swap(&tmp);

      if (cache_entry->dirty())
        num_dirty++;
      else if (CheckEntry(cache_entry.get()))
        num_entries++;
      else
        return ERR_INVALID_ENTRY;

      DCHECK_EQ(i, cache_entry->entry()->Data()->hash & mask_);
      address.set_value(cache_entry->GetNextAddress());
      if (!address.is_initialized())
        break;
    }
  }

  Trace("CheckAllEntries End");
  if (num_entries + num_dirty != data_->header.num_entries) {
    LOG(ERROR) << "Number of entries " << num_entries << " " << num_dirty <<
                  " " << data_->header.num_entries;
    DCHECK_LT(num_entries, data_->header.num_entries);
    return ERR_NUM_ENTRIES_MISMATCH;
  }

  return num_dirty;
}

bool BackendImplV3::CheckEntry(EntryImpl* cache_entry) {
  bool ok = block_files_.IsValid(cache_entry->entry()->address());
  ok = ok && block_files_.IsValid(cache_entry->rankings()->address());
  EntryStore* data = cache_entry->entry()->Data();
  for (size_t i = 0; i < arraysize(data->data_addr); i++) {
    if (data->data_addr[i]) {
      Addr address(data->data_addr[i]);
      if (address.is_block_file())
        ok = ok && block_files_.IsValid(address);
    }
  }

  return ok && cache_entry->rankings()->VerifyHash();
}

int BackendImplV3::MaxBuffersSize() {
  static int64 total_memory = base::SysInfo::AmountOfPhysicalMemory();
  static bool done = false;

  if (!done) {
    const int kMaxBuffersSize = 30 * 1024 * 1024;

    // We want to use up to 2% of the computer's memory.
    total_memory = total_memory * 2 / 100;
    if (total_memory > kMaxBuffersSize || total_memory <= 0)
      total_memory = kMaxBuffersSize;

    done = true;
  }

  return static_cast<int>(total_memory);
}

#endif  // defined(V3_NOT_JUST_YET_READY).

bool BackendImplV3::IsAllocAllowed(int current_size, int new_size) {
  return false;
}

net::CacheType BackendImplV3::GetCacheType() const {
  return cache_type_;
}

int32 BackendImplV3::GetEntryCount() const {
  return 0;
}

int BackendImplV3::OpenEntry(const std::string& key, Entry** entry,
                             const CompletionCallback& callback) {
  return net::ERR_FAILED;
}

int BackendImplV3::CreateEntry(const std::string& key, Entry** entry,
                               const CompletionCallback& callback) {
  return net::ERR_FAILED;
}

int BackendImplV3::DoomEntry(const std::string& key,
                             const CompletionCallback& callback) {
  return net::ERR_FAILED;
}

int BackendImplV3::DoomAllEntries(const CompletionCallback& callback) {
  return net::ERR_FAILED;
}

int BackendImplV3::DoomEntriesBetween(base::Time initial_time,
                                      base::Time end_time,
                                      const CompletionCallback& callback) {
  return net::ERR_FAILED;
}

int BackendImplV3::DoomEntriesSince(base::Time initial_time,
                                    const CompletionCallback& callback) {
  return net::ERR_FAILED;
}

int BackendImplV3::OpenNextEntry(void** iter, Entry** next_entry,
                                 const CompletionCallback& callback) {
  return net::ERR_FAILED;
}

void BackendImplV3::EndEnumeration(void** iter) {
  NOTIMPLEMENTED();
}

void BackendImplV3::GetStats(StatsItems* stats) {
  NOTIMPLEMENTED();
}

void BackendImplV3::OnExternalCacheHit(const std::string& key) {
  NOTIMPLEMENTED();
}

void BackendImplV3::CleanupCache() {
}

}  // namespace disk_cache
