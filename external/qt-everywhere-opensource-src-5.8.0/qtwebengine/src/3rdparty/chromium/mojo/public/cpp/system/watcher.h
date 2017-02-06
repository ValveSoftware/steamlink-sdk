// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_WATCHER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_WATCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {

// A Watcher watches a single Mojo handle for signal state changes.
//
// NOTE: Watchers may only be used on threads which have a running MessageLoop.
class Watcher {
 public:
  // A callback to be called any time a watched handle changes state in some
  // interesting way. The |result| argument indicates one of the following
  // conditions depending on its value:
  //
  //   |MOJO_RESULT_OK|: One or more of the signals being watched is satisfied.
  //
  //   |MOJO_RESULT_FAILED_PRECONDITION|: None of the signals being watched can
  //       ever be satisfied again.
  //
  //   |MOJO_RESULT_CANCELLED|: The handle has been closed and the watch has
  //       been cancelled implicitly.
  //
  //   |MOJO_RESULT_ABORTED|: Notifications can no longer be delivered for this
  //       watcher for some unspecified reason, e.g., the watching thread may
  //       be shutting down soon. Note that it is still necessary to explicitly
  //       Cancel() the watch in this case.
  using ReadyCallback = base::Callback<void(MojoResult result)>;

  explicit Watcher(scoped_refptr<base::SingleThreadTaskRunner> runner =
                       base::ThreadTaskRunnerHandle::Get());

  // NOTE: This destructor automatically calls |Cancel()| if the Watcher is
  // still active.
  ~Watcher();

  // Indicates if the Watcher is currently watching a handle.
  bool IsWatching() const;

  // Starts watching |handle|. A Watcher may only watch one handle at a time,
  // but it is safe to call this more than once as long as the previous watch
  // has been cancelled (i.e. |is_watching()| returns |false|.)
  //
  // If no signals in |signals| can ever be satisfied for |handle|, this returns
  // |MOJO_RESULT_FAILED_PRECONDITION|.
  //
  // If |handle| is not a valid watchable (message or data pipe) handle, this
  // returns |MOJO_RESULT_INVALID_ARGUMENT|.
  //
  // Otherwise |MOJO_RESULT_OK| is returned and the handle will be watched until
  // closure or cancellation.
  //
  // Once the watch is started, |callback| may be called at any time on the
  // current thread until |Cancel()| is called or the handle is closed.
  //
  // Destroying the Watcher implicitly calls |Cancel()|.
  MojoResult Start(Handle handle,
                   MojoHandleSignals signals,
                   const ReadyCallback& callback);

  // Cancels the current watch. Once this returns, the callback previously
  // passed to |Start()| will never be called again for this Watcher.
  void Cancel();

  Handle handle() const { return handle_; }
  ReadyCallback ready_callback() const { return callback_; }

 private:
  class MessageLoopObserver;
  friend class MessageLoopObserver;

  void OnHandleReady(MojoResult result);

  static void CallOnHandleReady(uintptr_t context,
                                MojoResult result,
                                MojoHandleSignalsState signals_state,
                                MojoWatchNotificationFlags flags);

  base::ThreadChecker thread_checker_;

  // The TaskRunner of this Watcher's owning thread. This field is safe to
  // access from any thread.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Whether |task_runner_| is the same as base::ThreadTaskRunnerHandle::Get()
  // for the thread.
  const bool is_default_task_runner_;

  std::unique_ptr<MessageLoopObserver> message_loop_observer_;

  // A persistent weak reference to this Watcher which can be passed to the
  // Dispatcher any time this object should be signalled. Safe to access (but
  // not to dereference!) from any thread.
  base::WeakPtr<Watcher> weak_self_;

  // Fields below must only be accessed on the Watcher's owning thread.

  // The handle currently under watch. Not owned.
  Handle handle_;

  // The callback to call when the handle is signaled.
  ReadyCallback callback_;

  base::WeakPtrFactory<Watcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Watcher);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_WATCHER_H_
