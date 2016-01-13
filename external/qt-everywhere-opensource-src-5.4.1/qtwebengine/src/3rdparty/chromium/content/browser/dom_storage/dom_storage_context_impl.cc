// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/time/time.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "webkit/browser/quota/special_storage_policy.h"

namespace content {

static const int kSessionStoraceScavengingSeconds = 60;

DOMStorageContextImpl::DOMStorageContextImpl(
    const base::FilePath& localstorage_directory,
    const base::FilePath& sessionstorage_directory,
    quota::SpecialStoragePolicy* special_storage_policy,
    DOMStorageTaskRunner* task_runner)
    : localstorage_directory_(localstorage_directory),
      sessionstorage_directory_(sessionstorage_directory),
      task_runner_(task_runner),
      is_shutdown_(false),
      force_keep_session_state_(false),
      special_storage_policy_(special_storage_policy),
      scavenging_started_(false) {
  // AtomicSequenceNum starts at 0 but we want to start session
  // namespace ids at one since zero is reserved for the
  // kLocalStorageNamespaceId.
  session_id_sequence_.GetNext();
}

DOMStorageContextImpl::~DOMStorageContextImpl() {
  if (session_storage_database_.get()) {
    // SessionStorageDatabase shouldn't be deleted right away: deleting it will
    // potentially involve waiting in leveldb::DBImpl::~DBImpl, and waiting
    // shouldn't happen on this thread.
    SessionStorageDatabase* to_release = session_storage_database_.get();
    to_release->AddRef();
    session_storage_database_ = NULL;
    task_runner_->PostShutdownBlockingTask(
        FROM_HERE,
        DOMStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(&SessionStorageDatabase::Release,
                   base::Unretained(to_release)));
  }
}

DOMStorageNamespace* DOMStorageContextImpl::GetStorageNamespace(
    int64 namespace_id) {
  if (is_shutdown_)
    return NULL;
  StorageNamespaceMap::iterator found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
    if (namespace_id == kLocalStorageNamespaceId) {
      if (!localstorage_directory_.empty()) {
        if (!base::CreateDirectory(localstorage_directory_)) {
          LOG(ERROR) << "Failed to create 'Local Storage' directory,"
                        " falling back to in-memory only.";
          localstorage_directory_ = base::FilePath();
        }
      }
      DOMStorageNamespace* local =
          new DOMStorageNamespace(localstorage_directory_, task_runner_.get());
      namespaces_[kLocalStorageNamespaceId] = local;
      return local;
    }
    return NULL;
  }
  return found->second.get();
}

void DOMStorageContextImpl::GetLocalStorageUsage(
    std::vector<LocalStorageUsageInfo>* infos,
    bool include_file_info) {
  if (localstorage_directory_.empty())
    return;
  base::FileEnumerator enumerator(localstorage_directory_, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (path.MatchesExtension(DOMStorageArea::kDatabaseFileExtension)) {
      LocalStorageUsageInfo info;
      info.origin = DOMStorageArea::OriginFromDatabaseFileName(path);
      if (include_file_info) {
        base::FileEnumerator::FileInfo find_info = enumerator.GetInfo();
        info.data_size = find_info.GetSize();
        info.last_modified = find_info.GetLastModifiedTime();
      }
      infos->push_back(info);
    }
  }
}

void DOMStorageContextImpl::GetSessionStorageUsage(
    std::vector<SessionStorageUsageInfo>* infos) {
  if (!session_storage_database_.get())
    return;
  std::map<std::string, std::vector<GURL> > namespaces_and_origins;
  session_storage_database_->ReadNamespacesAndOrigins(
      &namespaces_and_origins);
  for (std::map<std::string, std::vector<GURL> >::const_iterator it =
           namespaces_and_origins.begin();
       it != namespaces_and_origins.end(); ++it) {
    for (std::vector<GURL>::const_iterator origin_it = it->second.begin();
         origin_it != it->second.end(); ++origin_it) {
      SessionStorageUsageInfo info;
      info.persistent_namespace_id = it->first;
      info.origin = *origin_it;
      infos->push_back(info);
    }
  }
}

void DOMStorageContextImpl::DeleteLocalStorage(const GURL& origin) {
  DCHECK(!is_shutdown_);
  DOMStorageNamespace* local = GetStorageNamespace(kLocalStorageNamespaceId);
  local->DeleteLocalStorageOrigin(origin);
  // Synthesize a 'cleared' event if the area is open so CachedAreas in
  // renderers get emptied out too.
  DOMStorageArea* area = local->GetOpenStorageArea(origin);
  if (area)
    NotifyAreaCleared(area, origin);
}

