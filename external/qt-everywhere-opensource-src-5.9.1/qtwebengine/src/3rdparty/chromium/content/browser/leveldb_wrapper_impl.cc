// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/leveldb_wrapper_impl.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/leveldb/public/cpp/util.h"
#include "content/public/browser/browser_thread.h"

namespace content {

bool LevelDBWrapperImpl::s_aggressive_flushing_enabled_ = false;

LevelDBWrapperImpl::RateLimiter::RateLimiter(size_t desired_rate,
                                            base::TimeDelta time_quantum)
    : rate_(desired_rate), samples_(0), time_quantum_(time_quantum) {
  DCHECK_GT(desired_rate, 0ul);
}

base::TimeDelta LevelDBWrapperImpl::RateLimiter::ComputeTimeNeeded() const {
  return time_quantum_ * (samples_ / rate_);
}

base::TimeDelta LevelDBWrapperImpl::RateLimiter::ComputeDelayNeeded(
    const base::TimeDelta elapsed_time) const {
  base::TimeDelta time_needed = ComputeTimeNeeded();
  if (time_needed > elapsed_time)
    return time_needed - elapsed_time;
  return base::TimeDelta();
}

LevelDBWrapperImpl::CommitBatch::CommitBatch() : clear_all_first(false) {}
LevelDBWrapperImpl::CommitBatch::~CommitBatch() {}

size_t LevelDBWrapperImpl::CommitBatch::GetDataSize() const {
  if (changed_values.empty())
    return 0;

  size_t count = 0;
  for (const auto& pair : changed_values) {
    count += pair.first.size();
    if (pair.second)
      count += pair.second->size();
  }
  return count;
}

LevelDBWrapperImpl::LevelDBWrapperImpl(
    leveldb::mojom::LevelDBDatabase* database,
    const std::string& prefix,
    size_t max_size,
    base::TimeDelta default_commit_delay,
    int max_bytes_per_hour,
    int max_commits_per_hour,
    const base::Closure& no_bindings_callback)
    : prefix_(prefix),
      no_bindings_callback_(no_bindings_callback),
      database_(database),
      bytes_used_(0),
      max_size_(max_size),
      start_time_(base::TimeTicks::Now()),
      default_commit_delay_(default_commit_delay),
      data_rate_limiter_(max_bytes_per_hour, base::TimeDelta::FromHours(1)),
      commit_rate_limiter_(max_commits_per_hour, base::TimeDelta::FromHours(1)),
      weak_ptr_factory_(this) {
  bindings_.set_connection_error_handler(base::Bind(
      &LevelDBWrapperImpl::OnConnectionError, base::Unretained(this)));
}

LevelDBWrapperImpl::~LevelDBWrapperImpl() {
  if (commit_batch_)
    CommitChanges();
}

void LevelDBWrapperImpl::Bind(mojom::LevelDBWrapperRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void LevelDBWrapperImpl::AddObserver(mojom::LevelDBObserverPtr observer) {
  observers_.AddPtr(std::move(observer));
}

void LevelDBWrapperImpl::EnableAggressiveCommitDelay() {
  s_aggressive_flushing_enabled_ = true;
}

void LevelDBWrapperImpl::Put(const std::vector<uint8_t>& key,
                             const std::vector<uint8_t>& value,
                             const std::string& source,
                             const PutCallback& callback) {
  if (!map_) {
    LoadMap(base::Bind(&LevelDBWrapperImpl::Put, base::Unretained(this), key,
                       value, source, callback));
    return;
  }

  bool has_old_item = false;
  size_t old_item_size = 0;
  auto found = map_->find(key);
  if (found != map_->end()) {
    if (found->second == value) {
      callback.Run(true);  // Key already has this value.
      return;
    }
    old_item_size = key.size() + found->second.size();
    has_old_item = true;
  }
  size_t new_item_size = key.size() + value.size();
  size_t new_bytes_used = bytes_used_ - old_item_size + new_item_size;

  // Only check quota if the size is increasing, this allows
  // shrinking changes to pre-existing maps that are over budget.
  if (new_item_size > old_item_size && new_bytes_used > max_size_) {
    callback.Run(false);
    return;
  }

  if (database_) {
    CreateCommitBatchIfNeeded();
    commit_batch_->changed_values[key] = value;
  }

  std::vector<uint8_t> old_value;
  if (has_old_item) {
    old_value.swap((*map_)[key]);
  }
  (*map_)[key] = value;
  bytes_used_ = new_bytes_used;
  if (!has_old_item) {
    // We added a new key/value pair.
    observers_.ForAllPtrs(
        [&key, &value, &source](mojom::LevelDBObserver* observer) {
          observer->KeyAdded(key, value, source);
        });
  } else {
    // We changed the value for an existing key.
    observers_.ForAllPtrs(
        [&key, &value, &source, &old_value](mojom::LevelDBObserver* observer) {
          observer->KeyChanged(key, value, old_value, source);
        });
  }
  callback.Run(true);
}

void LevelDBWrapperImpl::Delete(const std::vector<uint8_t>& key,
                                const std::string& source,
                                const DeleteCallback& callback) {
  if (!map_) {
    LoadMap(base::Bind(&LevelDBWrapperImpl::Delete, base::Unretained(this), key,
                       source, callback));
    return;
  }

  auto found = map_->find(key);
  if (found == map_->end()) {
    callback.Run(true);
    return;
  }

  if (database_) {
    CreateCommitBatchIfNeeded();
    commit_batch_->changed_values[key] = base::nullopt;
  }

  std::vector<uint8_t> old_value(std::move(found->second));
  map_->erase(found);
  bytes_used_ -= key.size() + old_value.size();
  observers_.ForAllPtrs(
      [&key, &source, &old_value](mojom::LevelDBObserver* observer) {
        observer->KeyDeleted(key, old_value, source);
      });
  callback.Run(true);
}

void LevelDBWrapperImpl::DeleteAll(const std::string& source,
                                   const DeleteAllCallback& callback) {
  if (!map_) {
    LoadMap(
        base::Bind(&LevelDBWrapperImpl::DeleteAll, base::Unretained(this),
                    source, callback));
    return;
  }

  if (database_ && !map_->empty()) {
    CreateCommitBatchIfNeeded();
    commit_batch_->clear_all_first = true;
    commit_batch_->changed_values.clear();
  }
  map_->clear();
  bytes_used_ = 0;
  observers_.ForAllPtrs(
      [&source](mojom::LevelDBObserver* observer) {
        observer->AllDeleted(source);
      });
  callback.Run(true);
}

void LevelDBWrapperImpl::Get(const std::vector<uint8_t>& key,
                             const GetCallback& callback) {
  if (!map_) {
    LoadMap(base::Bind(&LevelDBWrapperImpl::Get, base::Unretained(this), key,
                       callback));
    return;
  }

  auto found = map_->find(key);
  if (found == map_->end()) {
    callback.Run(false, std::vector<uint8_t>());
    return;
  }
  callback.Run(true, found->second);
}

void LevelDBWrapperImpl::GetAll(const std::string& source,
                                const GetAllCallback& callback) {
  if (!map_) {
    LoadMap(
        base::Bind(&LevelDBWrapperImpl::GetAll, base::Unretained(this),
                   source, callback));
    return;
  }

  std::vector<mojom::KeyValuePtr> all;
  for (const auto& it : (*map_)) {
    mojom::KeyValuePtr kv = mojom::KeyValue::New();
    kv->key = it.first;
    kv->value = it.second;
    all.push_back(std::move(kv));
  }
  callback.Run(leveldb::mojom::DatabaseError::OK, std::move(all));
  observers_.ForAllPtrs(
      [source](mojom::LevelDBObserver* observer) {
        observer->GetAllComplete(source);
      });
}

void LevelDBWrapperImpl::OnConnectionError() {
  if (!bindings_.empty())
    return;
  no_bindings_callback_.Run();
}

void LevelDBWrapperImpl::LoadMap(const base::Closure& completion_callback) {
  DCHECK(!map_);
  on_load_complete_tasks_.push_back(completion_callback);
  if (on_load_complete_tasks_.size() > 1)
    return;

  // TODO(michaeln): Import from sqlite localstorage db.
  database_->GetPrefixed(leveldb::StdStringToUint8Vector(prefix_),
                         base::Bind(&LevelDBWrapperImpl::OnLoadComplete,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void LevelDBWrapperImpl::OnLoadComplete(
    leveldb::mojom::DatabaseError status,
    std::vector<leveldb::mojom::KeyValuePtr> data) {
  DCHECK(!map_);
  map_.reset(new ValueMap);
  for (auto& it : data)
    (*map_)[it->key] = it->value;

  // We proceed without using a backing store, nothing will be persisted but the
  // class is functional for the lifetime of the object.
  // TODO(michaeln): Uma here or in the DB file?
  if (status != leveldb::mojom::DatabaseError::OK)
    database_ = nullptr;

  std::vector<base::Closure> tasks;
  on_load_complete_tasks_.swap(tasks);
  for (auto& task : tasks)
    task.Run();
}

void LevelDBWrapperImpl::CreateCommitBatchIfNeeded() {
  if (commit_batch_)
    return;

  commit_batch_.reset(new CommitBatch());
  BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&LevelDBWrapperImpl::StartCommitTimer,
                  weak_ptr_factory_.GetWeakPtr()));
}

void LevelDBWrapperImpl::StartCommitTimer() {
  if (!commit_batch_)
    return;

  // Start a timer to commit any changes that accrue in the batch, but only if
  // no commits are currently in flight. In that case the timer will be
  // started after the commits have happened.
  if (commit_batches_in_flight_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&LevelDBWrapperImpl::CommitChanges,
                            weak_ptr_factory_.GetWeakPtr()),
      ComputeCommitDelay());
}

base::TimeDelta LevelDBWrapperImpl::ComputeCommitDelay() const {
  if (s_aggressive_flushing_enabled_)
    return base::TimeDelta::FromSeconds(1);

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  base::TimeDelta delay = std::max(
      default_commit_delay_,
      std::max(commit_rate_limiter_.ComputeDelayNeeded(elapsed_time),
               data_rate_limiter_.ComputeDelayNeeded(elapsed_time)));
  // TODO(michaeln): UMA_HISTOGRAM_LONG_TIMES("LevelDBWrapper.CommitDelay", d);
  return delay;
}

void LevelDBWrapperImpl::CommitChanges() {
  DCHECK(database_);
  if (commit_batch_)
    return;

  commit_rate_limiter_.add_samples(1);
  data_rate_limiter_.add_samples(commit_batch_->GetDataSize());

  // Commit all our changes in a single batch.
  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  if (commit_batch_->clear_all_first) {
    leveldb::mojom::BatchedOperationPtr item =
        leveldb::mojom::BatchedOperation::New();
    item->type = leveldb::mojom::BatchOperationType::DELETE_PREFIXED_KEY;
    item->key = leveldb::StdStringToUint8Vector(prefix_);
    operations.push_back(std::move(item));
  }
  for (auto& it : commit_batch_->changed_values) {
    leveldb::mojom::BatchedOperationPtr item =
        leveldb::mojom::BatchedOperation::New();
    item->key = std::move(it.first);
    if (!it.second) {
      item->type = leveldb::mojom::BatchOperationType::DELETE_KEY;
    } else {
      item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
      item->value = std::move(*(it.second));
    }
    operations.push_back(std::move(item));
  }
  commit_batch_.reset();

  ++commit_batches_in_flight_;

  // TODO(michaeln): Currently there is no guarantee LevelDBDatabaseImp::Write
  // will run during a clean shutdown. We need that to avoid dataloss.
  database_->Write(std::move(operations),
                   base::Bind(&LevelDBWrapperImpl::OnCommitComplete,
                              weak_ptr_factory_.GetWeakPtr()));
}

void LevelDBWrapperImpl::OnCommitComplete(leveldb::mojom::DatabaseError error) {
  // TODO(michaeln): What if it fails, uma here or in the DB class?
  --commit_batches_in_flight_;
  StartCommitTimer();
}

}  // namespace content
