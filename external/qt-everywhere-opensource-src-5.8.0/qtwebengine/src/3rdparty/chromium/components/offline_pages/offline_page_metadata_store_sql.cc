// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_metadata_store_sql.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/offline_page_item.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

// This is a macro instead of a const so that
// it can be used inline in other SQL statements below.
#define OFFLINE_PAGES_TABLE_NAME "offlinepages_v1"

// New columns should be added at the end of the list in order to avoid
// complicated table upgrade.
const char kOfflinePagesColumns[] =
    "(offline_id INTEGER PRIMARY KEY NOT NULL,"
    " creation_time INTEGER NOT NULL,"
    " file_size INTEGER NOT NULL,"
    " version INTEGER NOT NULL,"
    " last_access_time INTEGER NOT NULL,"
    " access_count INTEGER NOT NULL,"
    " status INTEGER NOT NULL DEFAULT 0,"
    // A note on this field:  It will be NULL for now and is reserved for
    // later use.  We will treat NULL as "Unknown" in any subsequent queries
    // for user_initiated values.
    " user_initiated INTEGER,"  // this is actually a boolean
    " expiration_time INTEGER NOT NULL DEFAULT 0,"
    " client_namespace VARCHAR NOT NULL,"
    " client_id VARCHAR NOT NULL,"
    " online_url VARCHAR NOT NULL,"
    " offline_url VARCHAR NOT NULL DEFAULT '',"
    " file_path VARCHAR NOT NULL"
    ")";

// This is cloned from //content/browser/appcache/appcache_database.cc
struct TableInfo {
  const char* table_name;
  const char* columns;
};

const TableInfo kOfflinePagesTable{OFFLINE_PAGES_TABLE_NAME,
                                   kOfflinePagesColumns};

// This enum is used to define the indices for the columns in each row
// that hold the different pieces of offline page.
enum : int {
  OP_OFFLINE_ID = 0,
  OP_CREATION_TIME,
  OP_FILE_SIZE,
  OP_VERSION,
  OP_LAST_ACCESS_TIME,
  OP_ACCESS_COUNT,
  OP_STATUS,
  OP_USER_INITIATED,
  OP_EXPIRATION_TIME,
  OP_CLIENT_NAMESPACE,
  OP_CLIENT_ID,
  OP_ONLINE_URL,
  OP_OFFLINE_URL,
  OP_FILE_PATH,
};

bool CreateTable(sql::Connection* db, const TableInfo& table_info) {
  std::string sql("CREATE TABLE ");
  sql += table_info.table_name;
  sql += table_info.columns;
  return db->Execute(sql.c_str());
}

bool RefreshColumns(sql::Connection* db) {
  if (!db->Execute("ALTER TABLE " OFFLINE_PAGES_TABLE_NAME
                   " RENAME TO temp_" OFFLINE_PAGES_TABLE_NAME)) {
    return false;
  }
  if (!CreateTable(db, kOfflinePagesTable))
    return false;
  if (!db->Execute(
          "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
          " (offline_id, creation_time, file_size, version, last_access_time, "
          "access_count, status, user_initiated, client_namespace, client_id, "
          "online_url, offline_url, file_path) "
          "SELECT offline_id, creation_time, file_size, version, "
          "last_access_time, "
          "access_count, status, user_initiated, client_namespace, client_id, "
          "online_url, offline_url, file_path "
          "FROM temp_" OFFLINE_PAGES_TABLE_NAME)) {
    return false;
  }
  if (!db->Execute("DROP TABLE IF EXISTS temp_" OFFLINE_PAGES_TABLE_NAME))
    return false;

  return true;
}

bool CreateSchema(sql::Connection* db) {
  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  if (!db->DoesTableExist(kOfflinePagesTable.table_name)) {
    if (!CreateTable(db, kOfflinePagesTable))
      return false;
  }

  if (!db->DoesColumnExist(kOfflinePagesTable.table_name, "expiration_time")) {
    if (!RefreshColumns(db))
      return false;
  }

  // TODO(bburns): Add indices here.
  return transaction.Commit();
}

