// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_WATCHER_H_
#define MOJO_EDK_SYSTEM_WATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {

struct HandleSignalsState;

// This object corresponds to a watch added by a single call to |MojoWatch()|.
//
// An event may occur at any time which should trigger a Watcher to run its
// callback, but the callback needs to be deferred until all EDK locks are
// released. At the same time, a watch may be cancelled at any time by
// |MojoCancelWatch()| and it is not OK for the callback to be invoked after
// that happens.
//
// Therefore a Watcher needs to have some associated thread-safe state to track
// its cancellation, which is why it's ref-counted.
class Watcher : public base::RefCountedThreadSafe<Watcher> {
 public:
  using WatchCallback = base::Callback<void(MojoResult,
                                            const HandleSignalsState&,
                                            MojoWatchNotificationFlags)>;

  // Constructs a new Watcher which watches for |signals| to be satisfied on a
  // handle and which invokes |callback| either when one such signal is
  // satisfied, or all such signals become unsatisfiable.
  Watcher(MojoHandleSignals signals, const WatchCallback& callback);

  // Runs the Watcher's callback with the given arguments if it hasn't been
  // cancelled yet.
  void MaybeInvokeCallback(MojoResult result,
                           const HandleSignalsState& state,
                           MojoWatchNotificationFlags flags);

  // Notifies the Watcher of a state change. This may result in the Watcher
  // adding a finalizer to the current RequestContext to invoke its callback,
  // cancellation notwithstanding.
  void NotifyForStateChange(const HandleSignalsState& signals_state);

  // Notifies the Watcher of handle closure. This always results in the Watcher
  // adding a finalizer to the current RequestContext to invoke its callback,
  // cancellation notwithstanding.
  void NotifyClosed();

  // Explicitly cancels the watch, guaranteeing that its callback will never be
  // be invoked again.
  void Cancel();

 private:
  friend class base::RefCountedThreadSafe<Watcher>;

  ~Watcher();

  // The set of signals which are watched by this Watcher.
  const MojoHandleSignals signals_;

  // The callback to invoke with a result and signal state any time signals in
  // |signals_| are satisfied or become permanently unsatisfiable.
  const WatchCallback callback_;

  // Guards |is_cancelled_|.
  base::Lock lock_;

  // Indicates whether the watch has been cancelled. A |Watcher| may exist for a
  // brief period of time after being cancelled if it's been attached as a
  // RequestContext finalizer. In such cases the callback must not be invoked,
  // hence this flag.
  bool is_cancelled_ = false;

  DISALLOW_COPY_AND_ASSIGN(Watcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_WATCHER_H_
