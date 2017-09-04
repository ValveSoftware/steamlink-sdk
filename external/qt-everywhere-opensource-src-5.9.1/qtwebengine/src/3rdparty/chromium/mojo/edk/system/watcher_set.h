// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_WATCHER_SET_H_
#define MOJO_EDK_SYSTEM_WATCHER_SET_H_

#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/watcher.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {

// A WatcherSet maintains a set of Watchers attached to a single handle and
// keyed on an arbitrary user context.
class WatcherSet {
 public:
  WatcherSet();
  ~WatcherSet();

  // Notifies all Watchers of a state change.
  void NotifyForStateChange(const HandleSignalsState& state);

  // Notifies all Watchers that their watched handle has been closed.
  void NotifyClosed();

  // Adds a new watcher to watch for signals in |signals| to be satisfied or
  // unsatisfiable. |current_state| is the current signals state of the
  // handle being watched.
  MojoResult Add(MojoHandleSignals signals,
                 const Watcher::WatchCallback& callback,
                 uintptr_t context,
                 const HandleSignalsState& current_state);

  // Removes a watcher from the set.
  MojoResult Remove(uintptr_t context);

 private:
  // A map of watchers keyed on context value.
  std::unordered_map<uintptr_t, scoped_refptr<Watcher>> watchers_;

  DISALLOW_COPY_AND_ASSIGN(WatcherSet);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_WATCHER_SET_H_
