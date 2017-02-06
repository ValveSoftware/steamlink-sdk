// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_area.h"

#include <inttypes.h>

#include <algorithm>
#include <cctype>  // for std::isalnum

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/process/process_info.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/local_storage_database_adapter.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/browser/dom_storage/session_storage_database_adapter.h"
#include "content/browser/leveldb_wrapper_impl.h"
#include "content/common/dom_storage/dom_storage_map.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/database/database_util.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/fileapi/file_system_util.h"

using storage::DatabaseUtil;

namespace content {

namespace {

// Delay for a moment after a value is set in anticipation
// of other values being set, so changes are batched.
const int kCommitDefaultDelaySecs = 5;

// To avoid excessive IO we apply limits to the amount of data being written
// and the frequency of writes. The specific values used are somewhat arbitrary.
const int kMaxBytesPerHour = kPerStorageAreaQuota;
const int kMaxCommitsPerHour = 60;

}  // namespace

bool DOMStorageArea::s_aggressive_flushing_enabled_ = false;

DOMStorageArea::RateLimiter::RateLimiter(size_t desired_rate,
                                         base::TimeDelta time_quantum)
    : rate_(desired_rate), samples_(0), time_quantum_(time_quantum) {
  DCHECK_GT(desired_rate, 0ul);
}

base::TimeDelta DOMStorageArea::RateLimiter::ComputeTimeNeeded() const {
  return time_quantum_ * (samples_ / rate_);
}

base::TimeDelta DOMStorageArea::RateLimiter::ComputeDelayNeeded(
    const base::TimeDelta elapsed_time) const {
  base::TimeDelta time_needed = ComputeTimeNeeded();
  if (time_needed > elapsed_time)
    return time_needed - elapsed_time;
  return base::TimeDelta();
}

DOMStorageArea::CommitBatch::CommitBatch() : clear_all_first(false) {
}
DOMStorageArea::CommitBatch::~CommitBatch() {}

size_t DOMStorageArea::CommitBatch::GetDataSize() const {
  return DOMStorageMap::CountBytes(changed_values);
}

// static
const base::FilePath::CharType DOMStorageArea::kDatabaseFileExtension[] =
    FILE_PATH_LITERAL(".localstorage");

// static
base::FilePath DOMStorageArea::DatabaseFileNameFromOrigin(const GURL& origin) {
  std::string filename = storage::GetIdentifierFromOrigin(origin);
  // There is no base::FilePath.AppendExtension() method, so start with just the
  // extension as the filename, and then InsertBeforeExtension the desired
  // name.
  return base::FilePath().Append(kDatabaseFileExtension).
      InsertBeforeExtensionASCII(filename);
}

// static
GURL DOMStorageArea::OriginFromDatabaseFileName(const base::FilePath& name) {
  DCHECK(name.MatchesExtension(kDatabaseFileExtension));
  std::string origin_id =
      name.BaseName().RemoveExtension().MaybeAsASCII();
  return storage::GetOriginFromIdentifier(origin_id);
}

void DOMStorageArea::EnableAggressiveCommitDelay() {
  s_aggressive_flushing_enabled_ = true;
  LevelDBWrapperImpl::EnableAggressiveCommitDelay();
}

DOMStorageArea::DOMStorageArea(const GURL& origin,
                               const base::FilePath& directory,
                               DOMStorageTaskRunner* task_runner)
    : namespace_id_(kLocalStorageNamespaceId),
      origin_(origin),
      directory_(directory),
      task_runner_(task_runner),
      map_(new DOMStorageMap(kPerStorageAreaQuota +
                             kPerStorageAreaOverQuotaAllowance)),
      is_initial_import_done_(true),
      is_shutdown_(false),
      commit_batches_in_flight_(0),
      start_time_(base::TimeTicks::Now()),
      data_rate_limiter_(kMaxBytesPerHour, base::TimeDelta::FromHours(1)),
      commit_rate_limiter_(kMaxCommitsPerHour, base::TimeDelta::FromHours(1)) {
  if (!directory.empty()) {
    base::FilePath path = directory.Append(DatabaseFileNameFromOrigin(origin_));
    backing_.reset(new LocalStorageDatabaseAdapter(path));
    is_initial_import_done_ = false;
  }
}

DOMStorageArea::DOMStorageArea(int64_t namespace_id,
                               const std::string& persistent_namespace_id,
                               const GURL& origin,
                               SessionStorageDatabase* session_storage_backing,
                               DOMStorageTaskRunner* task_runner)
    : namespace_id_(namespace_id),
      persistent_namespace_id_(persistent_namespace_id),
      origin_(origin),
      task_runner_(task_runner),
      map_(new DOMStorageMap(kPerStorageAreaQuota +
                             kPerStorageAreaOverQuotaAllowance)),
      session_storage_backing_(session_storage_backing),
      is_initial_import_done_(true),
      is_shutdown_(false),
      commit_batches_in_flight_(0),
      start_time_(base::TimeTicks::Now()),
      data_rate_limiter_(kMaxBytesPerHour, base::TimeDelta::FromHours(1)),
      commit_rate_limiter_(kMaxCommitsPerHour, base::TimeDelta::FromHours(1)) {
  DCHECK(namespace_id != kLocalStorageNamespaceId);
  if (session_storage_backing) {
    backing_.reset(new SessionStorageDatabaseAdapter(
        session_storage_backing, persistent_namespace_id, origin));
    is_initial_import_done_ = false;
  }
}

DOMStorageArea::~DOMStorageArea() {
}

void DOMStorageArea::ExtractValues(DOMStorageValuesMap* map) {
  if (is_shutdown_)
    return;
  InitialImportIfNeeded();
  map_->ExtractValues(map);
}

unsigned DOMStorageArea::Length() {
  if (is_shutdown_)
    return 0;
  InitialImportIfNeeded();
  return map_->Length();
}

base::NullableString16 DOMStorageArea::Key(unsigned index) {
  if (is_shutdown_)
    return base::NullableString16();
  InitialImportIfNeeded();
  return map_->Key(index);
}

base::NullableString16 DOMStorageArea::GetItem(const base::string16& key) {
  if (is_shutdown_)
    return base::NullableString16();
  InitialImportIfNeeded();
  return map_->GetItem(key);
}

bool DOMStorageArea::SetItem(const base::string16& key,
                             const base::string16& value,
                             base::NullableString16* old_value) {
  if (is_shutdown_)
    return false;
  InitialImportIfNeeded();
  if (!map_->HasOneRef())
    map_ = map_->DeepCopy();
  bool success = map_->SetItem(key, value, old_value);
  if (success && backing_ &&
      (old_value->is_null() || old_value->string() != value)) {
    CommitBatch* commit_batch = CreateCommitBatchIfNeeded();
    commit_batch->changed_values[key] = base::NullableString16(value, false);
  }
  return success;
}

bool DOMStorageArea::RemoveItem(const base::string16& key,
                                base::string16* old_value) {
  if (is_shutdown_)
    return false;
  InitialImportIfNeeded();
  if (!map_->HasOneRef())
    map_ = map_->DeepCopy();
  bool success = map_->RemoveItem(key, old_value);
  if (success && backing_) {
    CommitBatch* commit_batch = CreateCommitBatchIfNeeded();
    commit_batch->changed_values[key] = base::NullableString16();
  }
  return success;
}

bool DOMStorageArea::Clear() {
  if (is_shutdown_)
    return false;
  InitialImportIfNeeded();
  if (map_->Length() == 0)
    return false;

  map_ = new DOMStorageMap(kPerStorageAreaQuota +
                           kPerStorageAreaOverQuotaAllowance);

  if (backing_) {
    CommitBatch* commit_batch = CreateCommitBatchIfNeeded();
    commit_batch->clear_all_first = true;
    commit_batch->changed_values.clear();
  }

  return true;
}

void DOMStorageArea::FastClear() {
  // TODO(marja): Unify clearing localStorage and sessionStorage. The problem is
  // to make the following 3 to work together: 1) FastClear, 2) PurgeMemory and
  // 3) not creating events when clearing an empty area.
  if (is_shutdown_)
    return;

  map_ = new DOMStorageMap(kPerStorageAreaQuota +
                           kPerStorageAreaOverQuotaAllowance);
  // This ensures no import will happen while we're waiting to clear the data
  // from the database. This mechanism fails if PurgeMemory is called.
  is_initial_import_done_ = true;

  if (backing_) {
    CommitBatch* commit_batch = CreateCommitBatchIfNeeded();
    commit_batch->clear_all_first = true;
    commit_batch->changed_values.clear();
  }
}

DOMStorageArea* DOMStorageArea::ShallowCopy(
    int64_t destination_namespace_id,
    const std::string& destination_persistent_namespace_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, destination_namespace_id);

