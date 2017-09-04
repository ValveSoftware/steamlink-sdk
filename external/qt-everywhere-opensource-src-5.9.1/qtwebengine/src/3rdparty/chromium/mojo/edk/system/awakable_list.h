// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_AWAKABLE_LIST_H_
#define MOJO_EDK_SYSTEM_AWAKABLE_LIST_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/edk/system/watcher.h"
#include "mojo/edk/system/watcher_set.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {

class Awakable;
struct HandleSignalsState;

// |AwakableList| tracks all the |Waiter|s that are waiting on a given
// handle/|Dispatcher|. There should be a |AwakableList| for each handle that
// can be waited on (in any way). In the simple case, the |AwakableList| is
// owned by the |Dispatcher|, whereas in more complex cases it is owned by the
// secondary object (see simple_dispatcher.* and the explanatory comment in
// core.cc). This class is thread-unsafe (all concurrent access must be
// protected by some lock).
class MOJO_SYSTEM_IMPL_EXPORT AwakableList {
 public:
  AwakableList();
  ~AwakableList();

  void AwakeForStateChange(const HandleSignalsState& state);
  void CancelAll();
  void Add(Awakable* awakable, MojoHandleSignals signals, uintptr_t context);
  void Remove(Awakable* awakable);

  // Add and remove Watchers to this AwakableList.
  MojoResult AddWatcher(MojoHandleSignals signals,
                        const Watcher::WatchCallback& callback,
                        uintptr_t context,
                        const HandleSignalsState& current_state);
  MojoResult RemoveWatcher(uintptr_t context);

 private:
  struct AwakeInfo {
    AwakeInfo(Awakable* awakable, MojoHandleSignals signals, uintptr_t context)
        : awakable(awakable), signals(signals), context(context) {}

    Awakable* awakable;
    MojoHandleSignals signals;
    uintptr_t context;
  };
  using AwakeInfoList = std::vector<AwakeInfo>;

  AwakeInfoList awakables_;

  // TODO: Remove AwakableList and instead use WatcherSet directly in
  // dispatchers.
  WatcherSet watchers_;

  DISALLOW_COPY_AND_ASSIGN(AwakableList);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_AWAKABLE_LIST_H_