void DOMStorageContextImpl::DeleteSessionStorage(
    const SessionStorageUsageInfo& usage_info) {
  DCHECK(!is_shutdown_);
  DOMStorageNamespace* dom_storage_namespace = NULL;
  std::map<std::string, int64>::const_iterator it =
      persistent_namespace_id_to_namespace_id_.find(
          usage_info.persistent_namespace_id);
  if (it != persistent_namespace_id_to_namespace_id_.end()) {
    dom_storage_namespace = GetStorageNamespace(it->second);
  } else {
    int64 namespace_id = AllocateSessionId();
    CreateSessionNamespace(namespace_id, usage_info.persistent_namespace_id);
    dom_storage_namespace = GetStorageNamespace(namespace_id);
  }
  dom_storage_namespace->DeleteSessionStorageOrigin(usage_info.origin);
  // Synthesize a 'cleared' event if the area is open so CachedAreas in
  // renderers get emptied out too.
  DOMStorageArea* area =
      dom_storage_namespace->GetOpenStorageArea(usage_info.origin);
  if (area)
    NotifyAreaCleared(area, usage_info.origin);
}

void DOMStorageContextImpl::Shutdown() {
  is_shutdown_ = true;
  StorageNamespaceMap::const_iterator it = namespaces_.begin();
  for (; it != namespaces_.end(); ++it)
    it->second->Shutdown();

  if (localstorage_directory_.empty() && !session_storage_database_.get())
    return;

  // Respect the content policy settings about what to
  // keep and what to discard.
  if (force_keep_session_state_)
    return;  // Keep everything.

  bool has_session_only_origins =
      special_storage_policy_.get() &&
      special_storage_policy_->HasSessionOnlyOrigins();

  if (has_session_only_origins) {
    // We may have to delete something. We continue on the
    // commit sequence after area shutdown tasks have cycled
    // thru that sequence (and closed their database files).
    bool success = task_runner_->PostShutdownBlockingTask(
        FROM_HERE,
        DOMStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(&DOMStorageContextImpl::ClearSessionOnlyOrigins, this));
    DCHECK(success);
  }
}

void DOMStorageContextImpl::AddEventObserver(EventObserver* observer) {
  event_observers_.AddObserver(observer);
}

void DOMStorageContextImpl::RemoveEventObserver(EventObserver* observer) {
  event_observers_.RemoveObserver(observer);
}

void DOMStorageContextImpl::NotifyItemSet(
    const DOMStorageArea* area,
    const base::string16& key,
    const base::string16& new_value,
    const base::NullableString16& old_value,
    const GURL& page_url) {
  FOR_EACH_OBSERVER(
      EventObserver, event_observers_,
      OnDOMStorageItemSet(area, key, new_value, old_value, page_url));
}

void DOMStorageContextImpl::NotifyItemRemoved(
    const DOMStorageArea* area,
    const base::string16& key,
    const base::string16& old_value,
    const GURL& page_url) {
  FOR_EACH_OBSERVER(
      EventObserver, event_observers_,
      OnDOMStorageItemRemoved(area, key, old_value, page_url));
}

void DOMStorageContextImpl::NotifyAreaCleared(
    const DOMStorageArea* area,
    const GURL& page_url) {
  FOR_EACH_OBSERVER(
      EventObserver, event_observers_,
      OnDOMStorageAreaCleared(area, page_url));
}

void DOMStorageContextImpl::NotifyAliasSessionMerged(
    int64 namespace_id,
    DOMStorageNamespace* old_alias_master_namespace) {
  FOR_EACH_OBSERVER(
      EventObserver, event_observers_,
      OnDOMSessionStorageReset(namespace_id));
  if (old_alias_master_namespace)
    MaybeShutdownSessionNamespace(old_alias_master_namespace);
}

std::string DOMStorageContextImpl::AllocatePersistentSessionId() {
  std::string guid = base::GenerateGUID();
  std::replace(guid.begin(), guid.end(), '-', '_');
  return guid;
}

void DOMStorageContextImpl::CreateSessionNamespace(
    int64 namespace_id,
    const std::string& persistent_namespace_id) {
  if (is_shutdown_)
    return;
  DCHECK(namespace_id != kLocalStorageNamespaceId);
  DCHECK(namespaces_.find(namespace_id) == namespaces_.end());
  namespaces_[namespace_id] = new DOMStorageNamespace(
      namespace_id, persistent_namespace_id, session_storage_database_.get(),
      task_runner_.get());
  persistent_namespace_id_to_namespace_id_[persistent_namespace_id] =
      namespace_id;
}

