// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/simple_dispatcher.h"

#include "base/logging.h"

namespace mojo {
namespace system {

SimpleDispatcher::SimpleDispatcher() {
}

SimpleDispatcher::~SimpleDispatcher() {
}

void SimpleDispatcher::HandleSignalsStateChangedNoLock() {
  lock().AssertAcquired();
  waiter_list_.AwakeWaitersForStateChange(GetHandleSignalsStateNoLock());
}

void SimpleDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  waiter_list_.CancelAllWaiters();
}

MojoResult SimpleDispatcher::AddWaiterImplNoLock(Waiter* waiter,
                                                 MojoHandleSignals signals,
                                                 uint32_t context) {
  lock().AssertAcquired();

  HandleSignalsState state(GetHandleSignalsStateNoLock());
  if (state.satisfies(signals))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!state.can_satisfy(signals))
    return MOJO_RESULT_FAILED_PRECONDITION;

  waiter_list_.AddWaiter(waiter, signals, context);
  return MOJO_RESULT_OK;
}

void SimpleDispatcher::RemoveWaiterImplNoLock(Waiter* waiter) {
  lock().AssertAcquired();
  waiter_list_.RemoveWaiter(waiter);
}

}  // namespace system
}  // namespace mojo