bool DeleteByOfflineId(sql::Connection* db, int64_t offline_id) {
  static const char kSql[] =
      "DELETE FROM " OFFLINE_PAGES_TABLE_NAME " WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  return statement.Run();
}

// Create an offline page item from a SQL result.  Expects complete rows with
// all columns present.
OfflinePageItem MakeOfflinePageItem(sql::Statement* statement) {
  int64_t id = statement->ColumnInt64(OP_OFFLINE_ID);
  GURL url(statement->ColumnString(OP_ONLINE_URL));
  ClientId client_id(statement->ColumnString(OP_CLIENT_NAMESPACE),
                     statement->ColumnString(OP_CLIENT_ID));
#if defined(OS_POSIX)
  base::FilePath path(statement->ColumnString(OP_FILE_PATH));
#elif defined(OS_WIN)
  base::FilePath path(base::UTF8ToWide(statement->ColumnString(OP_FILE_PATH)));
#else
#error Unknown OS
#endif
  int64_t file_size = statement->ColumnInt64(OP_FILE_SIZE);
  base::Time creation_time =
      base::Time::FromInternalValue(statement->ColumnInt64(OP_CREATION_TIME));

  OfflinePageItem item(url, id, client_id, path, file_size, creation_time);
  item.last_access_time = base::Time::FromInternalValue(
      statement->ColumnInt64(OP_LAST_ACCESS_TIME));
  item.version = statement->ColumnInt(OP_VERSION);
  item.access_count = statement->ColumnInt(OP_ACCESS_COUNT);
  item.expiration_time =
      base::Time::FromInternalValue(statement->ColumnInt64(OP_EXPIRATION_TIME));
  return item;
}

bool InsertOrReplace(sql::Connection* db, const OfflinePageItem& item) {
  const char kSql[] =
      "INSERT OR REPLACE INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, online_url, client_namespace, client_id, file_path, "
      "file_size, creation_time, last_access_time, version, access_count, "
      "expiration_time)"
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, item.offline_id);
  statement.BindString(1, item.url.spec());
  statement.BindString(2, item.client_id.name_space);
  statement.BindString(3, item.client_id.id);
#if defined(OS_POSIX)
  std::string path_string = item.file_path.value();
#elif defined(OS_WIN)
  std::string path_string = base::WideToUTF8(item.file_path.value());
#else
#error Unknown OS
#endif
  statement.BindString(4, path_string);
  statement.BindInt64(5, item.file_size);
  statement.BindInt64(6, item.creation_time.ToInternalValue());
  statement.BindInt64(7, item.last_access_time.ToInternalValue());
  statement.BindInt(8, item.version);
  statement.BindInt(9, item.access_count);
  statement.BindInt64(10, item.expiration_time.ToInternalValue());
  return statement.Run();
}

bool InitDatabase(sql::Connection* db, base::FilePath path) {
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("OfflinePageMetadata");
  db->set_exclusive_locking();

  base::File::Error err;
  if (!base::CreateDirectoryAndGetError(path.DirName(), &err)) {
    LOG(ERROR) << "Failed to create offline pages db directory: "
               << base::File::ErrorToString(err);
    return false;
  }
  if (!db->Open(path)) {
    LOG(ERROR) << "Failed to open database";
    return false;
  }
  db->Preload();

  return CreateSchema(db);
}

}  // anonymous namespace

OfflinePageMetadataStoreSQL::OfflinePageMetadataStoreSQL(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : background_task_runner_(std::move(background_task_runner)),
      db_file_path_(path.AppendASCII("OfflinePages.db")) {}

OfflinePageMetadataStoreSQL::~OfflinePageMetadataStoreSQL() {
  if (db_.get() &&
      !background_task_runner_->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "SQL database will not be deleted.";
  }
}

void OfflinePageMetadataStoreSQL::LoadSync(
    sql::Connection* db,
    const base::FilePath& path,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const LoadCallback& callback) {
  if (!InitDatabase(db, path)) {
    NotifyLoadResult(runner, callback, STORE_INIT_FAILED,
                     std::vector<OfflinePageItem>());
    return;
  }

  const char kSql[] = "SELECT * FROM " OFFLINE_PAGES_TABLE_NAME;

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));

  std::vector<OfflinePageItem> result;
  while (statement.Step()) {
    result.push_back(MakeOfflinePageItem(&statement));
  }

  if (statement.Succeeded()) {
    NotifyLoadResult(runner, callback, LOAD_SUCCEEDED, result);
  } else {
    NotifyLoadResult(runner, callback, STORE_LOAD_FAILED,
                     std::vector<OfflinePageItem>());
  }
}

