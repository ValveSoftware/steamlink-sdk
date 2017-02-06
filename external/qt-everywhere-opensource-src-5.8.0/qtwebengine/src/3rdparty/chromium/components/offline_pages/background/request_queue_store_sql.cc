// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue_store_sql.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/save_page_request.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

// This is a macro instead of a const so that
// it can be used inline in other SQL statements below.
#define REQUEST_QUEUE_TABLE_NAME "request_queue_v1"

bool CreateRequestQueueTable(sql::Connection* db) {
  const char kSql[] = "CREATE TABLE IF NOT EXISTS " REQUEST_QUEUE_TABLE_NAME
                      " (request_id INTEGER PRIMARY KEY NOT NULL,"
                      " creation_time INTEGER NOT NULL,"
                      " activation_time INTEGER NOT NULL DEFAULT 0,"
                      " last_attempt_time INTEGER NOT NULL DEFAULT 0,"
                      " attempt_count INTEGER NOT NULL,"
                      " url VARCHAR NOT NULL,"
                      " client_namespace VARCHAR NOT NULL,"
                      " client_id VARCHAR NOT NULL"
                      ")";
  return db->Execute(kSql);
}

bool CreateSchema(sql::Connection* db) {
  // TODO(fgorski): Upgrade code goes here and requires transaction.
  if (!CreateRequestQueueTable(db))
    return false;

  // TODO(fgorski): Add indices here.
  return true;
}

bool DeleteRequestById(sql::Connection* db, int64_t request_id) {
  const char kSql[] =
      "DELETE FROM " REQUEST_QUEUE_TABLE_NAME " WHERE request_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request_id);
  return statement.Run();
}

bool DeleteRequestsByIds(sql::Connection* db,
                         const std::vector<int64_t>& request_ids,
                         int* count) {
  DCHECK(count);
  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  *count = 0;
  for (auto request_id : request_ids) {
    if (!DeleteRequestById(db, request_id))
      return false;
    *count += db->GetLastChangeCount();
  }

  if (!transaction.Commit())
    return false;

  return true;
}

// Create a save page request from a SQL result.  Expects complete rows with
// all columns present.  Columns are in order they are defined in select query
// in |RequestQueueStore::RequestSync| method.
SavePageRequest MakeSavePageRequest(const sql::Statement& statement) {
  const int64_t id = statement.ColumnInt64(0);
  const base::Time creation_time =
      base::Time::FromInternalValue(statement.ColumnInt64(1));
  const base::Time activation_time =
      base::Time::FromInternalValue(statement.ColumnInt64(2));
  const base::Time last_attempt_time =
      base::Time::FromInternalValue(statement.ColumnInt64(3));
  const int64_t last_attempt_count = statement.ColumnInt64(4);
  const GURL url(statement.ColumnString(5));
  const ClientId client_id(statement.ColumnString(6),
                           statement.ColumnString(7));

  SavePageRequest request(id, url, client_id, creation_time, activation_time);
  request.set_last_attempt_time(last_attempt_time);
  request.set_attempt_count(last_attempt_count);
  return request;
}

RequestQueueStore::UpdateStatus InsertOrReplace(
    sql::Connection* db,
    const SavePageRequest& request) {
  // In order to use the enums in the Bind* methods, keep the order of fields
  // the same as in the definition/select query.
  const char kInsertSql[] =
      "INSERT OR REPLACE INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time, last_attempt_time, "
      " attempt_count, url, client_namespace, client_id) "
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindInt64(0, request.request_id());
  statement.BindInt64(1, request.creation_time().ToInternalValue());
  statement.BindInt64(2, request.activation_time().ToInternalValue());
  statement.BindInt64(3, request.last_attempt_time().ToInternalValue());
  statement.BindInt64(4, request.attempt_count());
  statement.BindString(5, request.url().spec());
  statement.BindString(6, request.client_id().name_space);
  statement.BindString(7, request.client_id().id);

  // TODO(fgorski): Replace the UpdateStatus with boolean in the
  // RequestQueueStore interface and update this code.
  return statement.Run() ? RequestQueueStore::UpdateStatus::UPDATED
                         : RequestQueueStore::UpdateStatus::FAILED;
}

bool InitDatabase(sql::Connection* db, const base::FilePath& path) {
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("BackgroundRequestQueue");
  db->set_exclusive_locking();

  base::File::Error err;
  if (!base::CreateDirectoryAndGetError(path.DirName(), &err))
    return false;
  if (!db->Open(path))
    return false;
  db->Preload();

  return CreateSchema(db);
}

}  // anonymous namespace

