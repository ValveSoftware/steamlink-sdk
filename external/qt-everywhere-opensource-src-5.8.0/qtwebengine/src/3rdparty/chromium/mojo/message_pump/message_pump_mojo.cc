// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/message_pump/message_pump_mojo.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/containers/small_map.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "mojo/message_pump/message_pump_mojo_handler.h"
#include "mojo/message_pump/time_helper.h"
#include "mojo/public/c/system/wait_set.h"

namespace mojo {
namespace common {
namespace {

base::LazyInstance<base::ThreadLocalPointer<MessagePumpMojo> >::Leaky
    g_tls_current_pump = LAZY_INSTANCE_INITIALIZER;

MojoDeadline TimeTicksToMojoDeadline(base::TimeTicks time_ticks,
                                     base::TimeTicks now) {
  // The is_null() check matches that of HandleWatcher as well as how
  // |delayed_work_time| is used.
  if (time_ticks.is_null())
    return MOJO_DEADLINE_INDEFINITE;
  const int64_t delta = (time_ticks - now).InMicroseconds();
  return delta < 0 ? static_cast<MojoDeadline>(0) :
                     static_cast<MojoDeadline>(delta);
}

}  // namespace

struct MessagePumpMojo::RunState {
  RunState() : should_quit(false) {}

  base::TimeTicks delayed_work_time;

  bool should_quit;
};

MessagePumpMojo::MessagePumpMojo()
    : run_state_(NULL),
      next_handler_id_(0),
      event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
             base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(!current())
      << "There is already a MessagePumpMojo instance on this thread.";
  g_tls_current_pump.Pointer()->Set(this);

  MojoResult result = CreateMessagePipe(nullptr, &read_handle_, &write_handle_);
  CHECK_EQ(result, MOJO_RESULT_OK);
  CHECK(read_handle_.is_valid());
  CHECK(write_handle_.is_valid());

  MojoHandle handle;
  result = MojoCreateWaitSet(&handle);
  CHECK_EQ(result, MOJO_RESULT_OK);
  wait_set_handle_.reset(Handle(handle));
  CHECK(wait_set_handle_.is_valid());

  result =
      MojoAddHandle(wait_set_handle_.get().value(), read_handle_.get().value(),
                    MOJO_HANDLE_SIGNAL_READABLE);
  CHECK_EQ(result, MOJO_RESULT_OK);
}

MessagePumpMojo::~MessagePumpMojo() {
  DCHECK_EQ(this, current());
  g_tls_current_pump.Pointer()->Set(NULL);
}

// static
std::unique_ptr<base::MessagePump> MessagePumpMojo::Create() {
  return std::unique_ptr<MessagePump>(new MessagePumpMojo());
}

// static
MessagePumpMojo* MessagePumpMojo::current() {
  return g_tls_current_pump.Pointer()->Get();
}

void MessagePumpMojo::AddHandler(MessagePumpMojoHandler* handler,
                                 const Handle& handle,
                                 MojoHandleSignals wait_signals,
                                 base::TimeTicks deadline) {
  CHECK(handler);
  DCHECK(handle.is_valid());
  // Assume it's an error if someone tries to reregister an existing handle.
  CHECK_EQ(0u, handlers_.count(handle));
  Handler handler_data;
  handler_data.handler = handler;
  handler_data.wait_signals = wait_signals;
  handler_data.deadline = deadline;
  handler_data.id = next_handler_id_++;
  handlers_[handle] = handler_data;
  if (!deadline.is_null()) {
    bool inserted = deadline_handles_.insert(handle).second;
    DCHECK(inserted);
  }

  MojoResult result = MojoAddHandle(wait_set_handle_.get().value(),
                                    handle.value(), wait_signals);
  // Because stopping a HandleWatcher is now asynchronous, it's possible for the
  // handle to no longer be open at this point.
  CHECK(result == MOJO_RESULT_OK || result == MOJO_RESULT_INVALID_ARGUMENT);
}

void MessagePumpMojo::RemoveHandler(const Handle& handle) {
  MojoResult result =
      MojoRemoveHandle(wait_set_handle_.get().value(), handle.value());
  // At this point, it's possible that the handle has been closed, which would
  // cause MojoRemoveHandle() to return MOJO_RESULT_INVALID_ARGUMENT. It's also
  // possible for the handle to have already been removed, so all of the
  // possible error codes are valid here.
  CHECK(result == MOJO_RESULT_OK || result == MOJO_RESULT_NOT_FOUND ||
        result == MOJO_RESULT_INVALID_ARGUMENT);

  handlers_.erase(handle);
  deadline_handles_.erase(handle);
}

void MessagePumpMojo::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MessagePumpMojo::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessagePumpMojo::Run(Delegate* delegate) {
  RunState run_state;
  RunState* old_state = NULL;
  {
    base::AutoLock auto_lock(run_state_lock_);
    old_state = run_state_;
    run_state_ = &run_state;
  }
  DoRunLoop(&run_state, delegate);
  {
    base::AutoLock auto_lock(run_state_lock_);
    run_state_ = old_state;
  }
}

void MessagePumpMojo::Quit() {
  base::AutoLock auto_lock(run_state_lock_);
  if (run_state_)
    run_state_->should_quit = true;
}

void MessagePumpMojo::ScheduleWork() {
  SignalControlPipe();
}

void MessagePumpMojo::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  base::AutoLock auto_lock(run_state_lock_);
  if (!run_state_)
    return;
  run_state_->delayed_work_time = delayed_work_time;
}

