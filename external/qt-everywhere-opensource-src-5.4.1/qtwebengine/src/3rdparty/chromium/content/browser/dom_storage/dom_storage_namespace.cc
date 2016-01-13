// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_namespace.h"

#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/common/child_process_host.h"

namespace content {

namespace {

static const unsigned int kMaxTransactionLogEntries = 8 * 1024;

}  // namespace

DOMStorageNamespace::DOMStorageNamespace(
    const base::FilePath& directory,
    DOMStorageTaskRunner* task_runner)
    : namespace_id_(kLocalStorageNamespaceId),
      directory_(directory),
      task_runner_(task_runner),
      num_aliases_(0),
      old_master_for_close_area_(NULL),
      master_alias_count_decremented_(false),
      ready_for_deletion_pending_aliases_(false),
      must_persist_at_shutdown_(false) {
}

DOMStorageNamespace::DOMStorageNamespace(
    int64 namespace_id,
    const std::string& persistent_namespace_id,
    SessionStorageDatabase* session_storage_database,
    DOMStorageTaskRunner* task_runner)
    : namespace_id_(namespace_id),
      persistent_namespace_id_(persistent_namespace_id),
      task_runner_(task_runner),
      session_storage_database_(session_storage_database),
      num_aliases_(0),
      old_master_for_close_area_(NULL),
      master_alias_count_decremented_(false),
      ready_for_deletion_pending_aliases_(false),
      must_persist_at_shutdown_(false) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
}

DOMStorageNamespace::~DOMStorageNamespace() {
  STLDeleteValues(&transactions_);
  DecrementMasterAliasCount();
}

DOMStorageArea* DOMStorageNamespace::OpenStorageArea(const GURL& origin) {
  if (alias_master_namespace_)
    return alias_master_namespace_->OpenStorageArea(origin);
  if (AreaHolder* holder = GetAreaHolder(origin)) {
    ++(holder->open_count_);
    return holder->area_.get();
  }
  DOMStorageArea* area;
  if (namespace_id_ == kLocalStorageNamespaceId) {
    area = new DOMStorageArea(origin, directory_, task_runner_.get());
  } else {
    area = new DOMStorageArea(
        namespace_id_, persistent_namespace_id_, origin,
        session_storage_database_.get(), task_runner_.get());
  }
  areas_[origin] = AreaHolder(area, 1);
  return area;
}

void DOMStorageNamespace::CloseStorageArea(DOMStorageArea* area) {
  AreaHolder* holder = GetAreaHolder(area->origin());
  if (alias_master_namespace_) {
    DCHECK(!holder);
    if (old_master_for_close_area_)
      old_master_for_close_area_->CloseStorageArea(area);
    else
      alias_master_namespace_->CloseStorageArea(area);
    return;
  }
  DCHECK(holder);
  DCHECK_EQ(holder->area_.get(), area);
  --(holder->open_count_);
  // TODO(michaeln): Clean up areas that aren't needed in memory anymore.
  // The in-process-webkit based impl didn't do this either, but would be nice.
}

DOMStorageArea* DOMStorageNamespace::GetOpenStorageArea(const GURL& origin) {
  if (alias_master_namespace_)
    return alias_master_namespace_->GetOpenStorageArea(origin);
  AreaHolder* holder = GetAreaHolder(origin);
  if (holder && holder->open_count_)
    return holder->area_.get();
  return NULL;
}

DOMStorageNamespace* DOMStorageNamespace::Clone(
    int64 clone_namespace_id,
    const std::string& clone_persistent_namespace_id) {
  if (alias_master_namespace_) {
    return alias_master_namespace_->Clone(clone_namespace_id,
                                          clone_persistent_namespace_id);
  }
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, clone_namespace_id);
  DOMStorageNamespace* clone = new DOMStorageNamespace(
      clone_namespace_id, clone_persistent_namespace_id,
      session_storage_database_.get(), task_runner_.get());
  AreaMap::const_iterator it = areas_.begin();
  // Clone the in-memory structures.
  for (; it != areas_.end(); ++it) {
    DOMStorageArea* area = it->second.area_->ShallowCopy(
        clone_namespace_id, clone_persistent_namespace_id);
    clone->areas_[it->first] = AreaHolder(area, 0);
  }
  // And clone the on-disk structures, too.
  if (session_storage_database_.get()) {
    task_runner_->PostShutdownBlockingTask(
        FROM_HERE,
        DOMStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(base::IgnoreResult(&SessionStorageDatabase::CloneNamespace),
                   session_storage_database_.get(), persistent_namespace_id_,
                   clone_persistent_namespace_id));
  }
  return clone;
}

