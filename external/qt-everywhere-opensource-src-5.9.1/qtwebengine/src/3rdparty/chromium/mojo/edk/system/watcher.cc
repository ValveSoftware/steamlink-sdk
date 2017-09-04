// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/watcher.h"

#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/request_context.h"

namespace mojo {
namespace edk {

Watcher::Watcher(MojoHandleSignals signals, const WatchCallback& callback)
    : signals_(signals), callback_(callback) {
}

void Watcher::MaybeInvokeCallback(MojoResult result,
                                  const HandleSignalsState& state,
                                  MojoWatchNotificationFlags flags) {
  base::AutoLock lock(lock_);
  if (is_cancelled_)
    return;

  callback_.Run(result, state, flags);
}

void Watcher::NotifyForStateChange(const HandleSignalsState& signals_state) {
  RequestContext* request_context = RequestContext::current();
  if (signals_state.satisfies(signals_)) {
    request_context->AddWatchNotifyFinalizer(
        make_scoped_refptr(this), MOJO_RESULT_OK, signals_state);
  } else if (!signals_state.can_satisfy(signals_)) {
    request_context->AddWatchNotifyFinalizer(
        make_scoped_refptr(this), MOJO_RESULT_FAILED_PRECONDITION,
        signals_state);
  }
}

void Watcher::NotifyClosed() {
  static const HandleSignalsState closed_state = {0, 0};
  RequestContext::current()->AddWatchNotifyFinalizer(
      make_scoped_refptr(this), MOJO_RESULT_CANCELLED, closed_state);
}

void Watcher::Cancel() {
  base::AutoLock lock(lock_);
  is_cancelled_ = true;
}

Watcher::~Watcher() {}

}  // namespace edk
}  // namespace mojo
