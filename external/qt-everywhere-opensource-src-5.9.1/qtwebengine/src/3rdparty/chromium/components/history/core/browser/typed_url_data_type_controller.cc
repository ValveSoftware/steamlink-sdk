// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_data_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_client.h"

namespace browser_sync {

namespace {

// The history service exposes a special non-standard task API which calls back
// once a task has been dispatched, so we have to build a special wrapper around
// the tasks we want to run.
class RunTaskOnHistoryThread : public history::HistoryDBTask {
 public:
  explicit RunTaskOnHistoryThread(const base::Closure& task)
      : task_(new base::Closure(task)) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
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
};

}  // namespace

TypedUrlDataTypeController::TypedUrlDataTypeController(
    const base::Closure& dump_stack,
    syncer::SyncClient* sync_client,
    const char* history_disabled_pref_name)
    : NonUIDataTypeController(syncer::TYPED_URLS, dump_stack, sync_client),
      history_disabled_pref_name_(history_disabled_pref_name),
      sync_client_(sync_client) {
  pref_registrar_.Init(sync_client->GetPrefService());
  pref_registrar_.Add(
      history_disabled_pref_name_,
      base::Bind(
          &TypedUrlDataTypeController::OnSavingBrowserHistoryDisabledChanged,
          base::AsWeakPtr(this)));
}

syncer::ModelSafeGroup TypedUrlDataTypeController::model_safe_group() const {
  return syncer::GROUP_HISTORY;
}

bool TypedUrlDataTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return !sync_client_->GetPrefService()->GetBoolean(
      history_disabled_pref_name_);
}

void TypedUrlDataTypeController::OnSavingBrowserHistoryDisabledChanged() {
  DCHECK(CalledOnValidThread());
  if (sync_client_->GetPrefService()->GetBoolean(history_disabled_pref_name_)) {
    // We've turned off history persistence, so if we are running,
    // generate an unrecoverable error. This can be fixed by restarting
    // Chrome (on restart, typed urls will not be a registered type).
    if (state() != NOT_RUNNING && state() != STOPPING) {
      PostTaskOnBackendThread(
          FROM_HERE,
          base::Bind(&syncer::DataTypeErrorHandler::OnUnrecoverableError,
                     base::Passed(CreateErrorHandler()),
                     syncer::SyncError(
                         FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
                         "History saving is now disabled by policy.", type())));
    }
  }
}

bool TypedUrlDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(CalledOnValidThread());
  history::HistoryService* history = sync_client_->GetHistoryService();
  if (history) {
    history->ScheduleDBTask(std::unique_ptr<history::HistoryDBTask>(
                                new RunTaskOnHistoryThread(task)),
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