DOMStorageNamespace* DOMStorageNamespace::CreateAlias(
    int64 alias_namespace_id) {
  // Creates an alias of the current DOMStorageNamespace.
  // The alias will have a reference to this namespace (called the master),
  // and all operations will be redirected to the master (in particular,
  // the alias will never open any areas of its own, but always redirect
  // to the master). Accordingly, an alias will also never undergo the shutdown
  // procedure which initiates persisting to disk, since there is never any data
  // of its own to persist to disk. DOMStorageContextImpl is the place where
  // shutdowns are initiated, but only for non-alias DOMStorageNamespaces.
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id_);
  DCHECK_NE(kLocalStorageNamespaceId, alias_namespace_id);
  DOMStorageNamespace* alias = new DOMStorageNamespace(
      alias_namespace_id, persistent_namespace_id_,
      session_storage_database_.get(), task_runner_.get());
  if (alias_master_namespace_ != NULL) {
    DCHECK(alias_master_namespace_->alias_master_namespace_ == NULL);
    alias->alias_master_namespace_ = alias_master_namespace_;
  } else {
    alias->alias_master_namespace_ = this;
  }
  alias->alias_master_namespace_->num_aliases_++;
  return alias;
}

void DOMStorageNamespace::DeleteLocalStorageOrigin(const GURL& origin) {
  DCHECK(!session_storage_database_.get());
  DCHECK(!alias_master_namespace_.get());
  AreaHolder* holder = GetAreaHolder(origin);
  if (holder) {
    holder->area_->DeleteOrigin();
    return;
  }
  if (!directory_.empty()) {
    scoped_refptr<DOMStorageArea> area =
        new DOMStorageArea(origin, directory_, task_runner_.get());
    area->DeleteOrigin();
  }
}

void DOMStorageNamespace::DeleteSessionStorageOrigin(const GURL& origin) {
  if (alias_master_namespace_) {
    alias_master_namespace_->DeleteSessionStorageOrigin(origin);
    return;
  }
  DOMStorageArea* area = OpenStorageArea(origin);
  area->FastClear();
  CloseStorageArea(area);
}

void DOMStorageNamespace::PurgeMemory(PurgeOption option) {
  if (alias_master_namespace_) {
    alias_master_namespace_->PurgeMemory(option);
    return;
  }
  if (directory_.empty())
    return;  // We can't purge w/o backing on disk.
  AreaMap::iterator it = areas_.begin();
  while (it != areas_.end()) {
    // Leave it alone if changes are pending
    if (it->second.area_->HasUncommittedChanges()) {
      ++it;
      continue;
    }

    // If not in use, we can shut it down and remove
    // it from our collection entirely.
    if (it->second.open_count_ == 0) {
      it->second.area_->Shutdown();
      areas_.erase(it++);
      continue;
    }

    if (option == PURGE_AGGRESSIVE) {
      // If aggressive is true, we clear caches and such
      // for opened areas.
      it->second.area_->PurgeMemory();
    }

    ++it;
  }
}

void DOMStorageNamespace::Shutdown() {
  AreaMap::const_iterator it = areas_.begin();
  for (; it != areas_.end(); ++it)
    it->second.area_->Shutdown();
}

unsigned int DOMStorageNamespace::CountInMemoryAreas() const {
  if (alias_master_namespace_)
    return alias_master_namespace_->CountInMemoryAreas();
  unsigned int area_count = 0;
  for (AreaMap::const_iterator it = areas_.begin(); it != areas_.end(); ++it) {
    if (it->second.area_->IsLoadedInMemory())
      ++area_count;
  }
  return area_count;
}

