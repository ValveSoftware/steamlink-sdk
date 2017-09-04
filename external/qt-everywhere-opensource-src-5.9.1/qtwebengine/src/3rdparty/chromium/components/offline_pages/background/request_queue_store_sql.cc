// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue_store_sql.h"

#include <unordered_set>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/save_page_request.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

template class StoreUpdateResult<SavePageRequest>;

namespace {

using StoreStateCallback = base::Callback<void(StoreState)>;

// This is a macro instead of a const so that
// it can be used inline in other SQL statements below.
#define REQUEST_QUEUE_TABLE_NAME "request_queue_v1"
const bool kUserRequested = true;

bool CreateRequestQueueTable(sql::Connection* db) {
  const char kSql[] = "CREATE TABLE IF NOT EXISTS " REQUEST_QUEUE_TABLE_NAME
                      " (request_id INTEGER PRIMARY KEY NOT NULL,"
                      " creation_time INTEGER NOT NULL,"
                      " activation_time INTEGER NOT NULL DEFAULT 0,"
                      " last_attempt_time INTEGER NOT NULL DEFAULT 0,"
                      " started_attempt_count INTEGER NOT NULL,"
                      " completed_attempt_count INTEGER NOT NULL,"
                      " state INTEGER NOT NULL DEFAULT 0,"
                      " url VARCHAR NOT NULL,"
                      " client_namespace VARCHAR NOT NULL,"
                      " client_id VARCHAR NOT NULL"
                      ")";
  return db->Execute(kSql);
}

bool CreateSchema(sql::Connection* db) {
  // If there is not already a state column, we need to drop the old table.  We
  // are choosing to drop instead of upgrade since the feature is not yet
  // released, so we don't use a transaction to protect existing data, or try to
  // migrate it.
  if (!db->DoesColumnExist(REQUEST_QUEUE_TABLE_NAME, "state")) {
    if (!db->Execute("DROP TABLE IF EXISTS " REQUEST_QUEUE_TABLE_NAME))
      return false;
  }

  if (!CreateRequestQueueTable(db))
    return false;

  // TODO(fgorski): Add indices here.
  return true;
}

// Create a save page request from a SQL result.  Expects complete rows with
// all columns present.  Columns are in order they are defined in select query
// in |GetOneRequest| method.
std::unique_ptr<SavePageRequest> MakeSavePageRequest(
    const sql::Statement& statement) {
  const int64_t id = statement.ColumnInt64(0);
  const base::Time creation_time =
      base::Time::FromInternalValue(statement.ColumnInt64(1));
  const base::Time activation_time =
      base::Time::FromInternalValue(statement.ColumnInt64(2));
  const base::Time last_attempt_time =
      base::Time::FromInternalValue(statement.ColumnInt64(3));
  const int64_t started_attempt_count = statement.ColumnInt64(4);
  const int64_t completed_attempt_count = statement.ColumnInt64(5);
  const SavePageRequest::RequestState state =
      static_cast<SavePageRequest::RequestState>(statement.ColumnInt64(6));
  const GURL url(statement.ColumnString(7));
  const ClientId client_id(statement.ColumnString(8),
                           statement.ColumnString(9));

  DVLOG(2) << "making save page request - id " << id << " url " << url
           << " client_id " << client_id.name_space << "-" << client_id.id
           << " creation time " << creation_time << " user requested "
           << kUserRequested;

  std::unique_ptr<SavePageRequest> request(new SavePageRequest(
      id, url, client_id, creation_time, activation_time, kUserRequested));
  request->set_last_attempt_time(last_attempt_time);
  request->set_started_attempt_count(started_attempt_count);
  request->set_completed_attempt_count(completed_attempt_count);
  request->set_request_state(state);
  return request;
}

// Get a request for a specific id.
std::unique_ptr<SavePageRequest> GetOneRequest(sql::Connection* db,
                                               const int64_t request_id) {
  const char kSql[] =
      "SELECT request_id, creation_time, activation_time,"
      " last_attempt_time, started_attempt_count, completed_attempt_count,"
      " state, url, client_namespace, client_id"
      " FROM " REQUEST_QUEUE_TABLE_NAME " WHERE request_id=?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request_id);

  if (statement.Step())
    return MakeSavePageRequest(statement);
  return std::unique_ptr<SavePageRequest>(nullptr);
}

ItemActionStatus DeleteRequestById(sql::Connection* db, int64_t request_id) {
  const char kSql[] =
      "DELETE FROM " REQUEST_QUEUE_TABLE_NAME " WHERE request_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request_id);
  if (!statement.Run())
    return ItemActionStatus::STORE_ERROR;
  else if (db->GetLastChangeCount() == 0)
    return ItemActionStatus::NOT_FOUND;
  return ItemActionStatus::SUCCESS;
}

ItemActionStatus Insert(sql::Connection* db, const SavePageRequest& request) {
  const char kSql[] =
      "INSERT OR IGNORE INTO " REQUEST_QUEUE_TABLE_NAME
      " (request_id, creation_time, activation_time,"
      " last_attempt_time, started_attempt_count, completed_attempt_count,"
      " state, url, client_namespace, client_id)"
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request.request_id());
  statement.BindInt64(1, request.creation_time().ToInternalValue());
  statement.BindInt64(2, request.activation_time().ToInternalValue());
  statement.BindInt64(3, request.last_attempt_time().ToInternalValue());
  statement.BindInt64(4, request.started_attempt_count());
  statement.BindInt64(5, request.completed_attempt_count());
  statement.BindInt64(6, static_cast<int64_t>(request.request_state()));
  statement.BindString(7, request.url().spec());
  statement.BindString(8, request.client_id().name_space);
  statement.BindString(9, request.client_id().id);

