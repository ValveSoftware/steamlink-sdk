// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_context_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_info.h"
#include "content/public/common/content_switches.h"
#include "ui/base/text/bytes_formatting.h"
#include "webkit/browser/database/database_util.h"
#include "webkit/browser/quota/quota_manager_proxy.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/common/database/database_identifier.h"

using base::DictionaryValue;
using base::ListValue;
using webkit_database::DatabaseUtil;

namespace content {
const base::FilePath::CharType IndexedDBContextImpl::kIndexedDBDirectory[] =
    FILE_PATH_LITERAL("IndexedDB");

static const base::FilePath::CharType kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".indexeddb");

static const base::FilePath::CharType kLevelDBExtension[] =
    FILE_PATH_LITERAL(".leveldb");

namespace {

// This may be called after the IndexedDBContext is destroyed.
void GetAllOriginsAndPaths(const base::FilePath& indexeddb_path,
                           std::vector<GURL>* origins,
                           std::vector<base::FilePath>* file_paths) {
  // TODO(jsbell): DCHECK that this is running on an IndexedDB thread,
  // if a global handle to it is ever available.
  if (indexeddb_path.empty())
    return;
  base::FileEnumerator file_enumerator(
      indexeddb_path, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == kLevelDBExtension &&
        file_path.RemoveExtension().Extension() == kIndexedDBExtension) {
      std::string origin_id = file_path.BaseName().RemoveExtension()
          .RemoveExtension().MaybeAsASCII();
      origins->push_back(webkit_database::GetOriginFromIdentifier(origin_id));
      if (file_paths)
        file_paths->push_back(file_path);
    }
  }
}

// This will be called after the IndexedDBContext is destroyed.
void ClearSessionOnlyOrigins(
    const base::FilePath& indexeddb_path,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy) {
  // TODO(jsbell): DCHECK that this is running on an IndexedDB thread,
  // if a global handle to it is ever available.
  std::vector<GURL> origins;
  std::vector<base::FilePath> file_paths;
  GetAllOriginsAndPaths(indexeddb_path, &origins, &file_paths);
  DCHECK_EQ(origins.size(), file_paths.size());
  std::vector<base::FilePath>::const_iterator file_path_iter =
      file_paths.begin();
  for (std::vector<GURL>::const_iterator iter = origins.begin();
       iter != origins.end();
       ++iter, ++file_path_iter) {
    if (!special_storage_policy->IsStorageSessionOnly(*iter))
      continue;
    if (special_storage_policy->IsStorageProtected(*iter))
      continue;
    base::DeleteFile(*file_path_iter, true);
  }
}

}  // namespace

IndexedDBContextImpl::IndexedDBContextImpl(
    const base::FilePath& data_path,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* task_runner)
    : force_keep_session_state_(false),
      special_storage_policy_(special_storage_policy),
      quota_manager_proxy_(quota_manager_proxy),
      task_runner_(task_runner) {
  IDB_TRACE("init");
  if (!data_path.empty())
    data_path_ = data_path.Append(kIndexedDBDirectory);
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(new IndexedDBQuotaClient(this));
  }
}

IndexedDBFactory* IndexedDBContextImpl::GetIDBFactory() {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (!factory_) {
    // Prime our cache of origins with existing databases so we can
    // detect when dbs are newly created.
    GetOriginSet();
    factory_ = new IndexedDBFactory(this);
  }
  return factory_;
}

std::vector<GURL> IndexedDBContextImpl::GetAllOrigins() {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<GURL> origins;
  std::set<GURL>* origins_set = GetOriginSet();
  for (std::set<GURL>::const_iterator iter = origins_set->begin();
       iter != origins_set->end();
       ++iter) {
    origins.push_back(*iter);
  }
  return origins;
}

