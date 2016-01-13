// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/backend_worker_v3.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/blockfile/errors.h"
#include "net/disk_cache/blockfile/experiments.h"
#include "net/disk_cache/blockfile/file.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace {

#if defined(V3_NOT_JUST_YET_READY)

const char* kIndexName = "index";

// Seems like ~240 MB correspond to less than 50k entries for 99% of the people.
// Note that the actual target is to keep the index table load factor under 55%
// for most users.
const int k64kEntriesStore = 240 * 1000 * 1000;
const int kBaseTableLen = 64 * 1024;
const int kDefaultCacheSize = 80 * 1024 * 1024;

// Avoid trimming the cache for the first 5 minutes (10 timer ticks).
const int kTrimDelay = 10;

int DesiredIndexTableLen(int32 storage_size) {
  if (storage_size <= k64kEntriesStore)
    return kBaseTableLen;
  if (storage_size <= k64kEntriesStore * 2)
    return kBaseTableLen * 2;
  if (storage_size <= k64kEntriesStore * 4)
    return kBaseTableLen * 4;
  if (storage_size <= k64kEntriesStore * 8)
    return kBaseTableLen * 8;

  // The biggest storage_size for int32 requires a 4 MB table.
  return kBaseTableLen * 16;
}

int MaxStorageSizeForTable(int table_len) {
  return table_len * (k64kEntriesStore / kBaseTableLen);
}

size_t GetIndexSize(int table_len) {
  size_t table_size = sizeof(disk_cache::CacheAddr) * table_len;
  return sizeof(disk_cache::IndexHeader) + table_size;
}

// ------------------------------------------------------------------------

// Sets group for the current experiment. Returns false if the files should be
// discarded.
bool InitExperiment(disk_cache::IndexHeader* header, bool cache_created) {
  if (header->experiment == disk_cache::EXPERIMENT_OLD_FILE1 ||
      header->experiment == disk_cache::EXPERIMENT_OLD_FILE2) {
    // Discard current cache.
    return false;
  }

  if (base::FieldTrialList::FindFullName("SimpleCacheTrial") ==
          "ExperimentControl") {
    if (cache_created) {
      header->experiment = disk_cache::EXPERIMENT_SIMPLE_CONTROL;
      return true;
    } else if (header->experiment != disk_cache::EXPERIMENT_SIMPLE_CONTROL) {
      return false;
    }
  }

  header->experiment = disk_cache::NO_EXPERIMENT;
  return true;
}
#endif  // defined(V3_NOT_JUST_YET_READY).

}  // namespace

// ------------------------------------------------------------------------

namespace disk_cache {

BackendImplV3::Worker::Worker(const base::FilePath& path,
                              base::MessageLoopProxy* main_thread)
      : path_(path),
        block_files_(path),
        init_(false) {
}

#if defined(V3_NOT_JUST_YET_READY)

int BackendImpl::SyncInit() {
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
    timer_.reset(new base::RepeatingTimer<BackendImpl>());
    timer_->Start(FROM_HERE, TimeDelta::FromMilliseconds(timer_delay), this,
                  &BackendImpl::OnStatsTimer);
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

void BackendImpl::PrepareForRestart() {
  // Reset the mask_ if it was not given by the user.
  if (!(user_flags_ & kMask))
    mask_ = 0;

  if (!(user_flags_ & kNewEviction))
    new_eviction_ = false;

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

BackendImpl::~BackendImpl() {
  if (user_flags_ & kNoRandom) {
    // This is a unit test, so we want to be strict about not leaking entries
    // and completing all the work.
    background_queue_.WaitForPendingIO();
  } else {
    // This is most likely not a test, so we want to do as little work as
    // possible at this time, at the price of leaving dirty entries behind.
    background_queue_.DropPendingIO();
  }

  if (background_queue_.BackgroundIsCurrentThread()) {
    // Unit tests may use the same thread for everything.
    CleanupCache();
  } else {
    background_queue_.background_thread()->PostTask(
        FROM_HERE, base::Bind(&FinalCleanupCallback, base::Unretained(this)));
    // http://crbug.com/74623
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    done_.Wait();
  }
}

void BackendImpl::CleanupCache() {
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

base::FilePath BackendImpl::GetFileName(Addr address) const {
  if (!address.is_separate_file() || !address.is_initialized()) {
    NOTREACHED();
    return base::FilePath();
  }

  std::string tmp = base::StringPrintf("f_%06x", address.FileNumber());
  return path_.AppendASCII(tmp);
}

// We just created a new file so we're going to write the header and set the
// file length to include the hash table (zero filled).
bool BackendImpl::CreateBackingStore(disk_cache::File* file) {
  AdjustMaxCacheSize(0);

  IndexHeader header;
  header.table_len = DesiredIndexTableLen(max_size_);

  // We need file version 2.1 for the new eviction algorithm.
  if (new_eviction_)
    header.version = 0x20001;

  header.create_time = Time::Now().ToInternalValue();

  if (!file->Write(&header, sizeof(header), 0))
    return false;

  return file->SetLength(GetIndexSize(header.table_len));
}

bool BackendImpl::InitBackingStore(bool* file_created) {
  if (!base::CreateDirectory(path_))
    return false;

  base::FilePath index_name = path_.AppendASCII(kIndexName);

  int flags = base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_OPEN_ALWAYS |
              base::PLATFORM_FILE_EXCLUSIVE_WRITE;
  scoped_refptr<disk_cache::File> file(new disk_cache::File(
      base::CreatePlatformFile(index_name, flags, file_created, NULL)));

  if (!file->IsValid())
    return false;

  bool ret = true;
  if (*file_created)
    ret = CreateBackingStore(file.get());

  file = NULL;
  if (!ret)
    return false;

  index_ = new MappedFile();
  data_ = reinterpret_cast<Index*>(index_->Init(index_name, 0));
  if (!data_) {
    LOG(ERROR) << "Unable to map Index file";
    return false;
  }

  if (index_->GetLength() < sizeof(Index)) {
    // We verify this again on CheckIndex() but it's easier to make sure now
    // that the header is there.
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  return true;
}

void BackendImpl::ReportError(int error) {
  STRESS_DCHECK(!error || error == ERR_PREVIOUS_CRASH ||
                error == ERR_CACHE_CREATED);

  // We transmit positive numbers, instead of direct error codes.
  DCHECK_LE(error, 0);
  CACHE_UMA(CACHE_ERROR, "Error", 0, error * -1);
}


bool BackendImpl::CheckIndex() {
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

bool BackendImpl::InitStats() {
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

#endif  // defined(V3_NOT_JUST_YET_READY).

int BackendImplV3::Worker::Init(const CompletionCallback& callback) {
  return net::ERR_FAILED;
}

BackendImplV3::Worker::~Worker() {
}

}  // namespace disk_cache
