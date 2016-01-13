// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_area.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/local_storage_database_adapter.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/browser/dom_storage/session_storage_database_adapter.h"
#include "content/common/dom_storage/dom_storage_map.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "webkit/browser/database/database_util.h"
#include "webkit/common/database/database_identifier.h"
#include "webkit/common/fileapi/file_system_util.h"

using webkit_database::DatabaseUtil;

namespace content {

static const int kCommitTimerSeconds = 1;

DOMStorageArea::CommitBatch::CommitBatch()
  : clear_all_first(false) {
}
DOMStorageArea::CommitBatch::~CommitBatch() {}


// static
const base::FilePath::CharType DOMStorageArea::kDatabaseFileExtension[] =
    FILE_PATH_LITERAL(".localstorage");

// static
base::FilePath DOMStorageArea::DatabaseFileNameFromOrigin(const GURL& origin) {
  std::string filename = webkit_database::GetIdentifierFromOrigin(origin);
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
  return webkit_database::GetOriginFromIdentifier(origin_id);
}

DOMStorageArea::DOMStorageArea(
    const GURL& origin, const base::FilePath& directory,
    DOMStorageTaskRunner* task_runner)
    : namespace_id_(kLocalStorageNamespaceId), origin_(origin),
      directory_(directory),
      task_runner_(task_runner),
      map_(new DOMStorageMap(kPerStorageAreaQuota +
                             kPerStorageAreaOverQuotaAllowance)),
      is_initial_import_done_(true),
      is_shutdown_(false),
      commit_batches_in_flight_(0) {
  if (!directory.empty()) {
    base::FilePath path = directory.Append(DatabaseFileNameFromOrigin(origin_));
    backing_.reset(new LocalStorageDatabaseAdapter(path));
    is_initial_import_done_ = false;
  }
}

DOMStorageArea::DOMStorageArea(
    int64 namespace_id,
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
      commit_batches_in_flight_(0) {
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
  if (success && backing_) {
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
    int64 destination_namespace_id,
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
  // shallow copy is made (scheduled by the upper layer). Another OnCommitTimer
  // call might be in the event queue at this point, but it's handled gracefully
  // when it fires.
  if (commit_batch_)
    OnCommitTimer();
  return copy;
}

bool DOMStorageArea::HasUncommittedChanges() const {
  DCHECK(!is_shutdown_);
  return commit_batch_.get() || commit_batches_in_flight_;
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

    // Start a timer to commit any changes that accrue in the batch, but only if
    // no commits are currently in flight. In that case the timer will be
    // started after the commits have happened.
    if (!commit_batches_in_flight_) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&DOMStorageArea::OnCommitTimer, this),
          base::TimeDelta::FromSeconds(kCommitTimerSeconds));
    }
  }
  return commit_batch_.get();
}

void DOMStorageArea::OnCommitTimer() {
  if (is_shutdown_)
    return;

  DCHECK(backing_.get());

  // It's possible that there is nothing to commit, since a shallow copy occured
  // before the timer fired.
  if (!commit_batch_)
    return;

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
        FROM_HERE,
        base::Bind(&DOMStorageArea::OnCommitTimer, this),
        base::TimeDelta::FromSeconds(kCommitTimerSeconds));
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