  if (!statement.Run())
    return ItemActionStatus::STORE_ERROR;
  if (db->GetLastChangeCount() == 0)
    return ItemActionStatus::ALREADY_EXISTS;
  return ItemActionStatus::SUCCESS;
}

ItemActionStatus Update(sql::Connection* db, const SavePageRequest& request) {
  const char kSql[] =
      "UPDATE OR IGNORE " REQUEST_QUEUE_TABLE_NAME
      " SET creation_time = ?, activation_time = ?, last_attempt_time = ?,"
      " started_attempt_count = ?, completed_attempt_count = ?, state = ?,"
      " url = ?, client_namespace = ?, client_id = ?"
      " WHERE request_id = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, request.creation_time().ToInternalValue());
  statement.BindInt64(1, request.activation_time().ToInternalValue());
  statement.BindInt64(2, request.last_attempt_time().ToInternalValue());
  statement.BindInt64(3, request.started_attempt_count());
  statement.BindInt64(4, request.completed_attempt_count());
  statement.BindInt64(5, static_cast<int64_t>(request.request_state()));
  statement.BindString(6, request.url().spec());
  statement.BindString(7, request.client_id().name_space);
  statement.BindString(8, request.client_id().id);
  statement.BindInt64(9, request.request_id());

  if (!statement.Run())
    return ItemActionStatus::STORE_ERROR;
  if (db->GetLastChangeCount() == 0)
    return ItemActionStatus::NOT_FOUND;
  return ItemActionStatus::SUCCESS;
}

void PostStoreUpdateResultForIds(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    StoreState store_state,
    const std::vector<int64_t>& item_ids,
    ItemActionStatus action_status,
    const RequestQueueStore::UpdateCallback& callback) {
  std::unique_ptr<UpdateRequestsResult> result(
      new UpdateRequestsResult(store_state));
  for (const auto& item_id : item_ids)
    result->item_statuses.push_back(std::make_pair(item_id, action_status));
  runner->PostTask(FROM_HERE, base::Bind(callback, base::Passed(&result)));
}

void PostStoreErrorForAllRequests(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const std::vector<SavePageRequest>& items,
    const RequestQueueStore::UpdateCallback& callback) {
  std::vector<int64_t> item_ids;
  for (const auto& item : items)
    item_ids.push_back(item.request_id());
  PostStoreUpdateResultForIds(runner, StoreState::LOADED, item_ids,
                              ItemActionStatus::STORE_ERROR, callback);
}

void PostStoreErrorForAllIds(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const std::vector<int64_t>& item_ids,
    const RequestQueueStore::UpdateCallback& callback) {
  PostStoreUpdateResultForIds(runner, StoreState::LOADED, item_ids,
                              ItemActionStatus::STORE_ERROR, callback);
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

void GetRequestsSync(sql::Connection* db,
                     scoped_refptr<base::SingleThreadTaskRunner> runner,
                     const RequestQueueStore::GetRequestsCallback& callback) {
  const char kSql[] =
      "SELECT request_id, creation_time, activation_time,"
      " last_attempt_time, started_attempt_count, completed_attempt_count,"
      " state, url, client_namespace, client_id"
      " FROM " REQUEST_QUEUE_TABLE_NAME;

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));

  std::vector<std::unique_ptr<SavePageRequest>> requests;
  while (statement.Step())
    requests.push_back(MakeSavePageRequest(statement));

  runner->PostTask(FROM_HERE, base::Bind(callback, statement.Succeeded(),
                                         base::Passed(&requests)));
}

