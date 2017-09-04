// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/sync/browser/password_model_worker.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync/base/scoped_event_signal.h"

namespace browser_sync {

namespace {

void CallDoWorkAndSignalEvent(const syncer::WorkCallback& work,
                              syncer::ScopedEventSignal scoped_event_signal,
                              syncer::SyncerError* error_info) {
  *error_info = work.Run();
  // The event in |scoped_event_signal| is signaled at the end of this scope.
}

}  // namespace

PasswordModelWorker::PasswordModelWorker(
    const scoped_refptr<password_manager::PasswordStore>& password_store)
    : password_store_(password_store) {
  DCHECK(password_store.get());
}

syncer::SyncerError PasswordModelWorker::DoWorkAndWaitUntilDoneImpl(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error = syncer::UNSET;

  // Signaled when the task is deleted, i.e. after it runs or when it is
  // abandoned.
  base::WaitableEvent work_done_or_abandoned(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  bool scheduled = false;
  {
    base::AutoLock lock(password_store_lock_);
    if (!password_store_.get())
      return syncer::CANNOT_DO_WORK;

    scheduled = password_store_->ScheduleTask(base::Bind(
        &CallDoWorkAndSignalEvent, work,
        base::Passed(syncer::ScopedEventSignal(&work_done_or_abandoned)),
        &error));
  }

  if (scheduled)
    work_done_or_abandoned.Wait();
  else
    error = syncer::CANNOT_DO_WORK;
  return error;
}

syncer::ModelSafeGroup PasswordModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_PASSWORD;
}

bool PasswordModelWorker::IsOnModelThread() {
  // Ideally PasswordStore would expose a way to check whether this is the
  // thread it does work on. Since it doesn't, just return true to bypass a
  // CHECK in the sync code.
  return true;
}

PasswordModelWorker::~PasswordModelWorker() {}

void PasswordModelWorker::RequestStop() {
  ModelSafeWorker::RequestStop();

  base::AutoLock lock(password_store_lock_);
  password_store_ = NULL;
}

}  // namespace browser_sync
