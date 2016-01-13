// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_WAITER_H_
#define MOJO_SYSTEM_WAITER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "mojo/public/c/system/types.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// IMPORTANT (all-caps gets your attention, right?): |Waiter| methods are called
// under other locks, in particular, |Dispatcher::lock_|s, so |Waiter| methods
// must never call out to other objects (in particular, |Dispatcher|s). This
// class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT Waiter {
 public:
  Waiter();
  ~Waiter();

  // A |Waiter| can be used multiple times; |Init()| should be called before
  // each time it's used.
  void Init();

  // Waits until a suitable |Awake()| is called. (|context| may be null, in
  // which case, obviously no context is ever returned.)
  // Returns:
  //   - The result given to the first call to |Awake()| (possibly before this
  //     call to |Wait()|); in this case, |*context| is set to the value passed
  //     to that call to |Awake()|.
  //   - |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline was exceeded; in this
  //     case |*context| is not modified.
  //
  // Usually, the context passed to |Awake()| will be the value passed to
  // |Dispatcher::AddWaiter()|, which is usually the index to the array of
  // handles passed to |MojoWaitMany()| (or 0 for |MojoWait()|).
  //
  // Typical |Awake()| results are:
  //   - |MOJO_RESULT_OK| if one of the flags passed to
  //     |MojoWait()|/|MojoWaitMany()| (hence |Dispatcher::AddWaiter()|) was
  //     satisfied;
  //   - |MOJO_RESULT_CANCELLED| if a handle (on which
  //     |MojoWait()|/|MojoWaitMany()| was called) was closed (hence the
  //     dispatcher closed); and
  //   - |MOJO_RESULT_FAILED_PRECONDITION| if one of the set of flags passed to
  //     |MojoWait()|/|MojoWaitMany()| cannot or can no longer be satisfied by
  //     the corresponding handle (e.g., if the other end of a message or data
  //     pipe is closed).
  MojoResult Wait(MojoDeadline deadline, uint32_t* context);

  // Wake the waiter up with the given result and context (or no-op if it's been
  // woken up already).
  void Awake(MojoResult result, uint32_t context);

 private:
  base::ConditionVariable cv_;  // Associated to |lock_|.
  base::Lock lock_;  // Protects the following members.
#ifndef NDEBUG
  bool initialized_;
#endif
  bool awoken_;
  MojoResult awake_result_;
  // This is a |uint32_t| because we really only need to store an index (for
  // |MojoWaitMany()|). But in tests, it's convenient to use this for other
  // purposes (e.g., to distinguish between different wake-up reasons).
  uint32_t awake_context_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_WAITER_H_
