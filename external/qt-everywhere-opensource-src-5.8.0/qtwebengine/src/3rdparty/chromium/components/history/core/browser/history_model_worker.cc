// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_model_worker.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"

using base::WaitableEvent;

namespace browser_sync {

class WorkerTask : public history::HistoryDBTask {
 public:
  WorkerTask(
      const syncer::WorkCallback& work,
      WaitableEvent* done,
      syncer::SyncerError* error)
    : work_(work), done_(done), error_(error) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    *error_ = work_.Run();
    done_->Signal();
    return true;
  }

  // Since the DoWorkAndWaitUntilDone() is synchronous, we don't need to run
  // any code asynchronously on the main thread after completion.
  void DoneRunOnMainThread() override {}

 protected:
  ~WorkerTask() override {}

  syncer::WorkCallback work_;
  WaitableEvent* done_;
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
    base::CancelableTaskTracker* cancelable_tracker,
    WaitableEvent* done,
    syncer::SyncerError* error) {
  if (history_service.get()) {
    std::unique_ptr<history::HistoryDBTask> task(
        new WorkerTask(work, done, error));
    history_service->ScheduleDBTask(std::move(task), cancelable_tracker);
  } else {
    *error = syncer::CANNOT_DO_WORK;
    done->Signal();
  }
}

}  // namespace

HistoryModelWorker::HistoryModelWorker(
    const base::WeakPtr<history::HistoryService>& history_service,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    syncer::WorkerLoopDestructionObserver* observer)
    : syncer::ModelSafeWorker(observer),
      history_service_(history_service),
      ui_thread_(ui_thread) {
  CHECK(history_service.get());
  DCHECK(ui_thread_->BelongsToCurrentThread());
  cancelable_tracker_.reset(new base::CancelableTaskTracker);
}

void HistoryModelWorker::RegisterForLoopDestruction() {
  CHECK(history_service_.get());
  history_service_->ScheduleDBTask(
      std::unique_ptr<history::HistoryDBTask>(new AddDBThreadObserverTask(
          base::Bind(&HistoryModelWorker::RegisterOnDBThread, this))),
      cancelable_tracker_.get());
}

void HistoryModelWorker::RegisterOnDBThread() {
  SetWorkingLoopToCurrent();
}

syncer::SyncerError HistoryModelWorker::DoWorkAndWaitUntilDoneImpl(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error = syncer::UNSET;
  if (ui_thread_->PostTask(FROM_HERE,
                           base::Bind(&PostWorkerTask, history_service_, work,
                                      cancelable_tracker_.get(),
                                      work_done_or_stopped(), &error))) {
    work_done_or_stopped()->Wait();
  } else {
    error = syncer::CANNOT_DO_WORK;
  }
  return error;
}

syncer::ModelSafeGroup HistoryModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_HISTORY;
}

HistoryModelWorker::~HistoryModelWorker() {
  // The base::CancelableTaskTracker class is not thread-safe and must only be
  // used from a single thread but the current object may not be destroyed from
  // the UI thread, so delete it from the UI thread.
  ui_thread_->DeleteSoon(FROM_HERE, cancelable_tracker_.release());
}

}  // namespace browser_sync