  DOMStorageArea* copy = new DOMStorageArea(
      destination_namespace_id, destination_persistent_namespace_id, origin_,
      session_storage_backing_.get(), task_runner_.get());
  copy->map_ = map_;
  copy->is_shutdown_ = is_shutdown_;
  copy->is_initial_import_done_ = true;

  // All the uncommitted changes to this area need to happen before the actual
  // shallow copy is made (scheduled by the upper layer sometime after return).
  if (commit_batch_)
    ScheduleImmediateCommit();
  return copy;
}

bool DOMStorageArea::HasUncommittedChanges() const {
  return commit_batch_.get() || commit_batches_in_flight_;
}

void DOMStorageArea::ScheduleImmediateCommit() {
  DCHECK(HasUncommittedChanges());
  PostCommitTask();
}

void DOMStorageArea::DeleteOrigin() {
  DCHECK(!is_shutdown_);
  // This function shouldn't be called for sessionStorage.
  DCHECK(!session_storage_backing_.get());
  if (HasUncommittedChanges()) {
    // TODO(michaeln): This logically deletes the data immediately,
    // and in a matter of a second, deletes the rows from the backing
    // database file, but the file itself will linger until shutdown
    // or purge time. Ideally, this should delete the file more
    // quickly.
    Clear();
    return;
  }
  map_ = new DOMStorageMap(kPerStorageAreaQuota +
                           kPerStorageAreaOverQuotaAllowance);
  if (backing_) {
    is_initial_import_done_ = false;
    backing_->Reset();
    backing_->DeleteFiles();
  }
}

