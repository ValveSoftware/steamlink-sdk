// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_context_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_quota_client.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_info.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/database/database_identifier.h"
#include "ui/base/text/bytes_formatting.h"
#include "url/origin.h"

using base::DictionaryValue;
using base::ListValue;
using storage::DatabaseUtil;
using url::Origin;

namespace content {
const base::FilePath::CharType IndexedDBContextImpl::kIndexedDBDirectory[] =
    FILE_PATH_LITERAL("IndexedDB");

static const base::FilePath::CharType kBlobExtension[] =
    FILE_PATH_LITERAL(".blob");

static const base::FilePath::CharType kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".indexeddb");

static const base::FilePath::CharType kLevelDBExtension[] =
    FILE_PATH_LITERAL(".leveldb");

namespace {

// This may be called after the IndexedDBContext is destroyed.
void GetAllOriginsAndPaths(const base::FilePath& indexeddb_path,
                           std::vector<Origin>* origins,
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
      origins->push_back(Origin(storage::GetOriginFromIdentifier(origin_id)));
      if (file_paths)
        file_paths->push_back(file_path);
    }
  }
}

// This will be called after the IndexedDBContext is destroyed.
void ClearSessionOnlyOrigins(
    const base::FilePath& indexeddb_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  // TODO(jsbell): DCHECK that this is running on an IndexedDB thread,
  // if a global handle to it is ever available.
  std::vector<Origin> origins;
  std::vector<base::FilePath> file_paths;
  GetAllOriginsAndPaths(indexeddb_path, &origins, &file_paths);
  DCHECK_EQ(origins.size(), file_paths.size());
  auto file_path = file_paths.cbegin();
  auto origin = origins.cbegin();
  for (; origin != origins.cend(); ++origin, ++file_path) {
    const GURL origin_url = GURL(origin->Serialize());
    if (!special_storage_policy->IsStorageSessionOnly(origin_url))
      continue;
    if (special_storage_policy->IsStorageProtected(origin_url))
      continue;
    base::DeleteFile(*file_path, true);
  }
}

}  // namespace

IndexedDBContextImpl::IndexedDBContextImpl(
    const base::FilePath& data_path,
    storage::SpecialStoragePolicy* special_storage_policy,
    storage::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* task_runner)
    : force_keep_session_state_(false),
      special_storage_policy_(special_storage_policy),
      quota_manager_proxy_(quota_manager_proxy),
      task_runner_(task_runner) {
  IDB_TRACE("init");
  if (!data_path.empty())
    data_path_ = data_path.Append(kIndexedDBDirectory);
  quota_manager_proxy->RegisterClient(new IndexedDBQuotaClient(this));
}

IndexedDBFactory* IndexedDBContextImpl::GetIDBFactory() {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (!factory_.get()) {
    // Prime our cache of origins with existing databases so we can
    // detect when dbs are newly created.
    GetOriginSet();
    factory_ = new IndexedDBFactoryImpl(this);
  }
  return factory_.get();
}

std::vector<Origin> IndexedDBContextImpl::GetAllOrigins() {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  std::set<Origin>* origins_set = GetOriginSet();
  return std::vector<Origin>(origins_set->begin(), origins_set->end());
}

bool IndexedDBContextImpl::HasOrigin(const Origin& origin) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  std::set<Origin>* set = GetOriginSet();
  return set->find(origin) != set->end();
}

std::vector<IndexedDBInfo> IndexedDBContextImpl::GetAllOriginsInfo() {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<Origin> origins = GetAllOrigins();
  std::vector<IndexedDBInfo> result;
  for (const auto& origin : origins) {
    size_t connection_count = GetConnectionCount(origin);
    result.push_back(
        IndexedDBInfo(GURL(origin.Serialize()), GetOriginDiskUsage(origin),
                      GetOriginLastModified(origin), connection_count));
  }
  return result;
}

static bool HostNameComparator(const Origin& i, const Origin& j) {
  return i.host() < j.host();
}