void DOMStorageContextImpl::DeleteSessionNamespace(
    int64 namespace_id, bool should_persist_data) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
  StorageNamespaceMap::const_iterator it = namespaces_.find(namespace_id);
  if (it == namespaces_.end() ||
      it->second->ready_for_deletion_pending_aliases()) {
    return;
  }
  it->second->set_ready_for_deletion_pending_aliases(true);
  DOMStorageNamespace* alias_master = it->second->alias_master_namespace();
  if (alias_master) {
    DCHECK(it->second->num_aliases() == 0);
    DCHECK(alias_master->alias_master_namespace() == NULL);
    if (should_persist_data)
      alias_master->set_must_persist_at_shutdown(true);
    if (it->second->DecrementMasterAliasCount())
      MaybeShutdownSessionNamespace(alias_master);
    namespaces_.erase(namespace_id);
  } else {
    if (should_persist_data)
      it->second->set_must_persist_at_shutdown(true);
    MaybeShutdownSessionNamespace(it->second);
  }
}

void DOMStorageContextImpl::MaybeShutdownSessionNamespace(
    DOMStorageNamespace* ns) {
  if (ns->num_aliases() > 0 || !ns->ready_for_deletion_pending_aliases())
    return;
  DCHECK_EQ(ns->num_aliases(), 0);
  DCHECK(ns->alias_master_namespace() == NULL);
  std::string persistent_namespace_id =  ns->persistent_namespace_id();
  if (session_storage_database_.get()) {
    if (!ns->must_persist_at_shutdown()) {
      task_runner_->PostShutdownBlockingTask(
          FROM_HERE,
          DOMStorageTaskRunner::COMMIT_SEQUENCE,
          base::Bind(
              base::IgnoreResult(&SessionStorageDatabase::DeleteNamespace),
              session_storage_database_,
              persistent_namespace_id));
    } else {
      // Ensure that the data gets committed before we shut down.
      ns->Shutdown();
      if (!scavenging_started_) {
        // Protect the persistent namespace ID from scavenging.
        protected_persistent_session_ids_.insert(persistent_namespace_id);
      }
    }
  }
  persistent_namespace_id_to_namespace_id_.erase(persistent_namespace_id);
  namespaces_.erase(ns->namespace_id());
}

void DOMStorageContextImpl::CloneSessionNamespace(
    int64 existing_id, int64 new_id,
    const std::string& new_persistent_id) {
  if (is_shutdown_)
    return;
  DCHECK_NE(kLocalStorageNamespaceId, existing_id);
  DCHECK_NE(kLocalStorageNamespaceId, new_id);
  StorageNamespaceMap::iterator found = namespaces_.find(existing_id);
  if (found != namespaces_.end())
    namespaces_[new_id] = found->second->Clone(new_id, new_persistent_id);
  else
    CreateSessionNamespace(new_id, new_persistent_id);
}

void DOMStorageContextImpl::CreateAliasSessionNamespace(
    int64 existing_id, int64 new_id,
    const std::string& persistent_id) {
  if (is_shutdown_)
    return;
  DCHECK_NE(kLocalStorageNamespaceId, existing_id);
  DCHECK_NE(kLocalStorageNamespaceId, new_id);
  StorageNamespaceMap::iterator found = namespaces_.find(existing_id);
  if (found != namespaces_.end()) {
    namespaces_[new_id] = found->second->CreateAlias(new_id);
  } else {
    CreateSessionNamespace(new_id, persistent_id);
  }
}

void DOMStorageContextImpl::ClearSessionOnlyOrigins() {
  if (!localstorage_directory_.empty()) {
    std::vector<LocalStorageUsageInfo> infos;
    const bool kDontIncludeFileInfo = false;
    GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
    for (size_t i = 0; i < infos.size(); ++i) {
      const GURL& origin = infos[i].origin;
      if (special_storage_policy_->IsStorageProtected(origin))
        continue;
      if (!special_storage_policy_->IsStorageSessionOnly(origin))
        continue;

      base::FilePath database_file_path = localstorage_directory_.Append(
          DOMStorageArea::DatabaseFileNameFromOrigin(origin));
      sql::Connection::Delete(database_file_path);
    }
  }
  if (session_storage_database_.get()) {
    std::vector<SessionStorageUsageInfo> infos;
    GetSessionStorageUsage(&infos);
    for (size_t i = 0; i < infos.size(); ++i) {
      const GURL& origin = infos[i].origin;
      if (special_storage_policy_->IsStorageProtected(origin))
        continue;
      if (!special_storage_policy_->IsStorageSessionOnly(origin))
        continue;
      session_storage_database_->DeleteArea(infos[i].persistent_namespace_id,
                                            origin);
    }
  }
}

