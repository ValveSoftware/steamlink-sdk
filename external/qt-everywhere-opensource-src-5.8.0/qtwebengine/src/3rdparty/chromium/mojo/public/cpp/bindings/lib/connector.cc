// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/connector.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/sync_handle_watcher.h"

namespace mojo {

namespace {

// Similar to base::AutoLock, except that it does nothing if |lock| passed into
// the constructor is null.
class MayAutoLock {
 public:
  explicit MayAutoLock(base::Lock* lock) : lock_(lock) {
    if (lock_)
      lock_->Acquire();
  }

  ~MayAutoLock() {
    if (lock_) {
      lock_->AssertAcquired();
      lock_->Release();
    }
  }

 private:
  base::Lock* lock_;
  DISALLOW_COPY_AND_ASSIGN(MayAutoLock);
};

}  // namespace

// ----------------------------------------------------------------------------

Connector::Connector(ScopedMessagePipeHandle message_pipe,
                     ConnectorConfig config,
                     scoped_refptr<base::SingleThreadTaskRunner> runner)
    : message_pipe_(std::move(message_pipe)),
      incoming_receiver_(nullptr),
      task_runner_(std::move(runner)),
      handle_watcher_(task_runner_),
      error_(false),
      drop_writes_(false),
      enforce_errors_from_incoming_receiver_(true),
      paused_(false),
      lock_(config == MULTI_THREADED_SEND ? new base::Lock : nullptr),
      allow_woken_up_by_others_(false),
      sync_handle_watcher_callback_count_(0),
      weak_factory_(this) {
  weak_self_ = weak_factory_.GetWeakPtr();
  // Even though we don't have an incoming receiver, we still want to monitor
  // the message pipe to know if is closed or encounters an error.
  WaitToReadMore();
}

Connector::~Connector() {
  DCHECK(thread_checker_.CalledOnValidThread());

  CancelWait();
}

void Connector::CloseMessagePipe() {
  DCHECK(thread_checker_.CalledOnValidThread());

  CancelWait();
  MayAutoLock locker(lock_.get());
  message_pipe_.reset();
}

ScopedMessagePipeHandle Connector::PassMessagePipe() {
  DCHECK(thread_checker_.CalledOnValidThread());

  CancelWait();
  MayAutoLock locker(lock_.get());
  return std::move(message_pipe_);
}

void Connector::RaiseError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  HandleError(true, true);
}

bool Connector::WaitForIncomingMessage(MojoDeadline deadline) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error_)
    return false;

  ResumeIncomingMethodCallProcessing();

  MojoResult rv =
      Wait(message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE, deadline, nullptr);
  if (rv == MOJO_RESULT_SHOULD_WAIT || rv == MOJO_RESULT_DEADLINE_EXCEEDED)
    return false;
  if (rv != MOJO_RESULT_OK) {
    // Users that call WaitForIncomingMessage() should expect their code to be
    // re-entered, so we call the error handler synchronously.
    HandleError(rv != MOJO_RESULT_FAILED_PRECONDITION, false);
    return false;
  }
  ignore_result(ReadSingleMessage(&rv));
  return (rv == MOJO_RESULT_OK);
}

void Connector::PauseIncomingMethodCallProcessing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (paused_)
    return;

  paused_ = true;
  CancelWait();
}

void Connector::ResumeIncomingMethodCallProcessing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!paused_)
    return;

  paused_ = false;
  WaitToReadMore();
}

bool Connector::Accept(Message* message) {
  DCHECK(lock_ || thread_checker_.CalledOnValidThread());

  // It shouldn't hurt even if |error_| may be changed by a different thread at
  // the same time. The outcome is that we may write into |message_pipe_| after
  // encountering an error, which should be fine.
  if (error_)
    return false;

  MayAutoLock locker(lock_.get());

  if (!message_pipe_.is_valid() || drop_writes_)
    return true;

  MojoResult rv =
      WriteMessageNew(message_pipe_.get(), message->TakeMojoMessage(),
                      MOJO_WRITE_MESSAGE_FLAG_NONE);

  switch (rv) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // There's no point in continuing to write to this pipe since the other
      // end is gone. Avoid writing any future messages. Hide write failures
      // from the caller since we'd like them to continue consuming any backlog
      // of incoming messages before regarding the message pipe as closed.
      drop_writes_ = true;
      break;
    case MOJO_RESULT_BUSY:
      // We'd get a "busy" result if one of the message's handles is:
      //   - |message_pipe_|'s own handle;
      //   - simultaneously being used on another thread; or
      //   - in a "busy" state that prohibits it from being transferred (e.g.,
      //     a data pipe handle in the middle of a two-phase read/write,
      //     regardless of which thread that two-phase read/write is happening
      //     on).
      // TODO(vtl): I wonder if this should be a |DCHECK()|. (But, until
      // crbug.com/389666, etc. are resolved, this will make tests fail quickly
      // rather than hanging.)
      CHECK(false) << "Race condition or other bug detected";
      return false;
    default:
      // This particular write was rejected, presumably because of bad input.
      // The pipe is not necessarily in a bad state.
      return false;
  }
  return true;
}

