// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database_service.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/webdata/common/web_data_request_manager.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_backend.h"

using base::Bind;
using base::FilePath;

// Receives messages from the backend on the DB thread, posts them to
// WebDatabaseService on the UI thread.
class WebDatabaseService::BackendDelegate
    : public WebDatabaseBackend::Delegate {
 public:
  BackendDelegate(const base::WeakPtr<WebDatabaseService>& web_database_service)
      : web_database_service_(web_database_service),
        callback_thread_(base::ThreadTaskRunnerHandle::Get()) {}

  void DBLoaded(sql::InitStatus status,
                const std::string& diagnostics) override {
    callback_thread_->PostTask(
        FROM_HERE, base::Bind(&WebDatabaseService::OnDatabaseLoadDone,
                              web_database_service_, status, diagnostics));
  }
 private:
  const base::WeakPtr<WebDatabaseService> web_database_service_;
  scoped_refptr<base::SingleThreadTaskRunner> callback_thread_;
};

WebDatabaseService::WebDatabaseService(
    const base::FilePath& path,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread)
    : base::RefCountedDeleteOnMessageLoop<WebDatabaseService>(ui_thread),
      path_(path),
      db_loaded_(false),
      db_thread_(db_thread),
      weak_ptr_factory_(this) {
  // WebDatabaseService should be instantiated on UI thread.
  DCHECK(ui_thread->BelongsToCurrentThread());
  // WebDatabaseService requires DB thread if instantiated.
  DCHECK(db_thread.get());
}

WebDatabaseService::~WebDatabaseService() {
}

void WebDatabaseService::AddTable(std::unique_ptr<WebDatabaseTable> table) {
  if (!web_db_backend_.get()) {
    web_db_backend_ = new WebDatabaseBackend(
        path_, new BackendDelegate(weak_ptr_factory_.GetWeakPtr()), db_thread_);
  }
  web_db_backend_->AddTable(std::move(table));
}

void WebDatabaseService::LoadDatabase() {
  DCHECK(web_db_backend_.get());
  db_thread_->PostTask(
      FROM_HERE, Bind(&WebDatabaseBackend::InitDatabase, web_db_backend_));
}

void WebDatabaseService::ShutdownDatabase() {
  db_loaded_ = false;
  loaded_callbacks_.clear();
  error_callbacks_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!web_db_backend_.get())
    return;
  db_thread_->PostTask(
      FROM_HERE, Bind(&WebDatabaseBackend::ShutdownDatabase, web_db_backend_));
}

WebDatabase* WebDatabaseService::GetDatabaseOnDB() const {
  DCHECK(db_thread_->BelongsToCurrentThread());
  return web_db_backend_.get() ? web_db_backend_->database() : NULL;
}

scoped_refptr<WebDatabaseBackend> WebDatabaseService::GetBackend() const {
  return web_db_backend_;
}

void WebDatabaseService::ScheduleDBTask(
    const tracked_objects::Location& from_here,
    const WriteTask& task) {
  DCHECK(web_db_backend_.get());
  std::unique_ptr<WebDataRequest> request(
      new WebDataRequest(NULL, web_db_backend_->request_manager().get()));
  db_thread_->PostTask(
      from_here, Bind(&WebDatabaseBackend::DBWriteTaskWrapper, web_db_backend_,
                      task, base::Passed(&request)));
}

WebDataServiceBase::Handle WebDatabaseService::ScheduleDBTaskWithResult(
    const tracked_objects::Location& from_here,
    const ReadTask& task,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  DCHECK(web_db_backend_.get());
  std::unique_ptr<WebDataRequest> request(
      new WebDataRequest(consumer, web_db_backend_->request_manager().get()));
  WebDataServiceBase::Handle handle = request->GetHandle();
  db_thread_->PostTask(
      from_here, Bind(&WebDatabaseBackend::DBReadTaskWrapper, web_db_backend_,
                      task, base::Passed(&request)));
  return handle;
}

void WebDatabaseService::CancelRequest(WebDataServiceBase::Handle h) {
  if (!web_db_backend_.get())
    return;
  web_db_backend_->request_manager()->CancelRequest(h);
}

void WebDatabaseService::RegisterDBLoadedCallback(
    const DBLoadedCallback& callback) {
  loaded_callbacks_.push_back(callback);
}

void WebDatabaseService::RegisterDBErrorCallback(
    const DBLoadErrorCallback& callback) {
  error_callbacks_.push_back(callback);
}

void WebDatabaseService::OnDatabaseLoadDone(sql::InitStatus status,
                                            const std::string& diagnostics) {
  // The INIT_OK_WITH_DATA_LOSS status is an initialization success but with
  // suspected data loss, so we also run the error callbacks.
  if (status != sql::INIT_OK) {
    // Notify that the database load failed.
    while (!error_callbacks_.empty()) {
      // The profile error callback is a message box that runs in a nested run
      // loop. While it's being displayed, other OnDatabaseLoadDone() will run
      // (posted from WebDatabaseBackend::Delegate::DBLoaded()). We need to make
      // sure that after the callback running the message box returns, it checks
      // |error_callbacks_| before it accesses it.
      DBLoadErrorCallback error_callback = error_callbacks_.back();
      error_callbacks_.pop_back();
      if (!error_callback.is_null())
        error_callback.Run(status, diagnostics);
    }
  }

  if (status == sql::INIT_OK || status == sql::INIT_OK_WITH_DATA_LOSS) {
    db_loaded_ = true;

    while (!loaded_callbacks_.empty()) {
      DBLoadedCallback loaded_callback = loaded_callbacks_.back();
      loaded_callbacks_.pop_back();
      if (!loaded_callback.is_null())
        loaded_callback.Run();
    }
  }
}