void GetRequestsByIdsSync(sql::Connection* db,
                          scoped_refptr<base::SingleThreadTaskRunner> runner,
                          const std::vector<int64_t>& request_ids,
                          const RequestQueueStore::UpdateCallback& callback) {
  // TODO(fgorski): Perhaps add metrics here.
  std::unique_ptr<UpdateRequestsResult> result(
      new UpdateRequestsResult(StoreState::LOADED));

  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    PostStoreErrorForAllIds(runner, request_ids, callback);
    return;
  }

  // Make sure not to include the same request multiple times, preserving the
  // order of non-duplicated IDs in the result.
  std::unordered_set<int64_t> processed_ids;
  for (int64_t request_id : request_ids) {
    if (!processed_ids.insert(request_id).second)
      continue;
    std::unique_ptr<SavePageRequest> request = GetOneRequest(db, request_id);
    if (request.get())
      result->updated_items.push_back(*request);
    ItemActionStatus status =
        request.get() ? ItemActionStatus::SUCCESS : ItemActionStatus::NOT_FOUND;
    result->item_statuses.push_back(std::make_pair(request_id, status));
  }

  if (!transaction.Commit()) {
    PostStoreErrorForAllIds(runner, request_ids, callback);
    return;
  }

  runner->PostTask(FROM_HERE, base::Bind(callback, base::Passed(&result)));
}

void AddRequestSync(sql::Connection* db,
                    scoped_refptr<base::SingleThreadTaskRunner> runner,
                    const SavePageRequest& request,
                    const RequestQueueStore::AddCallback& callback) {
  // TODO(fgorski): add UMA metrics here.
  ItemActionStatus status = Insert(db, request);
  runner->PostTask(FROM_HERE, base::Bind(callback, status));
}

void UpdateRequestsSync(sql::Connection* db,
                        scoped_refptr<base::SingleThreadTaskRunner> runner,
                        const std::vector<SavePageRequest>& requests,
                        const RequestQueueStore::UpdateCallback& callback) {
  // TODO(fgorski): add UMA metrics here.
  std::unique_ptr<UpdateRequestsResult> result(
      new UpdateRequestsResult(StoreState::LOADED));

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    PostStoreErrorForAllRequests(runner, requests, callback);
    return;
  }

  for (const auto& request : requests) {
    ItemActionStatus status = Update(db, request);
    result->item_statuses.push_back(
        std::make_pair(request.request_id(), status));
    if (status == ItemActionStatus::SUCCESS)
      result->updated_items.push_back(request);
  }

  if (!transaction.Commit()) {
    PostStoreErrorForAllRequests(runner, requests, callback);
    return;
  }

  runner->PostTask(FROM_HERE, base::Bind(callback, base::Passed(&result)));
}

void RemoveRequestsSync(sql::Connection* db,
                        scoped_refptr<base::SingleThreadTaskRunner> runner,
                        const std::vector<int64_t>& request_ids,
                        const RequestQueueStore::UpdateCallback& callback) {
  // TODO(fgorski): Perhaps add metrics here.
  std::unique_ptr<UpdateRequestsResult> result(
      new UpdateRequestsResult(StoreState::LOADED));

  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    PostStoreErrorForAllIds(runner, request_ids, callback);
    return;
  }

  // Read the request before we delete it, and if the delete worked, put it on
  // the queue of requests that got deleted.
  for (int64_t request_id : request_ids) {
    std::unique_ptr<SavePageRequest> request = GetOneRequest(db, request_id);
    ItemActionStatus status = DeleteRequestById(db, request_id);
    result->item_statuses.push_back(std::make_pair(request_id, status));
    if (status == ItemActionStatus::SUCCESS)
      result->updated_items.push_back(*request);
  }

  if (!transaction.Commit()) {
    PostStoreErrorForAllIds(runner, request_ids, callback);
    return;
  }

  runner->PostTask(FROM_HERE, base::Bind(callback, base::Passed(&result)));
}

