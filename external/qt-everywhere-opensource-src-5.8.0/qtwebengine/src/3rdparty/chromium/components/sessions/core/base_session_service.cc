// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/base_session_service.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sessions/core/base_session_service_delegate.h"
#include "components/sessions/core/session_backend.h"

// BaseSessionService ---------------------------------------------------------

namespace sessions {
namespace {

// Helper used by ScheduleGetLastSessionCommands. It runs callback on TaskRunner
// thread if it's not canceled.
void RunIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    const BaseSessionService::GetCommandsCallback& callback,
    ScopedVector<SessionCommand> commands) {
  if (is_canceled.Run())
    return;
  callback.Run(std::move(commands));
}

void PostOrRunInternalGetCommandsCallback(
    base::TaskRunner* task_runner,
    const BaseSessionService::GetCommandsCallback& callback,
    ScopedVector<SessionCommand> commands) {
  if (task_runner->RunsTasksOnCurrentThread()) {
    callback.Run(std::move(commands));
  } else {
    task_runner->PostTask(FROM_HERE,
                          base::Bind(callback, base::Passed(&commands)));
  }
}

}  // namespace

// Delay between when a command is received, and when we save it to the
// backend.
static const int kSaveDelayMS = 2500;

BaseSessionService::BaseSessionService(
    SessionType type,
    const base::FilePath& path,
    BaseSessionServiceDelegate* delegate)
    : pending_reset_(false),
      commands_since_reset_(0),
      delegate_(delegate),
      sequence_token_(delegate_->GetBlockingPool()->GetSequenceToken()),
      weak_factory_(this) {
  backend_ = new SessionBackend(type, path);
  DCHECK(backend_.get());
}

BaseSessionService::~BaseSessionService() {}

void BaseSessionService::MoveCurrentSessionToLastSession() {
  Save();
  RunTaskOnBackendThread(
      FROM_HERE, base::Bind(&SessionBackend::MoveCurrentSessionToLastSession,
                            backend_));
}

void BaseSessionService::DeleteLastSession() {
  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::DeleteLastSession, backend_));
}

void BaseSessionService::ScheduleCommand(
    std::unique_ptr<SessionCommand> command) {
  DCHECK(command);
  commands_since_reset_++;
  pending_commands_.push_back(command.release());
  StartSaveTimer();
}

void BaseSessionService::AppendRebuildCommand(
    std::unique_ptr<SessionCommand> command) {
  DCHECK(command);
  pending_commands_.push_back(command.release());
}

void BaseSessionService::EraseCommand(SessionCommand* old_command) {
  ScopedVector<SessionCommand>::iterator it =
      std::find(pending_commands_.begin(),
                pending_commands_.end(),
                old_command);
  CHECK(it != pending_commands_.end());
  pending_commands_.erase(it);
}

void BaseSessionService::SwapCommand(
    SessionCommand* old_command,
    std::unique_ptr<SessionCommand> new_command) {
  ScopedVector<SessionCommand>::iterator it =
      std::find(pending_commands_.begin(),
                pending_commands_.end(),
                old_command);
  CHECK(it != pending_commands_.end());
  *it = new_command.release();
  delete old_command;
}

void BaseSessionService::ClearPendingCommands() {
  pending_commands_.clear();
}

void BaseSessionService::StartSaveTimer() {
  // Don't start a timer when testing.
  if (delegate_->ShouldUseDelayedSave() && base::MessageLoop::current() &&
      !weak_factory_.HasWeakPtrs()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BaseSessionService::Save, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kSaveDelayMS));
  }
}

void BaseSessionService::Save() {
  // Inform the delegate that we will save the commands now, giving it the
  // opportunity to append more commands.
  delegate_->OnWillSaveCommands();

  if (pending_commands_.empty())
    return;

  // We create a new ScopedVector which will receive all elements from the
  // current commands. This will also clear the current list.
  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::AppendCommands, backend_,
                 base::Passed(&pending_commands_),
                 pending_reset_));

  if (pending_reset_) {
    commands_since_reset_ = 0;
    pending_reset_ = false;
  }

  delegate_->OnSavedCommands();
}

base::CancelableTaskTracker::TaskId
BaseSessionService::ScheduleGetLastSessionCommands(
    const GetCommandsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId id =
      tracker->NewTrackedTaskId(&is_canceled);

  GetCommandsCallback run_if_not_canceled =
      base::Bind(&RunIfNotCanceled, is_canceled, callback);

  GetCommandsCallback callback_runner =
      base::Bind(&PostOrRunInternalGetCommandsCallback,
                 base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                 run_if_not_canceled);

  RunTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&SessionBackend::ReadLastSessionCommands, backend_,
                 is_canceled, callback_runner));
  return id;
}

void BaseSessionService::RunTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  base::SequencedWorkerPool* pool = delegate_->GetBlockingPool();
  if (!pool->IsShutdownInProgress()) {
    pool->PostSequencedWorkerTask(sequence_token_, from_here, task);
  } else {
    // Fall back to executing on the main thread if the sequence
    // worker pool has been requested to shutdown (around shutdown
    // time).
    task.Run();
  }
}

}  // namespace sessions
