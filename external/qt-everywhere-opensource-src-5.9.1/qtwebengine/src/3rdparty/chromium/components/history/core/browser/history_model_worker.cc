// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_model_worker.h"

#include <utility>

#include "base/synchronization/waitable_event.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/base/scoped_event_signal.h"

namespace browser_sync {

class WorkerTask : public history::HistoryDBTask {
 public:
  WorkerTask(const syncer::WorkCallback& work,
             syncer::ScopedEventSignal scoped_event_signal,
             syncer::SyncerError* error)
      : work_(work),
        scoped_event_signal_(std::move(scoped_event_signal)),
        error_(error) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    // Signal the completion event at the end of this scope.
    auto scoped_event_signal = std::move(scoped_event_signal_);

    // Run the task.
    *error_ = work_.Run();

    return true;
  }

  // Since the DoWorkAndWaitUntilDone() is synchronous, we don't need to run
  // any code asynchronously on the main thread after completion.
  void DoneRunOnMainThread() override {}

 protected:
  ~WorkerTask() override {
    // The event in |scoped_event_signal_| is signaled at the end of this
    // scope if this is destroyed before RunOnDBThread runs.
  }

  syncer::WorkCallback work_;
  syncer::ScopedEventSignal scoped_event_signal_;
  syncer::SyncerError* error_;
};

class AddDBThreadObserverTask : public history::HistoryDBTask {
 public:
  explicit AddDBThreadObserverTask(base::Closure register_callback)
     : register_callback_(register_callback) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    register_callback_.Run();
    return true;
  }

  void DoneRunOnMainThread() override {}

 private:
  ~AddDBThreadObserverTask() override {}

  base::Closure register_callback_;
};

namespace {

// Post the work task on |history_service|'s DB thread from the UI
// thread.
void PostWorkerTask(
    const base::WeakPtr<history::HistoryService>& history_service,
    const syncer::WorkCallback& work,
    syncer::ScopedEventSignal scoped_event_signal,
    base::CancelableTaskTracker* cancelable_tracker,
    syncer::SyncerError* error) {
  if (history_service.get()) {
    std::unique_ptr<history::HistoryDBTask> task(
        new WorkerTask(work, std::move(scoped_event_signal), error));
    history_service->ScheduleDBTask(std::move(task), cancelable_tracker);
  } else {
    *error = syncer::CANNOT_DO_WORK;
    // The event in |scoped_event_signal| is signaled at the end of this
    // scope.
  }
}

}  // namespace

HistoryModelWorker::HistoryModelWorker(
    const base::WeakPtr<history::HistoryService>& history_service,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread)
    : history_service_(history_service), ui_thread_(ui_thread) {
  CHECK(history_service.get());
  DCHECK(ui_thread_->BelongsToCurrentThread());
  cancelable_tracker_.reset(new base::CancelableTaskTracker);
}

syncer::SyncerError HistoryModelWorker::DoWorkAndWaitUntilDoneImpl(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error = syncer::UNSET;

  // Signaled after the task runs or when it is abandoned.
  base::WaitableEvent work_done_or_abandoned(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  if (ui_thread_->PostTask(FROM_HERE,
                           base::Bind(&PostWorkerTask, history_service_, work,
                                      base::Passed(syncer::ScopedEventSignal(
                                          &work_done_or_abandoned)),
                                      cancelable_tracker_.get(), &error))) {
    work_done_or_abandoned.Wait();
  } else {
    error = syncer::CANNOT_DO_WORK;
  }
  return error;
}

syncer::ModelSafeGroup HistoryModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_HISTORY;
}

bool HistoryModelWorker::IsOnModelThread() {
  // Ideally HistoryService would expose a way to check whether this is the
  // history DB thread. Since it doesn't, just return true to bypass a CHECK in
  // the sync code.
  return true;
}

HistoryModelWorker::~HistoryModelWorker() {
  // The base::CancelableTaskTracker class is not thread-safe and must only be
  // used from a single thread but the current object may not be destroyed from
  // the UI thread, so delete it from the UI thread.
  ui_thread_->DeleteSoon(FROM_HERE, cancelable_tracker_.release());
}

}  // namespace browser_sync