std::vector<IndexedDBInfo> IndexedDBContextImpl::GetAllOriginsInfo() {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<GURL> origins = GetAllOrigins();
  std::vector<IndexedDBInfo> result;
  for (std::vector<GURL>::const_iterator iter = origins.begin();
       iter != origins.end();
       ++iter) {
    const GURL& origin_url = *iter;

    base::FilePath idb_directory = GetFilePath(origin_url);
    size_t connection_count = GetConnectionCount(origin_url);
    result.push_back(IndexedDBInfo(origin_url,
                                   GetOriginDiskUsage(origin_url),
                                   GetOriginLastModified(origin_url),
                                   idb_directory,
                                   connection_count));
  }
  return result;
}

static bool HostNameComparator(const GURL& i, const GURL& j) {
  return i.host() < j.host();
}

base::ListValue* IndexedDBContextImpl::GetAllOriginsDetails() {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<GURL> origins = GetAllOrigins();

  std::sort(origins.begin(), origins.end(), HostNameComparator);

  scoped_ptr<base::ListValue> list(new base::ListValue());
  for (std::vector<GURL>::const_iterator iter = origins.begin();
       iter != origins.end();
       ++iter) {
    const GURL& origin_url = *iter;

    scoped_ptr<base::DictionaryValue> info(new base::DictionaryValue());
    info->SetString("url", origin_url.spec());
    info->SetString("size", ui::FormatBytes(GetOriginDiskUsage(origin_url)));
    info->SetDouble("last_modified",
                    GetOriginLastModified(origin_url).ToJsTime());
    info->SetString("path", GetFilePath(origin_url).value());
    info->SetDouble("connection_count", GetConnectionCount(origin_url));

    // This ends up being O(n^2) since we iterate over all open databases
    // to extract just those in the origin, and we're iterating over all
    // origins in the outer loop.

    if (factory_) {
      std::pair<IndexedDBFactory::OriginDBMapIterator,
                IndexedDBFactory::OriginDBMapIterator> range =
          factory_->GetOpenDatabasesForOrigin(origin_url);
      // TODO(jsbell): Sort by name?
      scoped_ptr<base::ListValue> database_list(new base::ListValue());

      for (IndexedDBFactory::OriginDBMapIterator it = range.first;
           it != range.second;
           ++it) {
        const IndexedDBDatabase* db = it->second;
        scoped_ptr<base::DictionaryValue> db_info(new base::DictionaryValue());

        db_info->SetString("name", db->name());
        db_info->SetDouble("pending_opens", db->PendingOpenCount());
        db_info->SetDouble("pending_upgrades", db->PendingUpgradeCount());
        db_info->SetDouble("running_upgrades", db->RunningUpgradeCount());
        db_info->SetDouble("pending_deletes", db->PendingDeleteCount());
        db_info->SetDouble("connection_count",
                           db->ConnectionCount() - db->PendingUpgradeCount() -
                               db->RunningUpgradeCount());

        scoped_ptr<base::ListValue> transaction_list(new base::ListValue());
        std::vector<const IndexedDBTransaction*> transactions =
            db->transaction_coordinator().GetTransactions();
        for (std::vector<const IndexedDBTransaction*>::iterator trans_it =
                 transactions.begin();
             trans_it != transactions.end();
             ++trans_it) {
          const IndexedDBTransaction* transaction = *trans_it;
          scoped_ptr<base::DictionaryValue> transaction_info(
              new base::DictionaryValue());

          const char* kModes[] = { "readonly", "readwrite", "versionchange" };
          transaction_info->SetString("mode", kModes[transaction->mode()]);
          switch (transaction->state()) {
            case IndexedDBTransaction::CREATED:
              transaction_info->SetString("status", "blocked");
              break;
            case IndexedDBTransaction::STARTED:
              if (transaction->diagnostics().tasks_scheduled > 0)
                transaction_info->SetString("status", "running");
              else
                transaction_info->SetString("status", "started");
              break;
            case IndexedDBTransaction::COMMITTING:
              transaction_info->SetString("status", "committing");
              break;
            case IndexedDBTransaction::FINISHED:
              transaction_info->SetString("status", "finished");
              break;
          }

          transaction_info->SetDouble(
              "pid",
              IndexedDBDispatcherHost::TransactionIdToProcessId(
                  transaction->id()));
          transaction_info->SetDouble(
              "tid",
              IndexedDBDispatcherHost::TransactionIdToRendererTransactionId(
                  transaction->id()));
          transaction_info->SetDouble(
              "age",
              (base::Time::Now() - transaction->diagnostics().creation_time)
                  .InMillisecondsF());
          transaction_info->SetDouble(
              "runtime",
              (base::Time::Now() - transaction->diagnostics().start_time)
                  .InMillisecondsF());
          transaction_info->SetDouble(
              "tasks_scheduled", transaction->diagnostics().tasks_scheduled);
          transaction_info->SetDouble(
              "tasks_completed", transaction->diagnostics().tasks_completed);

          scoped_ptr<base::ListValue> scope(new base::ListValue());
          for (std::set<int64>::const_iterator scope_it =
                   transaction->scope().begin();
               scope_it != transaction->scope().end();
               ++scope_it) {
            IndexedDBDatabaseMetadata::ObjectStoreMap::const_iterator it =
                db->metadata().object_stores.find(*scope_it);
            if (it != db->metadata().object_stores.end())
              scope->AppendString(it->second.name);
          }

          transaction_info->Set("scope", scope.release());
          transaction_list->Append(transaction_info.release());
        }
        db_info->Set("transactions", transaction_list.release());

        database_list->Append(db_info.release());
      }
      info->Set("databases", database_list.release());
    }

    list->Append(info.release());
  }
  return list.release();
}

