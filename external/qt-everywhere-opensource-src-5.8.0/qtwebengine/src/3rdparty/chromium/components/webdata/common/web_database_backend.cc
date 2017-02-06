// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database_backend.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "components/webdata/common/web_data_request_manager.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_table.h"

using base::Bind;
using base::FilePath;

WebDatabaseBackend::WebDatabaseBackend(
    const FilePath& path,
    Delegate* delegate,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread)
    : base::RefCountedDeleteOnMessageLoop<WebDatabaseBackend>(db_thread),
      db_path_(path),
      request_manager_(new WebDataRequestManager()),
      init_status_(sql::INIT_FAILURE),
      init_complete_(false),
      delegate_(delegate) {
}

void WebDatabaseBackend::AddTable(std::unique_ptr<WebDatabaseTable> table) {
  DCHECK(!db_.get());
  tables_.push_back(table.release());
}

void WebDatabaseBackend::InitDatabase() {
  LoadDatabaseIfNecessary();
  if (delegate_) {
    delegate_->DBLoaded(init_status_);
  }
}

sql::InitStatus WebDatabaseBackend::LoadDatabaseIfNecessary() {
  if (init_complete_ || db_path_.empty()) {
    return init_status_;
  }
  init_complete_ = true;
  db_.reset(new WebDatabase());

  for (ScopedVector<WebDatabaseTable>::iterator it = tables_.begin();
       it != tables_.end(); ++it) {
    db_->AddTable(*it);
  }

  init_status_ = db_->Init(db_path_);
  if (init_status_ != sql::INIT_OK) {
    LOG(ERROR) << "Cannot initialize the web database: " << init_status_;
    db_.reset(NULL);
    return init_status_;
  }

  db_->BeginTransaction();
  return init_status_;
}

void WebDatabaseBackend::ShutdownDatabase() {
  if (db_ && init_status_ == sql::INIT_OK)
    db_->CommitTransaction();
  db_.reset(NULL);
  init_complete_ = true;  // Ensures the init sequence is not re-run.
  init_status_ = sql::INIT_FAILURE;
}

void WebDatabaseBackend::DBWriteTaskWrapper(
    const WebDatabaseService::WriteTask& task,
    std::unique_ptr<WebDataRequest> request) {
  if (request->IsCancelled())
    return;

  ExecuteWriteTask(task);
  request_manager_->RequestCompleted(std::move(request));
}

void WebDatabaseBackend::ExecuteWriteTask(
    const WebDatabaseService::WriteTask& task) {
  LoadDatabaseIfNecessary();
  if (db_ && init_status_ == sql::INIT_OK) {
    WebDatabase::State state = task.Run(db_.get());
    if (state == WebDatabase::COMMIT_NEEDED)
      Commit();
  }
}

void WebDatabaseBackend::DBReadTaskWrapper(
    const WebDatabaseService::ReadTask& task,
    std::unique_ptr<WebDataRequest> request) {
  if (request->IsCancelled())
    return;

  request->SetResult(ExecuteReadTask(task));
  request_manager_->RequestCompleted(std::move(request));
}

std::unique_ptr<WDTypedResult> WebDatabaseBackend::ExecuteReadTask(
    const WebDatabaseService::ReadTask& task) {
  LoadDatabaseIfNecessary();
  if (db_ && init_status_ == sql::INIT_OK) {
    return task.Run(db_.get());
  }
  return nullptr;
}

WebDatabaseBackend::~WebDatabaseBackend() {
  ShutdownDatabase();
}

void WebDatabaseBackend::Commit() {
  DCHECK(db_);
  DCHECK_EQ(sql::INIT_OK, init_status_);
  db_->CommitTransaction();
  db_->BeginTransaction();
}
