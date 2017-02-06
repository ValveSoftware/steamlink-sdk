// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/sync_client.h"

namespace browser_sync {

namespace {

// The history service exposes a special non-standard task API which calls back
// once a task has been dispatched, so we have to build a special wrapper around
// the tasks we want to run.
class RunTaskOnHistoryThread : public history::HistoryDBTask {
 public:
  explicit RunTaskOnHistoryThread(const base::Closure& task,
                                  TypedUrlDataTypeController* dtc)
      : task_(new base::Closure(task)), dtc_(dtc) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    // Set the backend, then release our reference before executing the task.
    dtc_->SetBackend(backend);
    dtc_ = NULL;

    // Invoke the task, then free it immediately so we don't keep a reference
    // around all the way until DoneRunOnMainThread() is invoked back on the
    // main thread - we want to release references as soon as possible to avoid
    // keeping them around too long during shutdown.
    task_->Run();
    task_.reset();
    return true;
  }

  void DoneRunOnMainThread() override {}

 protected:
  ~RunTaskOnHistoryThread() override {}

  std::unique_ptr<base::Closure> task_;
  scoped_refptr<TypedUrlDataTypeController> dtc_;
};

}  // namespace

TypedUrlDataTypeController::TypedUrlDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    const char* history_disabled_pref_name)
    : NonUIDataTypeController(ui_thread, error_callback, sync_client),
      history_disabled_pref_name_(history_disabled_pref_name),
      backend_(NULL),
      sync_client_(sync_client) {
  pref_registrar_.Init(sync_client->GetPrefService());
  pref_registrar_.Add(
      history_disabled_pref_name_,
      base::Bind(
          &TypedUrlDataTypeController::OnSavingBrowserHistoryDisabledChanged,
          base::Unretained(this)));
}

syncer::ModelType TypedUrlDataTypeController::type() const {
  return syncer::TYPED_URLS;
}

syncer::ModelSafeGroup TypedUrlDataTypeController::model_safe_group() const {
  return syncer::GROUP_HISTORY;
}

bool TypedUrlDataTypeController::ReadyForStart() const {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  return !sync_client_->GetPrefService()->GetBoolean(
      history_disabled_pref_name_);
}

void TypedUrlDataTypeController::SetBackend(history::HistoryBackend* backend) {
  DCHECK(!ui_thread()->BelongsToCurrentThread());
  backend_ = backend;
}

void TypedUrlDataTypeController::OnSavingBrowserHistoryDisabledChanged() {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  if (sync_client_->GetPrefService()->GetBoolean(history_disabled_pref_name_)) {
    // We've turned off history persistence, so if we are running,
    // generate an unrecoverable error. This can be fixed by restarting
    // Chrome (on restart, typed urls will not be a registered type).
    if (state() != NOT_RUNNING && state() != STOPPING) {
      PostTaskOnBackendThread(
          FROM_HERE,
          base::Bind(&DataTypeController::OnSingleDataTypeUnrecoverableError,
                     this,
                     syncer::SyncError(
                         FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
                         "History saving is now disabled by policy.", type())));
    }
  }
}

bool TypedUrlDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(ui_thread()->BelongsToCurrentThread());
  history::HistoryService* history = sync_client_->GetHistoryService();
  if (history) {
    history->ScheduleDBTask(std::unique_ptr<history::HistoryDBTask>(
                                new RunTaskOnHistoryThread(task, this)),
                            &task_tracker_);
    return true;
  } else {
    // History must be disabled - don't start.
    LOG(WARNING) << "Cannot access history service - disabling typed url sync";
    return false;
  }
}

TypedUrlDataTypeController::~TypedUrlDataTypeController() {}

}  // namespace browser_sync