int64 IndexedDBContextImpl::GetOriginDiskUsage(const GURL& origin_url) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (data_path_.empty() || !IsInOriginSet(origin_url))
    return 0;
  EnsureDiskUsageCacheInitialized(origin_url);
  return origin_size_map_[origin_url];
}

base::Time IndexedDBContextImpl::GetOriginLastModified(const GURL& origin_url) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (data_path_.empty() || !IsInOriginSet(origin_url))
    return base::Time();
  base::FilePath idb_directory = GetFilePath(origin_url);
  base::File::Info file_info;
  if (!base::GetFileInfo(idb_directory, &file_info))
    return base::Time();
  return file_info.last_modified;
}

void IndexedDBContextImpl::DeleteForOrigin(const GURL& origin_url) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  ForceClose(origin_url, FORCE_CLOSE_DELETE_ORIGIN);
  if (data_path_.empty() || !IsInOriginSet(origin_url))
    return;

  base::FilePath idb_directory = GetFilePath(origin_url);
  EnsureDiskUsageCacheInitialized(origin_url);
  leveldb::Status s = LevelDBDatabase::Destroy(idb_directory);
  if (!s.ok()) {
    LOG(WARNING) << "Failed to delete LevelDB database: "
                 << idb_directory.AsUTF8Unsafe();
  } else {
    // LevelDB does not delete empty directories; work around this.
    // TODO(jsbell): Remove when upstream bug is fixed.
    // https://code.google.com/p/leveldb/issues/detail?id=209
    const bool kNonRecursive = false;
    base::DeleteFile(idb_directory, kNonRecursive);
  }

  QueryDiskAndUpdateQuotaUsage(origin_url);
  if (s.ok()) {
    RemoveFromOriginSet(origin_url);
    origin_size_map_.erase(origin_url);
    space_available_map_.erase(origin_url);
  }
}

void IndexedDBContextImpl::ForceClose(const GURL origin_url,
                                      ForceCloseReason reason) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("WebCore.IndexedDB.Context.ForceCloseReason",
                            reason,
                            FORCE_CLOSE_REASON_MAX);

  if (data_path_.empty() || !IsInOriginSet(origin_url))
    return;

  if (factory_)
    factory_->ForceClose(origin_url);
  DCHECK_EQ(0UL, GetConnectionCount(origin_url));
}