void MessagePumpMojo::DoRunLoop(RunState* run_state, Delegate* delegate) {
  bool more_work_is_plausible = true;
  for (;;) {
    const bool block = !more_work_is_plausible;
    if (read_handle_.is_valid()) {
      more_work_is_plausible = DoInternalWork(*run_state, block);
    } else {
      more_work_is_plausible = DoNonMojoWork(*run_state, block);
    }

    if (run_state->should_quit)
      break;

    more_work_is_plausible |= delegate->DoWork();
    if (run_state->should_quit)
      break;

    more_work_is_plausible |= delegate->DoDelayedWork(
        &run_state->delayed_work_time);
    if (run_state->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = delegate->DoIdleWork();
    if (run_state->should_quit)
      break;
  }
}

bool MessagePumpMojo::DoInternalWork(const RunState& run_state, bool block) {
  bool did_work = block;
  if (block) {
    // If the wait isn't blocking (deadline == 0), there's no point in waiting.
    // Wait sets do not require a wait operation to be performed in order to
    // retreive any ready handles. Performing a wait with deadline == 0 is
    // unnecessary work.
    did_work = WaitForReadyHandles(run_state);
  }

  did_work |= ProcessReadyHandles();
  did_work |= RemoveExpiredHandles();

  return did_work;
}

bool MessagePumpMojo::DoNonMojoWork(const RunState& run_state, bool block) {
  bool did_work = block;
  if (block) {
    const MojoDeadline deadline = GetDeadlineForWait(run_state);
    // Stolen from base/message_loop/message_pump_default.cc
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    if (deadline == MOJO_DEADLINE_INDEFINITE) {
      event_.Wait();
    } else {
      if (deadline > 0) {
        event_.TimedWait(base::TimeDelta::FromMicroseconds(deadline));
      } else {
        did_work = false;
      }
    }
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }

  did_work |= RemoveExpiredHandles();

  return did_work;
}

bool MessagePumpMojo::WaitForReadyHandles(const RunState& run_state) const {
  const MojoDeadline deadline = GetDeadlineForWait(run_state);
  const MojoResult wait_result = Wait(
      wait_set_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE, deadline, nullptr);
  if (wait_result == MOJO_RESULT_OK) {
    // Handles may be ready. Or not since wake-ups can be spurious in certain
    // circumstances.
    return true;
  } else if (wait_result == MOJO_RESULT_DEADLINE_EXCEEDED) {
    return false;
  }

  base::debug::Alias(&wait_result);
  // Unexpected result is likely fatal, crash so we can determine cause.
  CHECK(false);
  return false;
}

bool MessagePumpMojo::ProcessReadyHandles() {
  // Maximum number of handles to retrieve and process. Experimentally, the 95th
  // percentile is 1 handle, and the long-term average is 1.1. However, this has
  // been seen to reach >10 under heavy load. 8 is a hand-wavy compromise.
  const uint32_t kMaxServiced = 8;
  uint32_t num_ready_handles = kMaxServiced;
  MojoHandle handles[kMaxServiced];
  MojoResult handle_results[kMaxServiced];

  const MojoResult get_result =
      MojoGetReadyHandles(wait_set_handle_.get().value(), &num_ready_handles,
                          handles, handle_results, nullptr);
  CHECK(get_result == MOJO_RESULT_OK || get_result == MOJO_RESULT_SHOULD_WAIT);
  if (get_result != MOJO_RESULT_OK)
    return false;

  DCHECK(num_ready_handles);
  DCHECK_LE(num_ready_handles, kMaxServiced);
  // Do this in two steps, because notifying a handler may remove/add other
  // handles that may have also been woken up.
  // First, enumerate the IDs of the ready handles. Then, iterate over the
  // handles and only take action if the ID hasn't changed.
  // Since the size of this map is bounded by |kMaxServiced|, use a SmallMap to
  // avoid the per-element allocation.
  base::SmallMap<std::map<Handle, int>, kMaxServiced> ready_handles;
  for (uint32_t i = 0; i < num_ready_handles; i++) {
    const Handle handle = Handle(handles[i]);
    // Skip the control handle. It's special.
    if (handle.value() == read_handle_.get().value())
      continue;
    DCHECK(handle.is_valid());
    const auto it = handlers_.find(handle);
    // Skip handles that have been removed. This is possible because
    // RemoveHandler() can be called with a handle that has been closed. Because
    // the handle is closed, the MojoRemoveHandle() call in RemoveHandler()
    // would have failed, but the handle is still in the wait set. Once the
    // handle is retrieved using MojoGetReadyHandles(), it is implicitly removed
    // from the set. The result is either the pending result that existed when
    // the handle was closed, or |MOJO_RESULT_CANCELLED| to indicate that the
    // handle was closed.
    if (it == handlers_.end())
      continue;
    ready_handles[handle] = it->second.id;
  }

  for (uint32_t i = 0; i < num_ready_handles; i++) {
    const Handle handle = Handle(handles[i]);

    // If the handle has been removed, or it's ID has changed, skip over it.
    // If the handle's ID has changed, and it still satisfies its signals,
    // then it'll be caught in the next message pump iteration.
    const auto it = handlers_.find(handle);
    if ((handle.value() != read_handle_.get().value()) &&
        (it == handlers_.end() || it->second.id != ready_handles[handle])) {
      continue;
    }

    switch (handle_results[i]) {
      case MOJO_RESULT_CANCELLED:
      case MOJO_RESULT_FAILED_PRECONDITION:
        DVLOG(1) << "Error: " << handle_results[i]
                 << " handle: " << handle.value();
        if (handle.value() == read_handle_.get().value()) {
          // The Mojo EDK is shutting down. We can't just quit the message pump
          // because that may cause the thread to quit, which causes the
          // thread's MessageLoop to be destroyed, which races with any use of
          // |Thread::task_runner()|. So instead, we enter a "dumb" mode which
          // bypasses Mojo and just acts like a trivial message pump. That way,
          // we can wait for the usual thread exiting mechanism to happen.
          // The dumb mode is indicated by releasing the control pipe's read
          // handle.
          read_handle_.reset();
        } else {
          SignalHandleError(handle, handle_results[i]);
        }
        break;
      case MOJO_RESULT_OK:
        if (handle.value() == read_handle_.get().value()) {
          DVLOG(1) << "Signaled control pipe";
          // Control pipe was written to.
          ReadMessageRaw(read_handle_.get(), nullptr, nullptr, nullptr, nullptr,
                         MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
        } else {
          DVLOG(1) << "Handle ready: " << handle.value();
          SignalHandleReady(handle);
        }
        break;
      default:
        base::debug::Alias(&i);
        base::debug::Alias(&handle_results[i]);
        // Unexpected result is likely fatal, crash so we can determine cause.
        CHECK(false);
    }
  }
  return true;
}

bool MessagePumpMojo::RemoveExpiredHandles() {
  bool removed = false;
  // Notify and remove any handlers whose time has expired. First, iterate over
  // the set of handles that have a deadline, and add the expired handles to a
  // map of <Handle, id>. Then, iterate over those expired handles and remove
  // them. The two-step process is because a handler can add/remove new
  // handlers.
  std::map<Handle, int> expired_handles;
  const base::TimeTicks now(internal::NowTicks());
  for (const Handle handle : deadline_handles_) {
    const auto it = handlers_.find(handle);
    // Expect any handle in |deadline_handles_| to also be in |handlers_| since
    // the two are modified in lock-step.
    DCHECK(it != handlers_.end());
    if (!it->second.deadline.is_null() && it->second.deadline < now)
      expired_handles[handle] = it->second.id;
  }
  for (const auto& pair : expired_handles) {
    auto it = handlers_.find(pair.first);
    // Don't need to check deadline again since it can't change if id hasn't
    // changed.
    if (it != handlers_.end() && it->second.id == pair.second) {
      SignalHandleError(pair.first, MOJO_RESULT_DEADLINE_EXCEEDED);
      removed = true;
    }
  }
  return removed;
}

void MessagePumpMojo::SignalControlPipe() {
  const MojoResult result =
      WriteMessageRaw(write_handle_.get(), NULL, 0, NULL, 0,
                      MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Mojo EDK is shutting down.
    event_.Signal();
    return;
  }

  // If we can't write we likely won't wake up the thread and there is a strong
  // chance we'll deadlock.
  CHECK_EQ(MOJO_RESULT_OK, result);
}

MojoDeadline MessagePumpMojo::GetDeadlineForWait(
    const RunState& run_state) const {
  const base::TimeTicks now(internal::NowTicks());
  MojoDeadline deadline = TimeTicksToMojoDeadline(run_state.delayed_work_time,
                                                  now);
  for (const Handle handle : deadline_handles_) {
    auto it = handlers_.find(handle);
    DCHECK(it != handlers_.end());
    deadline = std::min(
        TimeTicksToMojoDeadline(it->second.deadline, now), deadline);
  }
  return deadline;
}

void MessagePumpMojo::SignalHandleReady(Handle handle) {
  auto it = handlers_.find(handle);
  DCHECK(it != handlers_.end());
  MessagePumpMojoHandler* handler = it->second.handler;

  WillSignalHandler();
  handler->OnHandleReady(handle);
  DidSignalHandler();
}

void MessagePumpMojo::SignalHandleError(Handle handle, MojoResult result) {
  auto it = handlers_.find(handle);
  DCHECK(it != handlers_.end());
  MessagePumpMojoHandler* handler = it->second.handler;

  RemoveHandler(handle);
  WillSignalHandler();
  handler->OnHandleError(handle, result);
  DidSignalHandler();
}

void MessagePumpMojo::WillSignalHandler() {
  FOR_EACH_OBSERVER(Observer, observers_, WillSignalHandler());
}

void MessagePumpMojo::DidSignalHandler() {
  FOR_EACH_OBSERVER(Observer, observers_, DidSignalHandler());
}

}  // namespace common
}  // namespace mojo
