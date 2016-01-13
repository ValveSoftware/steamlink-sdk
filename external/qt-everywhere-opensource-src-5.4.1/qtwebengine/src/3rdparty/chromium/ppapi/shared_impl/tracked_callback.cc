// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/tracked_callback.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_message_loop.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_message_loop_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {

namespace {

bool IsMainThread() {
  return PpapiGlobals::Get()
      ->GetMainThreadMessageLoop()
      ->BelongsToCurrentThread();
}

int32_t RunCompletionTask(TrackedCallback::CompletionTask completion_task,
                          int32_t result) {
  int32_t task_result = completion_task.Run(result);
  if (result != PP_ERROR_ABORTED)
    result = task_result;
  return result;
}

}  // namespace

// TrackedCallback -------------------------------------------------------------

// Note: don't keep a Resource* since it may go out of scope before us.
TrackedCallback::TrackedCallback(Resource* resource,
                                 const PP_CompletionCallback& callback)
    : is_scheduled_(false),
      resource_id_(resource ? resource->pp_resource() : 0),
      completed_(false),
      aborted_(false),
      callback_(callback),
      target_loop_(PpapiGlobals::Get()->GetCurrentMessageLoop()),
      result_for_blocked_callback_(PP_OK) {
  // Note that target_loop_ may be NULL at this point, if the plugin has not
  // attached a loop to this thread, or if this is an in-process plugin.
  // The Enter class should handle checking this for us.

  // TODO(dmichael): Add tracking at the instance level, for callbacks that only
  // have an instance (e.g. for MouseLock).
  if (resource) {
    tracker_ = PpapiGlobals::Get()->GetCallbackTrackerForInstance(
        resource->pp_instance());
    tracker_->Add(make_scoped_refptr(this));
  }

  base::Lock* proxy_lock = ProxyLock::Get();
  if (proxy_lock) {
    // If the proxy_lock is valid, we're running out-of-process, and locking
    // is enabled.
    if (is_blocking()) {
      // This is a blocking completion callback, so we will need a condition
      // variable for blocking & signalling the calling thread.
      operation_completed_condvar_.reset(
          new base::ConditionVariable(proxy_lock));
    } else {
      // It's a non-blocking callback, so we should have a MessageLoopResource
      // to dispatch to. Note that we don't error check here, though. Later,
      // EnterResource::SetResult will check to make sure the callback is valid
      // and take appropriate action.
    }
  }
}

TrackedCallback::~TrackedCallback() {}

void TrackedCallback::Abort() { Run(PP_ERROR_ABORTED); }

void TrackedCallback::PostAbort() { PostRun(PP_ERROR_ABORTED); }

void TrackedCallback::Run(int32_t result) {
  // Only allow the callback to be run once. Note that this also covers the case
  // where the callback was previously Aborted because its associated Resource
  // went away. The callback may live on for a while because of a reference from
  // a Closure. But when the Closure runs, Run() quietly does nothing, and the
  // callback will go away when all referring Closures go away.
  if (completed())
    return;
  if (result == PP_ERROR_ABORTED)
    aborted_ = true;

  // Note that this call of Run() may have been scheduled prior to Abort() or
  // PostAbort() being called. If we have been told to Abort, that always
  // trumps a result that was scheduled before, so we should make sure to pass
  // PP_ERROR_ABORTED.
  if (aborted())
    result = PP_ERROR_ABORTED;

  if (is_blocking()) {
    // If the condition variable is invalid, there are two possibilities. One,
    // we're running in-process, in which case the call should have come in on
    // the main thread and we should have returned PP_ERROR_BLOCKS_MAIN_THREAD
    // well before this. Otherwise, this callback was not created as a
    // blocking callback. Either way, there's some internal error.
    if (!operation_completed_condvar_.get()) {
      NOTREACHED();
      return;
    }
    result_for_blocked_callback_ = result;
    // Retain ourselves, since MarkAsCompleted will remove us from the
    // tracker. Then MarkAsCompleted before waking up the blocked thread,
    // which could potentially re-enter.
    scoped_refptr<TrackedCallback> thiz(this);
    MarkAsCompleted();
    // Wake up the blocked thread. See BlockUntilComplete for where the thread
    // Wait()s.
    operation_completed_condvar_->Signal();
  } else {
    // If there's a target_loop_, and we're not on the right thread, we need to
    // post to target_loop_.
    if (target_loop_.get() &&
        target_loop_.get() != PpapiGlobals::Get()->GetCurrentMessageLoop()) {
      PostRun(result);
      return;
    }

    // Copy callback fields now, since |MarkAsCompleted()| may delete us.
    PP_CompletionCallback callback = callback_;
    CompletionTask completion_task = completion_task_;
    completion_task_.Reset();
    // Do this before running the callback in case of reentrancy from running
    // the completion task.
    MarkAsCompleted();

    if (!completion_task.is_null())
      result = RunCompletionTask(completion_task, result);

    // TODO(dmichael): Associate a message loop with the callback; if it's not
    // the same as the current thread's loop, then post it to the right loop.
    CallWhileUnlocked(PP_RunCompletionCallback, &callback, result);
  }
}

