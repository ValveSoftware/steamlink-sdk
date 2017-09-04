// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/wait_set_dispatcher.h"

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "mojo/edk/system/awakable.h"

namespace mojo {
namespace edk {

class WaitSetDispatcher::Waiter final : public Awakable {
 public:
  explicit Waiter(WaitSetDispatcher* dispatcher) : dispatcher_(dispatcher) {}
  ~Waiter() {}

  // |Awakable| implementation.
  bool Awake(MojoResult result, uintptr_t context) override {
    // Note: This is called with various Mojo locks held.
    dispatcher_->WakeDispatcher(result, context);
    // Removes |this| from the dispatcher's list of waiters.
    return false;
  }

 private:
  WaitSetDispatcher* const dispatcher_;
};

WaitSetDispatcher::WaitState::WaitState() {}

WaitSetDispatcher::WaitState::WaitState(const WaitState& other) = default;

WaitSetDispatcher::WaitState::~WaitState() {}

WaitSetDispatcher::WaitSetDispatcher()
    : waiter_(new WaitSetDispatcher::Waiter(this)) {}

Dispatcher::Type WaitSetDispatcher::GetType() const {
  return Type::WAIT_SET;
}

MojoResult WaitSetDispatcher::Close() {
  base::AutoLock lock(lock_);

  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  is_closed_ = true;

  {
    base::AutoLock locker(awakable_lock_);
    awakable_list_.CancelAll();
  }

  for (const auto& entry : waiting_dispatchers_)
    entry.second.dispatcher->RemoveAwakable(waiter_.get(), nullptr);
  waiting_dispatchers_.clear();

  base::AutoLock locker(awoken_lock_);
  awoken_queue_.clear();
  processed_dispatchers_.clear();

  return MOJO_RESULT_OK;
}

MojoResult WaitSetDispatcher::AddWaitingDispatcher(
    const scoped_refptr<Dispatcher>& dispatcher,
    MojoHandleSignals signals,
    uintptr_t context) {
  if (dispatcher == this)
    return MOJO_RESULT_INVALID_ARGUMENT;

  base::AutoLock lock(lock_);

  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  uintptr_t dispatcher_handle = reinterpret_cast<uintptr_t>(dispatcher.get());
  auto it = waiting_dispatchers_.find(dispatcher_handle);
  if (it != waiting_dispatchers_.end()) {
    return MOJO_RESULT_ALREADY_EXISTS;
  }

  const MojoResult result = dispatcher->AddAwakable(waiter_.get(), signals,
                                                    dispatcher_handle, nullptr);
  if (result == MOJO_RESULT_INVALID_ARGUMENT) {
    // Dispatcher is closed.
    return result;
  } else if (result != MOJO_RESULT_OK) {
    WakeDispatcher(result, dispatcher_handle);
  }

  WaitState state;
  state.dispatcher = dispatcher;
  state.context = context;
  state.signals = signals;
  bool inserted = waiting_dispatchers_.insert(
      std::make_pair(dispatcher_handle, state)).second;
  DCHECK(inserted);

  return MOJO_RESULT_OK;
}

MojoResult WaitSetDispatcher::RemoveWaitingDispatcher(
    const scoped_refptr<Dispatcher>& dispatcher) {
  uintptr_t dispatcher_handle = reinterpret_cast<uintptr_t>(dispatcher.get());

  base::AutoLock lock(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  auto it = waiting_dispatchers_.find(dispatcher_handle);
  if (it == waiting_dispatchers_.end())
    return MOJO_RESULT_NOT_FOUND;

  dispatcher->RemoveAwakable(waiter_.get(), nullptr);
  // At this point, it should not be possible for |waiter_| to be woken with
  // |dispatcher|.
  waiting_dispatchers_.erase(it);

  base::AutoLock locker(awoken_lock_);
  int num_erased = 0;
  for (auto it = awoken_queue_.begin(); it != awoken_queue_.end();) {
    if (it->first == dispatcher_handle) {
      it = awoken_queue_.erase(it);
      num_erased++;
    } else {
      ++it;
    }
  }
  // The dispatcher should only exist in the queue once.
  DCHECK_LE(num_erased, 1);
  processed_dispatchers_.erase(
      std::remove(processed_dispatchers_.begin(), processed_dispatchers_.end(),
                  dispatcher_handle),
      processed_dispatchers_.end());

  return MOJO_RESULT_OK;
}

MojoResult WaitSetDispatcher::GetReadyDispatchers(
    uint32_t* count,
    DispatcherVector* dispatchers,
    MojoResult* results,
    uintptr_t* contexts) {
  base::AutoLock lock(lock_);

  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  dispatchers->clear();

  // Re-queue any already retrieved dispatchers. These should be the dispatchers
  // that were returned on the last call to this function. This loop is
  // necessary to preserve the logically level-triggering behaviour of waiting
  // in Mojo. In particular, if no action is taken on a signal, that signal
  // continues to be satisfied, and therefore a |MojoWait()| on that
  // handle/signal continues to return immediately.
  std::deque<uintptr_t> pending;
  {
    base::AutoLock locker(awoken_lock_);
    pending.swap(processed_dispatchers_);
  }
  for (uintptr_t d : pending) {
    auto it = waiting_dispatchers_.find(d);
    // Anything in |processed_dispatchers_| should also be in
    // |waiting_dispatchers_| since dispatchers are removed from both in
    // |RemoveWaitingDispatcherImplNoLock()|.
    DCHECK(it != waiting_dispatchers_.end());

    // |awoken_mutex_| cannot be held here because
    // |Dispatcher::AddAwakable()| acquires the Dispatcher's mutex. This
    // mutex is held while running |WakeDispatcher()| below, which needs to
    // acquire |awoken_mutex_|. Holding |awoken_mutex_| here would result in
    // a deadlock.
    const MojoResult result = it->second.dispatcher->AddAwakable(
        waiter_.get(), it->second.signals, d, nullptr);

    if (result == MOJO_RESULT_INVALID_ARGUMENT) {
      // Dispatcher is closed. Implicitly remove it from the wait set since
      // it may be impossible to remove using |MojoRemoveHandle()|.
      waiting_dispatchers_.erase(it);
    } else if (result != MOJO_RESULT_OK) {
      WakeDispatcher(result, d);
    }
  }

  const uint32_t max_woken = *count;
  uint32_t num_woken = 0;

  base::AutoLock locker(awoken_lock_);
  while (!awoken_queue_.empty() && num_woken < max_woken) {
    uintptr_t d = awoken_queue_.front().first;
    MojoResult result = awoken_queue_.front().second;
    awoken_queue_.pop_front();

    auto it = waiting_dispatchers_.find(d);
    DCHECK(it != waiting_dispatchers_.end());

    results[num_woken] = result;
    dispatchers->push_back(it->second.dispatcher);
    if (contexts)
      contexts[num_woken] = it->second.context;

    if (result != MOJO_RESULT_CANCELLED) {
      processed_dispatchers_.push_back(d);
    } else {
      // |MOJO_RESULT_CANCELLED| indicates that the dispatcher was closed.
      // Return it, but also implcitly remove it from the wait set.
      waiting_dispatchers_.erase(it);
    }

    num_woken++;
  }

  *count = num_woken;
  if (!num_woken)
    return MOJO_RESULT_SHOULD_WAIT;

  return MOJO_RESULT_OK;
}

HandleSignalsState WaitSetDispatcher::GetHandleSignalsState() const {
  base::AutoLock lock(lock_);
  return GetHandleSignalsStateNoLock();
}

HandleSignalsState WaitSetDispatcher::GetHandleSignalsStateNoLock() const {
  lock_.AssertAcquired();
  if (is_closed_)
    return HandleSignalsState();

  HandleSignalsState rv;
  rv.satisfiable_signals = MOJO_HANDLE_SIGNAL_READABLE;
  base::AutoLock locker(awoken_lock_);
  if (!awoken_queue_.empty() || !processed_dispatchers_.empty())
    rv.satisfied_signals = MOJO_HANDLE_SIGNAL_READABLE;
  return rv;
}

MojoResult WaitSetDispatcher::AddAwakable(Awakable* awakable,
                                          MojoHandleSignals signals,
                                          uintptr_t context,
                                          HandleSignalsState* signals_state) {
  base::AutoLock lock(lock_);
  // |awakable_lock_| is acquired here instead of immediately before adding to
  // |awakable_list_| because we need to check the signals state and add to
  // |awakable_list_| as an atomic operation. If the pair isn't atomic, it is
  // possible for the signals state to change after it is checked, but before
  // the awakable is added. In that case, the added awakable won't be signalled.
  base::AutoLock awakable_locker(awakable_lock_);
  HandleSignalsState state(GetHandleSignalsStateNoLock());
  if (state.satisfies(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!state.can_satisfy(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  awakable_list_.Add(awakable, signals, context);
  return MOJO_RESULT_OK;
}

void WaitSetDispatcher::RemoveAwakable(Awakable* awakable,
                                       HandleSignalsState* signals_state) {
  {
    base::AutoLock locker(awakable_lock_);
    awakable_list_.Remove(awakable);
  }
  if (signals_state)
    *signals_state = GetHandleSignalsState();
}

bool WaitSetDispatcher::BeginTransit() {
  // You can't transfer wait sets!
  return false;
}

WaitSetDispatcher::~WaitSetDispatcher() {
  DCHECK(waiting_dispatchers_.empty());
  DCHECK(awoken_queue_.empty());
  DCHECK(processed_dispatchers_.empty());
}

void WaitSetDispatcher::WakeDispatcher(MojoResult result, uintptr_t context) {
  {
    base::AutoLock locker(awoken_lock_);

    if (result == MOJO_RESULT_ALREADY_EXISTS)
      result = MOJO_RESULT_OK;

    awoken_queue_.push_back(std::make_pair(context, result));
  }

  base::AutoLock locker(awakable_lock_);
  HandleSignalsState signals_state;
  signals_state.satisfiable_signals = MOJO_HANDLE_SIGNAL_READABLE;
  signals_state.satisfied_signals = MOJO_HANDLE_SIGNAL_READABLE;
  awakable_list_.AwakeForStateChange(signals_state);
}

}  // namespace edk
}  // namespace mojo