base::ListValue* IndexedDBContextImpl::GetAllOriginsDetails() {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<Origin> origins = GetAllOrigins();

  std::sort(origins.begin(), origins.end(), HostNameComparator);

  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (const auto& origin : origins) {
    std::unique_ptr<base::DictionaryValue> info(new base::DictionaryValue());
    info->SetString("url", origin.Serialize());
    info->SetString("size", ui::FormatBytes(GetOriginDiskUsage(origin)));
    info->SetDouble("last_modified", GetOriginLastModified(origin).ToJsTime());
    if (!is_incognito()) {
      std::unique_ptr<base::ListValue> paths(new base::ListValue());
      for (const base::FilePath& path : GetStoragePaths(origin))
        paths->AppendString(path.value());
      info->Set("paths", paths.release());
    }
    info->SetDouble("connection_count", GetConnectionCount(origin));

    // This ends up being O(n^2) since we iterate over all open databases
    // to extract just those in the origin, and we're iterating over all
    // origins in the outer loop.

    if (factory_.get()) {
      std::pair<IndexedDBFactory::OriginDBMapIterator,
                IndexedDBFactory::OriginDBMapIterator>
          range = factory_->GetOpenDatabasesForOrigin(origin);
      // TODO(jsbell): Sort by name?
      std::unique_ptr<base::ListValue> database_list(new base::ListValue());

      for (IndexedDBFactory::OriginDBMapIterator it = range.first;
           it != range.second;
           ++it) {
        const IndexedDBDatabase* db = it->second;
        std::unique_ptr<base::DictionaryValue> db_info(
            new base::DictionaryValue());

        db_info->SetString("name", db->name());
        db_info->SetDouble("pending_opens", db->PendingOpenCount());
        db_info->SetDouble("pending_upgrades", db->PendingUpgradeCount());
        db_info->SetDouble("running_upgrades", db->RunningUpgradeCount());
        db_info->SetDouble("pending_deletes", db->PendingDeleteCount());
        db_info->SetDouble("connection_count",
                           db->ConnectionCount() - db->PendingUpgradeCount() -
                               db->RunningUpgradeCount());

        std::unique_ptr<base::ListValue> transaction_list(
            new base::ListValue());
        std::vector<const IndexedDBTransaction*> transactions =
            db->transaction_coordinator().GetTransactions();
        for (const auto* transaction : transactions) {
          std::unique_ptr<base::DictionaryValue> transaction_info(
              new base::DictionaryValue());

          const char* const kModes[] =
              { "readonly", "readwrite", "versionchange" };
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

          std::unique_ptr<base::ListValue> scope(new base::ListValue());
          for (const auto& id : transaction->scope()) {
            IndexedDBDatabaseMetadata::ObjectStoreMap::const_iterator it =
                db->metadata().object_stores.find(id);
            if (it != db->metadata().object_stores.end())
              scope->AppendString(it->second.name);
          }

          transaction_info->Set("scope", scope.release());
          transaction_list->Append(std::move(transaction_info));
        }
        db_info->Set("transactions", transaction_list.release());

        database_list->Append(std::move(db_info));
      }
      info->Set("databases", database_list.release());
    }

    list->Append(std::move(info));
  }
  return list.release();
}

int IndexedDBContextImpl::GetOriginBlobFileCount(const Origin& origin) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  int count = 0;
  base::FileEnumerator file_enumerator(GetBlobStorePath(origin), true,
                                       base::FileEnumerator::FILES);
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

// TODO(jsbell): Update callers to use url::Origin overload and remove.
int64_t IndexedDBContextImpl::GetOriginDiskUsage(const GURL& origin_url) {
  return GetOriginDiskUsage(Origin(origin_url));
}

int64_t IndexedDBContextImpl::GetOriginDiskUsage(const Origin& origin) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (data_path_.empty() || !HasOrigin(origin))
    return 0;
  EnsureDiskUsageCacheInitialized(origin);
  return origin_size_map_[origin];
}

base::Time IndexedDBContextImpl::GetOriginLastModified(const Origin& origin) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (data_path_.empty() || !HasOrigin(origin))
    return base::Time();
  base::FilePath idb_directory = GetLevelDBPath(origin);
  base::File::Info file_info;
  if (!base::GetFileInfo(idb_directory, &file_info))
    return base::Time();
  return file_info.last_modified;
}

// TODO(jsbell): Update callers to use url::Origin overload and remove.
void IndexedDBContextImpl::DeleteForOrigin(const GURL& origin_url) {
  DeleteForOrigin(Origin(origin_url));
}