void TrackedCallback::PostRun(int32_t result) {
  if (completed()) {
    NOTREACHED();
    return;
  }
  if (result == PP_ERROR_ABORTED)
    aborted_ = true;
  // We might abort when there's already a scheduled callback, but callers
  // should never try to PostRun more than once otherwise.
  DCHECK(result == PP_ERROR_ABORTED || !is_scheduled_);

  if (is_blocking()) {
    // We might not have a MessageLoop to post to, so we must call Run()
    // directly.
    Run(result);
  } else {
    base::Closure callback_closure(
        RunWhileLocked(base::Bind(&TrackedCallback::Run, this, result)));
    if (target_loop_) {
      target_loop_->PostClosure(FROM_HERE, callback_closure, 0);
    } else {
      // We must be running in-process and on the main thread (the Enter
      // classes protect against having a null target_loop_ otherwise).
      DCHECK(IsMainThread());
      DCHECK(PpapiGlobals::Get()->IsHostGlobals());
      base::MessageLoop::current()->PostTask(FROM_HERE, callback_closure);
    }
  }
  is_scheduled_ = true;
}

void TrackedCallback::set_completion_task(
    const CompletionTask& completion_task) {
  DCHECK(completion_task_.is_null());
  completion_task_ = completion_task;
}

// static
bool TrackedCallback::IsPending(
    const scoped_refptr<TrackedCallback>& callback) {
  if (!callback.get())
    return false;
  if (callback->aborted())
    return false;
  return !callback->completed();
}

// static
bool TrackedCallback::IsScheduledToRun(
    const scoped_refptr<TrackedCallback>& callback) {
  return IsPending(callback) && callback->is_scheduled_;
}

int32_t TrackedCallback::BlockUntilComplete() {
  // Note, we are already holding the proxy lock in all these methods, including
  // this one (see ppapi/thunk/enter.cc for where it gets acquired).

  // It doesn't make sense to wait on a non-blocking callback. Furthermore,
  // BlockUntilComplete should never be called for in-process plugins, where
  // blocking callbacks are not supported.
  CHECK(operation_completed_condvar_.get());
  if (!is_blocking() || !operation_completed_condvar_.get()) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }

  while (!completed())
    operation_completed_condvar_->Wait();

  if (!completion_task_.is_null()) {
    result_for_blocked_callback_ =
        RunCompletionTask(completion_task_, result_for_blocked_callback_);
    completion_task_.Reset();
  }
  return result_for_blocked_callback_;
}

void TrackedCallback::MarkAsCompleted() {
  DCHECK(!completed());

  // We will be removed; maintain a reference to ensure we won't be deleted
  // until we're done.
  scoped_refptr<TrackedCallback> thiz = this;
  completed_ = true;
  // We may not have a valid resource, in which case we're not in the tracker.
  if (resource_id_)
    tracker_->Remove(thiz);
  tracker_ = NULL;
}

}  // namespace ppapi