DOMStorageNamespace::AreaHolder*
DOMStorageNamespace::GetAreaHolder(const GURL& origin) {
  AreaMap::iterator found = areas_.find(origin);
  if (found == areas_.end())
    return NULL;
  return &(found->second);
}

void DOMStorageNamespace::AddTransactionLogProcessId(int process_id) {
  DCHECK(process_id != ChildProcessHost::kInvalidUniqueID);
  DCHECK(transactions_.count(process_id) == 0);
  TransactionData* transaction_data = new TransactionData;
  transactions_[process_id] = transaction_data;
}

void DOMStorageNamespace::RemoveTransactionLogProcessId(int process_id) {
  DCHECK(process_id != ChildProcessHost::kInvalidUniqueID);
  DCHECK(transactions_.count(process_id) == 1);
  delete transactions_[process_id];
  transactions_.erase(process_id);
}

SessionStorageNamespace::MergeResult DOMStorageNamespace::Merge(
    bool actually_merge,
    int process_id,
    DOMStorageNamespace* other,
    DOMStorageContextImpl* context) {
  if (!alias_master_namespace())
    return SessionStorageNamespace::MERGE_RESULT_NAMESPACE_NOT_ALIAS;
  if (transactions_.count(process_id) < 1)
    return SessionStorageNamespace::MERGE_RESULT_NOT_LOGGING;
  TransactionData* data = transactions_[process_id];
  if (data->max_log_size_exceeded)
    return SessionStorageNamespace::MERGE_RESULT_TOO_MANY_TRANSACTIONS;
  if (data->log.size() < 1) {
    if (actually_merge)
      SwitchToNewAliasMaster(other, context);
    return SessionStorageNamespace::MERGE_RESULT_NO_TRANSACTIONS;
  }

  // skip_areas and skip_keys store areas and (area, key) pairs, respectively,
  // that have already been handled previously. Any further modifications to
  // them will not change the result of the hypothetical merge.
  std::set<GURL> skip_areas;
  typedef std::pair<GURL, base::string16> OriginKey;
  std::set<OriginKey> skip_keys;
  // Indicates whether we could still merge the namespaces preserving all
  // individual transactions.
  for (unsigned int i = 0; i < data->log.size(); i++) {
    TransactionRecord& transaction = data->log[i];
    if (transaction.transaction_type == TRANSACTION_CLEAR) {
      skip_areas.insert(transaction.origin);
      continue;
    }
    if (skip_areas.find(transaction.origin) != skip_areas.end())
      continue;
    if (skip_keys.find(OriginKey(transaction.origin, transaction.key))
        != skip_keys.end()) {
      continue;
    }
    if (transaction.transaction_type == TRANSACTION_REMOVE ||
        transaction.transaction_type == TRANSACTION_WRITE) {
      skip_keys.insert(OriginKey(transaction.origin, transaction.key));
      continue;
    }
    if (transaction.transaction_type == TRANSACTION_READ) {
      DOMStorageArea* area = other->OpenStorageArea(transaction.origin);
      base::NullableString16 other_value = area->GetItem(transaction.key);
      other->CloseStorageArea(area);
      if (transaction.value != other_value)
        return SessionStorageNamespace::MERGE_RESULT_NOT_MERGEABLE;
      continue;
    }
    NOTREACHED();
  }
  if (!actually_merge)
    return SessionStorageNamespace::MERGE_RESULT_MERGEABLE;

  // Actually perform the merge.

  for (unsigned int i = 0; i < data->log.size(); i++) {
    TransactionRecord& transaction = data->log[i];
    if (transaction.transaction_type == TRANSACTION_READ)
      continue;
    DOMStorageArea* area = other->OpenStorageArea(transaction.origin);
    if (transaction.transaction_type == TRANSACTION_CLEAR) {
      area->Clear();
      if (context)
        context->NotifyAreaCleared(area, transaction.page_url);
    }
    if (transaction.transaction_type == TRANSACTION_REMOVE) {
      base::string16 old_value;
      area->RemoveItem(transaction.key, &old_value);
      if (context) {
        context->NotifyItemRemoved(area, transaction.key, old_value,
                                   transaction.page_url);
      }
    }
    if (transaction.transaction_type == TRANSACTION_WRITE) {
      base::NullableString16 old_value;
      area->SetItem(transaction.key, base::string16(transaction.value.string()),
                    &old_value);
      if (context) {
        context->NotifyItemSet(area, transaction.key,transaction.value.string(),
                               old_value, transaction.page_url);
      }
    }
    other->CloseStorageArea(area);
  }

  SwitchToNewAliasMaster(other, context);
  return SessionStorageNamespace::MERGE_RESULT_MERGEABLE;
}