void DOMStorageArea::PurgeMemory() {
  DCHECK(!is_shutdown_);
  // Purging sessionStorage is not supported; it won't work with FastClear.
  DCHECK(!session_storage_backing_.get());
  if (!is_initial_import_done_ ||  // We're not using any memory.
      !backing_.get() ||  // We can't purge anything.
      HasUncommittedChanges())  // We leave things alone with changes pending.
    return;

  // Drop the in memory cache, we'll reload when needed.
  is_initial_import_done_ = false;
  map_ = new DOMStorageMap(kPerStorageAreaQuota +
                           kPerStorageAreaOverQuotaAllowance);

  // Recreate the database object, this frees up the open sqlite connection
  // and its page cache.
  backing_->Reset();
}

void DOMStorageArea::Shutdown() {
  DCHECK(!is_shutdown_);
  is_shutdown_ = true;
  map_ = NULL;
  if (!backing_)
    return;

  bool success = task_runner_->PostShutdownBlockingTask(
      FROM_HERE,
      DOMStorageTaskRunner::COMMIT_SEQUENCE,
      base::Bind(&DOMStorageArea::ShutdownInCommitSequence, this));
  DCHECK(success);
}

void DOMStorageArea::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(task_runner_->IsRunningOnPrimarySequence());
  if (!is_initial_import_done_)
    return;

  // Limit the url length to 50 and strip special characters.
  std::string url = origin_.spec().substr(0, 50);
  for (size_t index = 0; index < url.size(); ++index) {
    if (!std::isalnum(url[index]))
      url[index] = '_';
  }
  std::string name =
      base::StringPrintf("dom_storage/%s/0x%" PRIXPTR, url.c_str(),
                         reinterpret_cast<uintptr_t>(this));

  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (commit_batch_) {
    auto commit_batch_mad = pmd->CreateAllocatorDump(name + "/commit_batch");
    commit_batch_mad->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        commit_batch_->GetDataSize());
    if (system_allocator_name)
      pmd->AddSuballocation(commit_batch_mad->guid(), system_allocator_name);
  }

  // Do not add storage map usage if less than 1KB.
  if (map_->bytes_used() < 1024)
    return;

  auto map_mad = pmd->CreateAllocatorDump(name + "/storage_map");
  map_mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                     base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                     map_->bytes_used());
  if (system_allocator_name)
    pmd->AddSuballocation(map_mad->guid(), system_allocator_name);
  // TODO(ssid): Add memory usage of local backing storage crbug.com/605785.
}

void DOMStorageArea::InitialImportIfNeeded() {
  if (is_initial_import_done_)
    return;

  DCHECK(backing_.get());

  base::TimeTicks before = base::TimeTicks::Now();
  DOMStorageValuesMap initial_values;
  backing_->ReadAllValues(&initial_values);
  map_->SwapValues(&initial_values);
  is_initial_import_done_ = true;
  base::TimeDelta time_to_import = base::TimeTicks::Now() - before;
  UMA_HISTOGRAM_TIMES("LocalStorage.BrowserTimeToPrimeLocalStorage",
                      time_to_import);

  size_t local_storage_size_kb = map_->bytes_used() / 1024;
  // Track localStorage size, from 0-6MB. Note that the maximum size should be
  // 5MB, but we add some slop since we want to make sure the max size is always
  // above what we see in practice, since histograms can't change.
  UMA_HISTOGRAM_CUSTOM_COUNTS("LocalStorage.BrowserLocalStorageSizeInKB",
                              local_storage_size_kb,
                              0, 6 * 1024, 50);
  if (local_storage_size_kb < 100) {
    UMA_HISTOGRAM_TIMES(
        "LocalStorage.BrowserTimeToPrimeLocalStorageUnder100KB",
        time_to_import);
  } else if (local_storage_size_kb < 1000) {
    UMA_HISTOGRAM_TIMES(
        "LocalStorage.BrowserTimeToPrimeLocalStorage100KBTo1MB",
        time_to_import);
  } else {
    UMA_HISTOGRAM_TIMES(
        "LocalStorage.BrowserTimeToPrimeLocalStorage1MBTo5MB",
        time_to_import);
  }
}