void IndexedDBContextImpl::DeleteForOrigin(const Origin& origin) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  ForceClose(origin, FORCE_CLOSE_DELETE_ORIGIN);
  if (data_path_.empty() || !HasOrigin(origin))
    return;

  base::FilePath idb_directory = GetLevelDBPath(origin);
  EnsureDiskUsageCacheInitialized(origin);
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
  base::DeleteFile(GetBlobStorePath(origin), true /* recursive */);
  QueryDiskAndUpdateQuotaUsage(origin);
  if (s.ok()) {
    RemoveFromOriginSet(origin);
    origin_size_map_.erase(origin);
  }
}

// TODO(jsbell): Update callers to use url::Origin overload and remove.
void IndexedDBContextImpl::CopyOriginData(const GURL& origin_url,
                                          IndexedDBContext* dest_context) {
  CopyOriginData(Origin(origin_url), dest_context);
}

void IndexedDBContextImpl::CopyOriginData(const Origin& origin,
                                          IndexedDBContext* dest_context) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (data_path_.empty() || !HasOrigin(origin))
    return;

  IndexedDBContextImpl* dest_context_impl =
      static_cast<IndexedDBContextImpl*>(dest_context);

  ForceClose(origin, FORCE_CLOSE_COPY_ORIGIN);

  // Make sure we're not about to delete our own database.
  CHECK_NE(dest_context_impl->data_path().value(), data_path().value());

  // Delete any existing storage paths in the destination context.
  // A previously failed migration may have left behind partially copied
  // directories.
  for (const base::FilePath& dest_path :
       dest_context_impl->GetStoragePaths(origin))
    base::DeleteFile(dest_path, true);

  base::FilePath dest_data_path = dest_context_impl->data_path();
  base::CreateDirectory(dest_data_path);

  for (const base::FilePath& src_data_path : GetStoragePaths(origin)) {
    if (base::PathExists(src_data_path)) {
      base::CopyDirectory(src_data_path, dest_data_path, true);
    }
  }
}

void IndexedDBContextImpl::ForceClose(const Origin origin,
                                      ForceCloseReason reason) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("WebCore.IndexedDB.Context.ForceCloseReason",
                            reason,
                            FORCE_CLOSE_REASON_MAX);

  if (data_path_.empty() || !HasOrigin(origin))
    return;

  if (factory_.get())
    factory_->ForceClose(origin);
  DCHECK_EQ(0UL, GetConnectionCount(origin));
}

size_t IndexedDBContextImpl::GetConnectionCount(const Origin& origin) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  if (data_path_.empty() || !HasOrigin(origin))
    return 0;

  if (!factory_.get())
    return 0;

  return factory_->GetConnectionCount(origin);
}

std::vector<base::FilePath> IndexedDBContextImpl::GetStoragePaths(
    const Origin& origin) const {
  std::vector<base::FilePath> paths;
  paths.push_back(GetLevelDBPath(origin));
  paths.push_back(GetBlobStorePath(origin));
  return paths;
}

// TODO(jsbell): Update callers to use url::Origin overload and remove.
base::FilePath IndexedDBContextImpl::GetFilePathForTesting(
    const GURL& origin_url) const {
  return GetFilePathForTesting(Origin(origin_url));
}

base::FilePath IndexedDBContextImpl::GetFilePathForTesting(
    const Origin& origin) const {
  return GetLevelDBPath(origin);
}

void IndexedDBContextImpl::SetTaskRunnerForTesting(
    base::SequencedTaskRunner* task_runner) {
  DCHECK(!task_runner_.get());
  task_runner_ = task_runner;
}

void IndexedDBContextImpl::ConnectionOpened(const Origin& origin,
                                            IndexedDBConnection* connection) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  quota_manager_proxy()->NotifyStorageAccessed(
      storage::QuotaClient::kIndexedDatabase, GURL(origin.Serialize()),
      storage::kStorageTypeTemporary);
  if (AddToOriginSet(origin)) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(origin);
  } else {
    EnsureDiskUsageCacheInitialized(origin);
  }
}

void IndexedDBContextImpl::ConnectionClosed(const Origin& origin,
                                            IndexedDBConnection* connection) {
  DCHECK(TaskRunner()->RunsTasksOnCurrentThread());
  quota_manager_proxy()->NotifyStorageAccessed(
      storage::QuotaClient::kIndexedDatabase, GURL(origin.Serialize()),
      storage::kStorageTypeTemporary);
  if (factory_.get() && factory_->GetConnectionCount(origin) == 0)
    QueryDiskAndUpdateQuotaUsage(origin);
}