void OfflinePageMetadataStoreSQL::AddOrUpdateOfflinePageSync(
    const OfflinePageItem& offline_page,
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const UpdateCallback& callback) {
  // TODO(bburns): add UMA metrics here (and for levelDB).
  bool ok = InsertOrReplace(db, offline_page);
  runner->PostTask(FROM_HERE, base::Bind(callback, ok));
}

void OfflinePageMetadataStoreSQL::RemoveOfflinePagesSync(
    const std::vector<int64_t>& offline_ids,
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const UpdateCallback& callback) {
  // TODO(bburns): add UMA metrics here (and for levelDB).

  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    runner->PostTask(FROM_HERE, base::Bind(callback, false));
    return;
  }
  for (auto offline_id : offline_ids) {
    if (!DeleteByOfflineId(db, offline_id)) {
      runner->PostTask(FROM_HERE, base::Bind(callback, false));
      return;
    }
  }

  bool success = transaction.Commit();
  runner->PostTask(FROM_HERE, base::Bind(callback, success));
}

void OfflinePageMetadataStoreSQL::ResetSync(
    std::unique_ptr<sql::Connection> db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const ResetCallback& callback) {
  const char kSql[] = "DELETE FROM " OFFLINE_PAGES_TABLE_NAME;
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  runner->PostTask(FROM_HERE, base::Bind(callback, statement.Run()));
}

void OfflinePageMetadataStoreSQL::NotifyLoadResult(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const LoadCallback& callback,
    LoadStatus status,
    const std::vector<OfflinePageItem>& result) {
  // TODO(bburns): Switch to SQL specific UMA metrics.
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.LoadStatus", status,
                            OfflinePageMetadataStore::LOAD_STATUS_COUNT);
  if (status == LOAD_SUCCEEDED) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.SavedPageCount",
                         static_cast<int32_t>(result.size()));
  } else {
    DVLOG(1) << "Offline pages database loading failed: " << status;
  }
  runner->PostTask(FROM_HERE, base::Bind(callback, status, result));
}

void OfflinePageMetadataStoreSQL::Load(const LoadCallback& callback) {
  db_.reset(new sql::Connection());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OfflinePageMetadataStoreSQL::LoadSync, db_.get(),
                 db_file_path_, base::ThreadTaskRunnerHandle::Get(), callback));
}

void OfflinePageMetadataStoreSQL::AddOrUpdateOfflinePage(
    const OfflinePageItem& offline_page,
    const UpdateCallback& callback) {
  DCHECK(db_.get());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OfflinePageMetadataStoreSQL::AddOrUpdateOfflinePageSync,
                 offline_page, db_.get(), base::ThreadTaskRunnerHandle::Get(),
                 callback));
}

void OfflinePageMetadataStoreSQL::RemoveOfflinePages(
    const std::vector<int64_t>& offline_ids,
    const UpdateCallback& callback) {
  DCHECK(db_.get());

  if (offline_ids.empty()) {
    // Nothing to do, but post a callback instead of calling directly
    // to preserve the async style behavior to prevent bugs.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, true));
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OfflinePageMetadataStoreSQL::RemoveOfflinePagesSync,
                 offline_ids, db_.get(), base::ThreadTaskRunnerHandle::Get(),
                 callback));
}

void OfflinePageMetadataStoreSQL::Reset(const ResetCallback& callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OfflinePageMetadataStoreSQL::ResetSync, base::Passed(&db_),
                 base::ThreadTaskRunnerHandle::Get(), callback));
}

}  // namespace offline_pages