void Connector::AllowWokenUpBySyncWatchOnSameThread() {
  DCHECK(thread_checker_.CalledOnValidThread());

  allow_woken_up_by_others_ = true;

  EnsureSyncWatcherExists();
  sync_watcher_->AllowWokenUpBySyncWatchOnSameThread();
}

bool Connector::SyncWatch(const bool* should_stop) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error_)
    return false;

  ResumeIncomingMethodCallProcessing();

  EnsureSyncWatcherExists();
  return sync_watcher_->SyncWatch(should_stop);
}

void Connector::OnWatcherHandleReady(MojoResult result) {
  OnHandleReadyInternal(result);
}

void Connector::OnSyncHandleWatcherHandleReady(MojoResult result) {
  base::WeakPtr<Connector> weak_self(weak_self_);

  sync_handle_watcher_callback_count_++;
  OnHandleReadyInternal(result);
  // At this point, this object might have been deleted.
  if (weak_self)
    sync_handle_watcher_callback_count_--;
}

void Connector::OnHandleReadyInternal(MojoResult result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (result != MOJO_RESULT_OK) {
    HandleError(result != MOJO_RESULT_FAILED_PRECONDITION, false);
    return;
  }
  ReadAllAvailableMessages();
  // At this point, this object might have been deleted. Return.
}

void Connector::WaitToReadMore() {
  CHECK(!paused_);
  DCHECK(!handle_watcher_.IsWatching());

  MojoResult rv = handle_watcher_.Start(
      message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::Bind(&Connector::OnWatcherHandleReady,
                 base::Unretained(this)));

  if (rv != MOJO_RESULT_OK) {
    // If the watch failed because the handle is invalid or its conditions can
    // no longer be met, we signal the error asynchronously to avoid reentry.
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Connector::OnWatcherHandleReady, weak_self_, rv));
  }

  if (allow_woken_up_by_others_) {
    EnsureSyncWatcherExists();
    sync_watcher_->AllowWokenUpBySyncWatchOnSameThread();
  }
}

bool Connector::ReadSingleMessage(MojoResult* read_result) {
  CHECK(!paused_);

  bool receiver_result = false;

  // Detect if |this| was destroyed during message dispatch. Allow for the
  // possibility of re-entering ReadMore() through message dispatch.
  base::WeakPtr<Connector> weak_self = weak_self_;

  Message message;
  const MojoResult rv = ReadMessage(message_pipe_.get(), &message);
  *read_result = rv;

  if (rv == MOJO_RESULT_OK) {
    receiver_result =
        incoming_receiver_ && incoming_receiver_->Accept(&message);
  }

  if (!weak_self)
    return false;

  if (rv == MOJO_RESULT_SHOULD_WAIT)
    return true;

  if (rv != MOJO_RESULT_OK) {
    HandleError(rv != MOJO_RESULT_FAILED_PRECONDITION, false);
    return false;
  }

  if (enforce_errors_from_incoming_receiver_ && !receiver_result) {
    HandleError(true, false);
    return false;
  }
  return true;
}

void Connector::ReadAllAvailableMessages() {
  while (!error_) {
    MojoResult rv;

    // Return immediately if |this| was destroyed. Do not touch any members!
    if (!ReadSingleMessage(&rv))
      return;

    if (paused_)
      return;

    if (rv == MOJO_RESULT_SHOULD_WAIT)
      break;
  }
}

void Connector::CancelWait() {
  handle_watcher_.Cancel();
  sync_watcher_.reset();
}

void Connector::HandleError(bool force_pipe_reset, bool force_async_handler) {
  if (error_ || !message_pipe_.is_valid())
    return;

  if (paused_) {
    // Enforce calling the error handler asynchronously if the user has paused
    // receiving messages. We need to wait until the user starts receiving
    // messages again.
    force_async_handler = true;
  }

  if (!force_pipe_reset && force_async_handler)
    force_pipe_reset = true;

  if (force_pipe_reset) {
    CancelWait();
    MayAutoLock locker(lock_.get());
    message_pipe_.reset();
    MessagePipe dummy_pipe;
    message_pipe_ = std::move(dummy_pipe.handle0);
  } else {
    CancelWait();
  }

  if (force_async_handler) {
    if (!paused_)
      WaitToReadMore();
  } else {
    error_ = true;
    if (!connection_error_handler_.is_null())
      connection_error_handler_.Run();
  }
}

void Connector::EnsureSyncWatcherExists() {
  if (sync_watcher_)
    return;
  sync_watcher_.reset(new SyncHandleWatcher(
      message_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::Bind(&Connector::OnSyncHandleWatcherHandleReady,
                 base::Unretained(this))));
}

}  // namespace mojo