RequestQueueStoreSQL::RequestQueueStoreSQL(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : background_task_runner_(std::move(background_task_runner)),
      db_file_path_(path.AppendASCII("RequestQueue.db")),
      weak_ptr_factory_(this) {
  OpenConnection();
}

RequestQueueStoreSQL::~RequestQueueStoreSQL() {
  if (db_.get())
    background_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

// static
void RequestQueueStoreSQL::OpenConnectionSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const base::FilePath& path,
    const base::Callback<void(bool)>& callback) {
  bool success = InitDatabase(db, path);
  runner->PostTask(FROM_HERE, base::Bind(callback, success));
}

// static
void RequestQueueStoreSQL::GetRequestsSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const GetRequestsCallback& callback) {
  const char kSql[] =
      "SELECT request_id, creation_time, activation_time,"
      " last_attempt_time, attempt_count, url, client_namespace, client_id"
      " FROM " REQUEST_QUEUE_TABLE_NAME;

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));

  std::vector<SavePageRequest> result;
  while (statement.Step())
    result.push_back(MakeSavePageRequest(statement));

  runner->PostTask(FROM_HERE,
                   base::Bind(callback, statement.Succeeded(), result));
}

// static
void RequestQueueStoreSQL::AddOrUpdateRequestSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const SavePageRequest& request,
    const UpdateCallback& callback) {
  // TODO(fgorski): add UMA metrics here.
  RequestQueueStore::UpdateStatus status = InsertOrReplace(db, request);
  runner->PostTask(FROM_HERE, base::Bind(callback, status));
}

// static
void RequestQueueStoreSQL::RemoveRequestsSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const std::vector<int64_t>& request_ids,
    const RemoveCallback& callback) {
  // TODO(fgorski): add UMA metrics here.
  int count = 0;
  if (DeleteRequestsByIds(db, request_ids, &count))
    runner->PostTask(FROM_HERE, base::Bind(callback, true, count));
  else
    runner->PostTask(FROM_HERE, base::Bind(callback, false, 0));
}

// static
void RequestQueueStoreSQL::ResetSync(
    sql::Connection* db,
    const base::FilePath& db_file_path,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const ResetCallback& callback) {
  // This method deletes the content of the whole store and reinitializes it.
  bool success = db->Raze();
  db->Close();
  if (success)
    success = InitDatabase(db, db_file_path);
  runner->PostTask(FROM_HERE, base::Bind(callback, success));
}

void RequestQueueStoreSQL::GetRequests(const GetRequestsCallback& callback) {
  DCHECK(db_.get());
  if (!db_.get()) {
    // Nothing to do, but post a callback instead of calling directly
    // to preserve the async style behavior to prevent bugs.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, false, std::vector<SavePageRequest>()));
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RequestQueueStoreSQL::GetRequestsSync, db_.get(),
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void RequestQueueStoreSQL::AddOrUpdateRequest(const SavePageRequest& request,
                                              const UpdateCallback& callback) {
  DCHECK(db_.get());
  if (!db_.get()) {
    // Nothing to do, but post a callback instead of calling directly
    // to preserve the async style behavior to prevent bugs.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, UpdateStatus::FAILED));
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestQueueStoreSQL::AddOrUpdateRequestSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), request, callback));
}

void RequestQueueStoreSQL::RemoveRequests(
    const std::vector<int64_t>& request_ids,
    const RemoveCallback& callback) {
  DCHECK(db_.get());
  if (!db_.get()) {
    // Nothing to do, but post a callback instead of calling directly
    // to preserve the async style behavior to prevent bugs.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, false, 0));
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestQueueStoreSQL::RemoveRequestsSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), request_ids, callback));
}

void RequestQueueStoreSQL::Reset(const ResetCallback& callback) {
  DCHECK(db_.get());
  if (!db_.get()) {
    // Nothing to do, but post a callback instead of calling directly
    // to preserve the async style behavior to prevent bugs.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestQueueStoreSQL::ResetSync, db_.get(), db_file_path_,
                 base::ThreadTaskRunnerHandle::Get(),
                 base::Bind(&RequestQueueStoreSQL::OnResetDone,
                            weak_ptr_factory_.GetWeakPtr(), callback)));
}

void RequestQueueStoreSQL::OpenConnection() {
  DCHECK(!db_);
  db_.reset(new sql::Connection());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RequestQueueStoreSQL::OpenConnectionSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), db_file_path_,
                 base::Bind(&RequestQueueStoreSQL::OnOpenConnectionDone,
                            weak_ptr_factory_.GetWeakPtr())));
}

void RequestQueueStoreSQL::OnOpenConnectionDone(bool success) {
  DCHECK(db_.get());

  // Unfortunately we were not able to open DB connection.
  if (!success)
    db_.reset();
}

void RequestQueueStoreSQL::OnResetDone(const ResetCallback& callback,
                                       bool success) {
  // Complete connection initialization post reset.
  OnOpenConnectionDone(success);
  callback.Run(success);
}

}  // namespace offline_pages