void DOMStorageContextImpl::SetSaveSessionStorageOnDisk() {
  DCHECK(namespaces_.empty());
  if (!sessionstorage_directory_.empty()) {
    session_storage_database_ = new SessionStorageDatabase(
        sessionstorage_directory_);
  }
}

void DOMStorageContextImpl::StartScavengingUnusedSessionStorage() {
  if (session_storage_database_.get()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&DOMStorageContextImpl::FindUnusedNamespaces,
                              this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

void DOMStorageContextImpl::FindUnusedNamespaces() {
  DCHECK(session_storage_database_.get());
  if (scavenging_started_)
    return;
  scavenging_started_ = true;
  std::set<std::string> namespace_ids_in_use;
  for (StorageNamespaceMap::const_iterator it = namespaces_.begin();
       it != namespaces_.end(); ++it)
    namespace_ids_in_use.insert(it->second->persistent_namespace_id());
  std::set<std::string> protected_persistent_session_ids;
  protected_persistent_session_ids.swap(protected_persistent_session_ids_);
  task_runner_->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::COMMIT_SEQUENCE,
      base::Bind(
          &DOMStorageContextImpl::FindUnusedNamespacesInCommitSequence,
          this, namespace_ids_in_use, protected_persistent_session_ids));
}

void DOMStorageContextImpl::FindUnusedNamespacesInCommitSequence(
    const std::set<std::string>& namespace_ids_in_use,
    const std::set<std::string>& protected_persistent_session_ids) {
  DCHECK(session_storage_database_.get());
  // Delete all namespaces which don't have an associated DOMStorageNamespace
  // alive.
  std::map<std::string, std::vector<GURL> > namespaces_and_origins;
  session_storage_database_->ReadNamespacesAndOrigins(&namespaces_and_origins);
  for (std::map<std::string, std::vector<GURL> >::const_iterator it =
           namespaces_and_origins.begin();
       it != namespaces_and_origins.end(); ++it) {
    if (namespace_ids_in_use.find(it->first) == namespace_ids_in_use.end() &&
        protected_persistent_session_ids.find(it->first) ==
        protected_persistent_session_ids.end()) {
      deletable_persistent_namespace_ids_.push_back(it->first);
    }
  }
  if (!deletable_persistent_namespace_ids_.empty()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(
            &DOMStorageContextImpl::DeleteNextUnusedNamespace,
            this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

void DOMStorageContextImpl::DeleteNextUnusedNamespace() {
  if (is_shutdown_)
    return;
  task_runner_->PostShutdownBlockingTask(
        FROM_HERE, DOMStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(
            &DOMStorageContextImpl::DeleteNextUnusedNamespaceInCommitSequence,
            this));
}

void DOMStorageContextImpl::DeleteNextUnusedNamespaceInCommitSequence() {
  if (deletable_persistent_namespace_ids_.empty())
    return;
  const std::string& persistent_id = deletable_persistent_namespace_ids_.back();
  session_storage_database_->DeleteNamespace(persistent_id);
  deletable_persistent_namespace_ids_.pop_back();
  if (!deletable_persistent_namespace_ids_.empty()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(
            &DOMStorageContextImpl::DeleteNextUnusedNamespace,
            this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

void DOMStorageContextImpl::AddTransactionLogProcessId(int64 namespace_id,
                                                       int process_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
  StorageNamespaceMap::const_iterator it = namespaces_.find(namespace_id);
  if (it == namespaces_.end())
    return;
  it->second->AddTransactionLogProcessId(process_id);
}

void DOMStorageContextImpl::RemoveTransactionLogProcessId(int64 namespace_id,
                                                       int process_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
  StorageNamespaceMap::const_iterator it = namespaces_.find(namespace_id);
  if (it == namespaces_.end())
    return;
  it->second->RemoveTransactionLogProcessId(process_id);
}

SessionStorageNamespace::MergeResult
DOMStorageContextImpl::MergeSessionStorage(
    int64 namespace1_id, bool actually_merge, int process_id,
    int64 namespace2_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace1_id);
  DCHECK_NE(kLocalStorageNamespaceId, namespace2_id);
  StorageNamespaceMap::const_iterator it = namespaces_.find(namespace1_id);
  if (it == namespaces_.end())
    return SessionStorageNamespace::MERGE_RESULT_NAMESPACE_NOT_FOUND;
  DOMStorageNamespace* ns1 = it->second;
  it = namespaces_.find(namespace2_id);
  if (it == namespaces_.end())
    return SessionStorageNamespace::MERGE_RESULT_NAMESPACE_NOT_FOUND;
  DOMStorageNamespace* ns2 = it->second;
  return ns1->Merge(actually_merge, process_id, ns2, this);
}

}  // namespace content