bool DOMStorageNamespace::IsLoggingRenderer(int process_id) {
  DCHECK(process_id != ChildProcessHost::kInvalidUniqueID);
  if (transactions_.count(process_id) < 1)
    return false;
  return !transactions_[process_id]->max_log_size_exceeded;
}

void DOMStorageNamespace::AddTransaction(
    int process_id, const TransactionRecord& transaction) {
  if (!IsLoggingRenderer(process_id))
    return;
  TransactionData* transaction_data = transactions_[process_id];
  DCHECK(transaction_data);
  if (transaction_data->max_log_size_exceeded)
    return;
  transaction_data->log.push_back(transaction);
  if (transaction_data->log.size() > kMaxTransactionLogEntries) {
    transaction_data->max_log_size_exceeded = true;
    transaction_data->log.clear();
  }
}

bool DOMStorageNamespace::DecrementMasterAliasCount() {
  if (!alias_master_namespace_ || master_alias_count_decremented_)
    return false;
  DCHECK_GT(alias_master_namespace_->num_aliases_, 0);
  alias_master_namespace_->num_aliases_--;
  master_alias_count_decremented_ = true;
  return (alias_master_namespace_->num_aliases_ == 0);
}

void DOMStorageNamespace::SwitchToNewAliasMaster(
    DOMStorageNamespace* new_master,
    DOMStorageContextImpl* context) {
  DCHECK(alias_master_namespace());
  scoped_refptr<DOMStorageNamespace> old_master = alias_master_namespace();
  if (new_master->alias_master_namespace())
    new_master = new_master->alias_master_namespace();
  DCHECK(!new_master->alias_master_namespace());
  DCHECK(old_master != this);
  DCHECK(old_master != new_master);
  DecrementMasterAliasCount();
  alias_master_namespace_ = new_master;
  alias_master_namespace_->num_aliases_++;
  master_alias_count_decremented_ = false;
  // There are three things that we need to clean up:
  // -- the old master may ready for shutdown, if its last alias has disappeared
  // -- The dom_storage hosts need to close and reopen their areas, so that
  // they point to the correct new areas.
  // -- The renderers will need to reset their local caches.
  // All three of these things are accomplished with the following call below.
  // |context| will be NULL in unit tests, which is when this will
  // not apply, of course.
  // During this call, open areas will be closed & reopened, so that they now
  // come from the correct new master. Therefore, we must send close operations
  // to the old master.
  old_master_for_close_area_ = old_master.get();
  if (context)
    context->NotifyAliasSessionMerged(namespace_id(), old_master.get());
  old_master_for_close_area_ = NULL;
}

DOMStorageNamespace::TransactionData::TransactionData()
    : max_log_size_exceeded(false) {
}

DOMStorageNamespace::TransactionData::~TransactionData() {
}

DOMStorageNamespace::TransactionRecord::TransactionRecord() {
}

DOMStorageNamespace::TransactionRecord::~TransactionRecord() {
}

// AreaHolder

DOMStorageNamespace::AreaHolder::AreaHolder()
    : open_count_(0) {
}

DOMStorageNamespace::AreaHolder::AreaHolder(
    DOMStorageArea* area, int count)
    : area_(area), open_count_(count) {
}

DOMStorageNamespace::AreaHolder::~AreaHolder() {
}

}  // namespace content