size_t IndexedDBContextImpl::GetConnectionCount(const GURL& origin_url) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (data_path_.empty() || !IsInOriginSet(origin_url))
    return 0;

  if (!factory_)
    return 0;

  return factory_->GetConnectionCount(origin_url);
}

base::FilePath IndexedDBContextImpl::GetFilePath(const GURL& origin_url) const {
  std::string origin_id = webkit_database::GetIdentifierFromOrigin(origin_url);
  return GetIndexedDBFilePath(origin_id);
}

base::FilePath IndexedDBContextImpl::GetFilePathForTesting(
    const std::string& origin_id) const {
  return GetIndexedDBFilePath(origin_id);
}

void IndexedDBContextImpl::SetTaskRunnerForTesting(
    base::SequencedTaskRunner* task_runner) {
  DCHECK(!task_runner_);
  task_runner_ = task_runner;
}

void IndexedDBContextImpl::ConnectionOpened(const GURL& origin_url,
                                            IndexedDBConnection* connection) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (quota_manager_proxy()) {
    quota_manager_proxy()->NotifyStorageAccessed(
        quota::QuotaClient::kIndexedDatabase,
        origin_url,
        quota::kStorageTypeTemporary);
  }
  if (AddToOriginSet(origin_url)) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(origin_url);
  } else {
    EnsureDiskUsageCacheInitialized(origin_url);
  }
  QueryAvailableQuota(origin_url);
}

void IndexedDBContextImpl::ConnectionClosed(const GURL& origin_url,
                                            IndexedDBConnection* connection) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (quota_manager_proxy()) {
    quota_manager_proxy()->NotifyStorageAccessed(
        quota::QuotaClient::kIndexedDatabase,
        origin_url,
        quota::kStorageTypeTemporary);
  }
  if (factory_ && factory_->GetConnectionCount(origin_url) == 0)
    QueryDiskAndUpdateQuotaUsage(origin_url);
}

void IndexedDBContextImpl::TransactionComplete(const GURL& origin_url) {
  DCHECK(!factory_ || factory_->GetConnectionCount(origin_url) > 0);
  QueryDiskAndUpdateQuotaUsage(origin_url);
  QueryAvailableQuota(origin_url);
}

void IndexedDBContextImpl::DatabaseDeleted(const GURL& origin_url) {
  AddToOriginSet(origin_url);
  QueryDiskAndUpdateQuotaUsage(origin_url);
  QueryAvailableQuota(origin_url);
}

bool IndexedDBContextImpl::WouldBeOverQuota(const GURL& origin_url,
                                            int64 additional_bytes) {
  if (space_available_map_.find(origin_url) == space_available_map_.end()) {
    // We haven't heard back from the QuotaManager yet, just let it through.
    return false;
  }
  bool over_quota = additional_bytes > space_available_map_[origin_url];
  return over_quota;
}

bool IndexedDBContextImpl::IsOverQuota(const GURL& origin_url) {
  const int kOneAdditionalByte = 1;
  return WouldBeOverQuota(origin_url, kOneAdditionalByte);
}

quota::QuotaManagerProxy* IndexedDBContextImpl::quota_manager_proxy() {
  return quota_manager_proxy_;
}

IndexedDBContextImpl::~IndexedDBContextImpl() {
  if (factory_) {
    TaskRunner()->PostTask(
        FROM_HERE, base::Bind(&IndexedDBFactory::ContextDestroyed, factory_));
    factory_ = NULL;
  }

  if (data_path_.empty())
    return;

  if (force_keep_session_state_)
    return;

  bool has_session_only_databases =
      special_storage_policy_ &&
      special_storage_policy_->HasSessionOnlyOrigins();

  // Clearing only session-only databases, and there are none.
  if (!has_session_only_databases)
    return;

  TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &ClearSessionOnlyOrigins, data_path_, special_storage_policy_));
}