void OpenConnectionSync(sql::Connection* db,
                        scoped_refptr<base::SingleThreadTaskRunner> runner,
                        const base::FilePath& path,
                        const StoreStateCallback& callback) {
  StoreState state =
      InitDatabase(db, path) ? StoreState::LOADED : StoreState::FAILED_LOADING;
  runner->PostTask(FROM_HERE, base::Bind(callback, state));
}

void ResetSync(sql::Connection* db,
               const base::FilePath& db_file_path,
               scoped_refptr<base::SingleThreadTaskRunner> runner,
               const StoreStateCallback& callback) {
  // This method deletes the content of the whole store and reinitializes it.
  bool success = db->Raze();
  db->Close();
  StoreState state;
  if (!success)
    state = StoreState::FAILED_RESET;
  if (InitDatabase(db, db_file_path))
    state = StoreState::LOADED;
  else
    state = StoreState::FAILED_LOADING;

  runner->PostTask(FROM_HERE, base::Bind(callback, state));
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

bool RequestQueueStoreSQL::CheckDb(const base::Closure& callback) {
  DCHECK(db_.get());
  if (!db_.get()) {
    // Nothing to do, but post a callback instead of calling directly
    // to preserve the async style behavior to prevent bugs.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
    return false;
  }
  return true;
}

void RequestQueueStoreSQL::GetRequests(const GetRequestsCallback& callback) {
  DCHECK(db_.get());
  std::vector<std::unique_ptr<SavePageRequest>> requests;
  if (!CheckDb(base::Bind(callback, false, base::Passed(&requests))))
    return;

  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GetRequestsSync, db_.get(),
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

void RequestQueueStoreSQL::GetRequestsByIds(
    const std::vector<int64_t>& request_ids,
    const UpdateCallback& callback) {
  if (!db_.get()) {
    PostStoreErrorForAllIds(base::ThreadTaskRunnerHandle::Get(), request_ids,
                            callback);
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GetRequestsByIdsSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), request_ids, callback));
}

void RequestQueueStoreSQL::AddRequest(const SavePageRequest& request,
                                      const AddCallback& callback) {
  if (!CheckDb(base::Bind(callback, ItemActionStatus::STORE_ERROR)))
    return;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AddRequestSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), request, callback));
}

void RequestQueueStoreSQL::UpdateRequests(
    const std::vector<SavePageRequest>& requests,
    const UpdateCallback& callback) {
  if (!db_.get()) {
    PostStoreErrorForAllRequests(base::ThreadTaskRunnerHandle::Get(), requests,
                                 callback);
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UpdateRequestsSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), requests, callback));
}

void RequestQueueStoreSQL::RemoveRequests(
    const std::vector<int64_t>& request_ids,
    const UpdateCallback& callback) {
  if (!db_.get()) {
    PostStoreErrorForAllIds(base::ThreadTaskRunnerHandle::Get(), request_ids,
                            callback);
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RemoveRequestsSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), request_ids, callback));
}

void RequestQueueStoreSQL::Reset(const ResetCallback& callback) {
  DCHECK(db_.get());
  if (!CheckDb(base::Bind(callback, false)))
    return;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ResetSync, db_.get(), db_file_path_,
                 base::ThreadTaskRunnerHandle::Get(),
                 base::Bind(&RequestQueueStoreSQL::OnResetDone,
                            weak_ptr_factory_.GetWeakPtr(), callback)));
}

void RequestQueueStoreSQL::OpenConnection() {
  DCHECK(!db_);
  db_.reset(new sql::Connection());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OpenConnectionSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), db_file_path_,
                 base::Bind(&RequestQueueStoreSQL::OnOpenConnectionDone,
                            weak_ptr_factory_.GetWeakPtr())));
}

void RequestQueueStoreSQL::OnOpenConnectionDone(StoreState state) {
  DCHECK(db_.get());

  state_ = state;

  // Unfortunately we were not able to open DB connection.
  if (state_ != StoreState::LOADED)
    db_.reset();
}

void RequestQueueStoreSQL::OnResetDone(const ResetCallback& callback,
                                       StoreState state) {
  // Complete connection initialization post reset.
  OnOpenConnectionDone(state);
  callback.Run(state == StoreState::LOADED);
}

StoreState RequestQueueStoreSQL::state() const {
  return state_;
}

}  // namespace offline_pages