void IndexedDBContextImpl::TransactionComplete(const Origin& origin) {
  DCHECK(!factory_.get() || factory_->GetConnectionCount(origin) > 0);
  QueryDiskAndUpdateQuotaUsage(origin);
}

void IndexedDBContextImpl::DatabaseDeleted(const Origin& origin) {
  AddToOriginSet(origin);
  QueryDiskAndUpdateQuotaUsage(origin);
}

IndexedDBContextImpl::~IndexedDBContextImpl() {
  if (factory_.get()) {
    TaskRunner()->PostTask(
        FROM_HERE, base::Bind(&IndexedDBFactory::ContextDestroyed, factory_));
    factory_ = NULL;
  }

  if (data_path_.empty())
    return;

  if (force_keep_session_state_)
    return;

  bool has_session_only_databases =
      special_storage_policy_.get() &&
      special_storage_policy_->HasSessionOnlyOrigins();

  // Clearing only session-only databases, and there are none.
  if (!has_session_only_databases)
    return;

  TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &ClearSessionOnlyOrigins, data_path_, special_storage_policy_));
}

// static
base::FilePath IndexedDBContextImpl::GetBlobStoreFileName(
    const Origin& origin) {
  std::string origin_id =
      storage::GetIdentifierFromOrigin(GURL(origin.Serialize()));
  return base::FilePath()
      .AppendASCII(origin_id)
      .AddExtension(kIndexedDBExtension)
      .AddExtension(kBlobExtension);
}

// static
base::FilePath IndexedDBContextImpl::GetLevelDBFileName(const Origin& origin) {
  std::string origin_id =
      storage::GetIdentifierFromOrigin(GURL(origin.Serialize()));
  return base::FilePath()
      .AppendASCII(origin_id)
      .AddExtension(kIndexedDBExtension)
      .AddExtension(kLevelDBExtension);
}

base::FilePath IndexedDBContextImpl::GetBlobStorePath(
    const Origin& origin) const {
  DCHECK(!data_path_.empty());
  return data_path_.Append(GetBlobStoreFileName(origin));
}

base::FilePath IndexedDBContextImpl::GetLevelDBPath(
    const Origin& origin) const {
  DCHECK(!data_path_.empty());
  return data_path_.Append(GetLevelDBFileName(origin));
}

int64_t IndexedDBContextImpl::ReadUsageFromDisk(const Origin& origin) const {
  if (data_path_.empty())
    return 0;
  int64_t total_size = 0;
  for (const base::FilePath& path : GetStoragePaths(origin))
    total_size += base::ComputeDirectorySize(path);
  return total_size;
}

void IndexedDBContextImpl::EnsureDiskUsageCacheInitialized(
    const Origin& origin) {
  if (origin_size_map_.find(origin) == origin_size_map_.end())
    origin_size_map_[origin] = ReadUsageFromDisk(origin);
}

void IndexedDBContextImpl::QueryDiskAndUpdateQuotaUsage(const Origin& origin) {
  int64_t former_disk_usage = origin_size_map_[origin];
  int64_t current_disk_usage = ReadUsageFromDisk(origin);
  int64_t difference = current_disk_usage - former_disk_usage;
  if (difference) {
    origin_size_map_[origin] = current_disk_usage;
    quota_manager_proxy()->NotifyStorageModified(
        storage::QuotaClient::kIndexedDatabase, GURL(origin.Serialize()),
        storage::kStorageTypeTemporary, difference);
  }
}

std::set<Origin>* IndexedDBContextImpl::GetOriginSet() {
  if (!origin_set_) {
    std::vector<Origin> origins;
    GetAllOriginsAndPaths(data_path_, &origins, NULL);
    origin_set_.reset(new std::set<Origin>(origins.begin(), origins.end()));
  }
  return origin_set_.get();
}

void IndexedDBContextImpl::ResetCaches() {
  origin_set_.reset();
  origin_size_map_.clear();
}

base::SequencedTaskRunner* IndexedDBContextImpl::TaskRunner() const {
  return task_runner_.get();
}

}  // namespace content