DOMStorageArea::CommitBatch* DOMStorageArea::CreateCommitBatchIfNeeded() {
  DCHECK(!is_shutdown_);
  if (!commit_batch_) {
    commit_batch_.reset(new CommitBatch());
    BrowserThread::PostAfterStartupTask(
        FROM_HERE, task_runner_,
        base::Bind(&DOMStorageArea::StartCommitTimer, this));
  }
  return commit_batch_.get();
}

void DOMStorageArea::StartCommitTimer() {
  if (is_shutdown_ || !commit_batch_)
    return;

  // Start a timer to commit any changes that accrue in the batch, but only if
  // no commits are currently in flight. In that case the timer will be
  // started after the commits have happened.
  if (commit_batches_in_flight_)
    return;

  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&DOMStorageArea::OnCommitTimer, this),
      ComputeCommitDelay());
}

base::TimeDelta DOMStorageArea::ComputeCommitDelay() const {
  if (s_aggressive_flushing_enabled_)
    return base::TimeDelta::FromSeconds(1);

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  base::TimeDelta delay = std::max(
      base::TimeDelta::FromSeconds(kCommitDefaultDelaySecs),
      std::max(commit_rate_limiter_.ComputeDelayNeeded(elapsed_time),
               data_rate_limiter_.ComputeDelayNeeded(elapsed_time)));
  UMA_HISTOGRAM_LONG_TIMES("LocalStorage.CommitDelay", delay);
  return delay;
}

void DOMStorageArea::OnCommitTimer() {
  if (is_shutdown_)
    return;

  // It's possible that there is nothing to commit if an immediate
  // commit occured after the timer was scheduled but before it fired.
  if (!commit_batch_)
    return;

  PostCommitTask();
}

void DOMStorageArea::PostCommitTask() {
  if (is_shutdown_ || !commit_batch_)
    return;

  DCHECK(backing_.get());

  commit_rate_limiter_.add_samples(1);
  data_rate_limiter_.add_samples(commit_batch_->GetDataSize());

  // This method executes on the primary sequence, we schedule
  // a task for immediate execution on the commit sequence.
  DCHECK(task_runner_->IsRunningOnPrimarySequence());
  bool success = task_runner_->PostShutdownBlockingTask(
      FROM_HERE,
      DOMStorageTaskRunner::COMMIT_SEQUENCE,
      base::Bind(&DOMStorageArea::CommitChanges, this,
                 base::Owned(commit_batch_.release())));
  ++commit_batches_in_flight_;
  DCHECK(success);
}

void DOMStorageArea::CommitChanges(const CommitBatch* commit_batch) {
  // This method executes on the commit sequence.
  DCHECK(task_runner_->IsRunningOnCommitSequence());
  backing_->CommitChanges(commit_batch->clear_all_first,
                          commit_batch->changed_values);
  // TODO(michaeln): what if CommitChanges returns false (e.g., we're trying to
  // commit to a DB which is in an inconsistent state?)
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageArea::OnCommitComplete, this));
}

void DOMStorageArea::OnCommitComplete() {
  // We're back on the primary sequence in this method.
  DCHECK(task_runner_->IsRunningOnPrimarySequence());
  --commit_batches_in_flight_;
  if (is_shutdown_)
    return;
  if (commit_batch_.get() && !commit_batches_in_flight_) {
    // More changes have accrued, restart the timer.
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&DOMStorageArea::OnCommitTimer, this),
        ComputeCommitDelay());
  }
}

void DOMStorageArea::ShutdownInCommitSequence() {
  // This method executes on the commit sequence.
  DCHECK(task_runner_->IsRunningOnCommitSequence());
  DCHECK(backing_.get());
  if (commit_batch_) {
    // Commit any changes that accrued prior to the timer firing.
    bool success = backing_->CommitChanges(
        commit_batch_->clear_all_first,
        commit_batch_->changed_values);
    DCHECK(success);
  }
  commit_batch_.reset();
  backing_.reset();
  session_storage_backing_ = NULL;
}

}  // namespace content