base::FilePath IndexedDBContextImpl::GetIndexedDBFilePath(
    const std::string& origin_id) const {
  DCHECK(!data_path_.empty());
  return data_path_.AppendASCII(origin_id).AddExtension(kIndexedDBExtension)
      .AddExtension(kLevelDBExtension);
}

int64 IndexedDBContextImpl::ReadUsageFromDisk(const GURL& origin_url) const {
  if (data_path_.empty())
    return 0;
  base::FilePath file_path = GetFilePath(origin_url);
  return base::ComputeDirectorySize(file_path);
}

void IndexedDBContextImpl::EnsureDiskUsageCacheInitialized(
    const GURL& origin_url) {
  if (origin_size_map_.find(origin_url) == origin_size_map_.end())
    origin_size_map_[origin_url] = ReadUsageFromDisk(origin_url);
}

void IndexedDBContextImpl::QueryDiskAndUpdateQuotaUsage(
    const GURL& origin_url) {
  int64 former_disk_usage = origin_size_map_[origin_url];
  int64 current_disk_usage = ReadUsageFromDisk(origin_url);
  int64 difference = current_disk_usage - former_disk_usage;
  if (difference) {
    origin_size_map_[origin_url] = current_disk_usage;
    // quota_manager_proxy() is NULL in unit tests.
    if (quota_manager_proxy()) {
      quota_manager_proxy()->NotifyStorageModified(
          quota::QuotaClient::kIndexedDatabase,
          origin_url,
          quota::kStorageTypeTemporary,
          difference);
    }
  }
}

void IndexedDBContextImpl::GotUsageAndQuota(const GURL& origin_url,
                                            quota::QuotaStatusCode status,
                                            int64 usage,
                                            int64 quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(status == quota::kQuotaStatusOk || status == quota::kQuotaErrorAbort)
      << "status was " << status;
  if (status == quota::kQuotaErrorAbort) {
    // We seem to no longer care to wait around for the answer.
    return;
  }
  TaskRunner()->PostTask(FROM_HERE,
                         base::Bind(&IndexedDBContextImpl::GotUpdatedQuota,
                                    this,
                                    origin_url,
                                    usage,
                                    quota));
}

void IndexedDBContextImpl::GotUpdatedQuota(const GURL& origin_url,
                                           int64 usage,
                                           int64 quota) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  space_available_map_[origin_url] = quota - usage;
}

void IndexedDBContextImpl::QueryAvailableQuota(const GURL& origin_url) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
    if (quota_manager_proxy()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(
              &IndexedDBContextImpl::QueryAvailableQuota, this, origin_url));
    }
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!quota_manager_proxy() || !quota_manager_proxy()->quota_manager())
    return;
  quota_manager_proxy()->quota_manager()->GetUsageAndQuota(
      origin_url,
      quota::kStorageTypeTemporary,
      base::Bind(&IndexedDBContextImpl::GotUsageAndQuota, this, origin_url));
}

std::set<GURL>* IndexedDBContextImpl::GetOriginSet() {
  if (!origin_set_) {
    origin_set_.reset(new std::set<GURL>);
    std::vector<GURL> origins;
    GetAllOriginsAndPaths(data_path_, &origins, NULL);
    for (std::vector<GURL>::const_iterator iter = origins.begin();
         iter != origins.end();
         ++iter) {
      origin_set_->insert(*iter);
    }
  }
  return origin_set_.get();
}

void IndexedDBContextImpl::ResetCaches() {
  origin_set_.reset();
  origin_size_map_.clear();
  space_available_map_.clear();
}

base::TaskRunner* IndexedDBContextImpl::TaskRunner() const {
  return task_runner_;
}

}  // namespace content
