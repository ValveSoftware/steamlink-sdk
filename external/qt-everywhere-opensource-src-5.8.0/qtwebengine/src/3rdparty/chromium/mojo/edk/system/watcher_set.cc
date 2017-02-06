// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/watcher_set.h"

#include "mojo/edk/system/request_context.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {

WatcherSet::WatcherSet() {}

WatcherSet::~WatcherSet() {}

void WatcherSet::NotifyForStateChange(const HandleSignalsState& state) {
  for (const auto& entry : watchers_)
    entry.second->NotifyForStateChange(state);
}

void WatcherSet::NotifyClosed() {
  for (const auto& entry : watchers_)
    entry.second->NotifyClosed();
}

MojoResult WatcherSet::Add(MojoHandleSignals signals,
                           const Watcher::WatchCallback& callback,
                           uintptr_t context,
                           const HandleSignalsState& current_state) {
  auto it = watchers_.find(context);
  if (it != watchers_.end())
    return MOJO_RESULT_ALREADY_EXISTS;

  if (!current_state.can_satisfy(signals))
    return MOJO_RESULT_FAILED_PRECONDITION;

  scoped_refptr<Watcher> watcher(new Watcher(signals, callback));
  watchers_.insert(std::make_pair(context, watcher));

  watcher->NotifyForStateChange(current_state);

  return MOJO_RESULT_OK;
}

MojoResult WatcherSet::Remove(uintptr_t context) {
  auto it = watchers_.find(context);
  if (it == watchers_.end())
    return MOJO_RESULT_INVALID_ARGUMENT;

  RequestContext::current()->AddWatchCancelFinalizer(it->second);
  watchers_.erase(it);
  return MOJO_RESULT_OK;
}

}  // namespace edk
}  // namespace mojo
