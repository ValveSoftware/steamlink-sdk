// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/constants and functions specific to wait sets.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_WAIT_SET_H_
#define MOJO_PUBLIC_C_SYSTEM_WAIT_SET_H_

#include <stdint.h>

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Note: See the comment in functions.h about the meaning of the "optional"
// label for pointer parameters.

// Creates a wait set. A wait set is a way to efficiently wait on multiple
// handles.
//
// On success, |*wait_set_handle| will contain a handle to a wait set.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |wait_set_handle| is null.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateWaitSet(
    MojoHandle* wait_set_handle);  // Out.

// Adds a wait on |handle| to |wait_set_handle|.
//
// A handle can only be added to any given wait set once, but may be added to
// any number of different wait sets. To modify the signals being waited for,
// the handle must first be removed, and then added with the new signals.
//
// If a handle is closed while still in the wait set, it is implicitly removed
// from the set after being returned from |MojoGetReadyHandles()| with the
// result |MOJO_RESULT_CANCELLED|.
//
// It is safe to add a handle to a wait set while performing a wait on another
// thread. If the added handle already has its signals satisfied, the waiting
// thread will be woken.
//
// Returns:
//   |MOJO_RESULT_OK| if |handle| was successfully added to |wait_set_handle|.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle, or
//       |wait_set_handle| is not a valid wait set.
//   |MOJO_RESULT_ALREADY_EXISTS| if |handle| already exists in
//       |wait_set_handle|.
MOJO_SYSTEM_EXPORT MojoResult MojoAddHandle(
    MojoHandle wait_set_handle,
    MojoHandle handle,
    MojoHandleSignals signals);

// Removes |handle| from |wait_set_handle|.
//
// It is safe to remove a handle from a wait set while performing a wait on
// another thread. If handle has its signals satisfied while it is being
// removed, the waiting thread may be woken up, but no handle may be available
// when |MojoGetReadyHandles()| is called.
//
// Returns:
//   |MOJO_RESULT_OK| if |handle| was successfully removed from
//       |wait_set_handle|.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle, or
//       |wait_set_handle| is not a valid wait set.
//   |MOJO_RESULT_NOT_FOUND| if |handle| does not exist in |wait_set_handle|.
MOJO_SYSTEM_EXPORT MojoResult MojoRemoveHandle(
    MojoHandle wait_set_handle,
    MojoHandle handle);

// Retrieves a set of ready handles from |wait_set_handle|. A handle is ready if
// at least of of the following is true:
//   - The handle's signals are satisfied.
//   - It becomes known that no signal for the handle will ever be satisfied.
//   - The handle is closed.
//
// A wait set may have ready handles when it satisfies the
// |MOJO_HANDLE_SIGNAL_READABLE| signal. Since handles may be added and removed
// from a wait set concurrently, it is possible for a wait set to satisfy
// |MOJO_HANDLE_SIGNAL_READABLE|, but not have any ready handles when
// |MojoGetReadyHandles()| is called. These spurious wake-ups must be gracefully
// handled.
//
// |*count| on input, must contain the maximum number of ready handles to be
// returned. On output, it will contain the number of ready handles returned.
//
// |handles| must point to an array of size |*count| of |MojoHandle|. It will be
// populated with handles that are considered ready. The number of handles
// returned will be in |*count|.
//
// |results| must point to an array of size |*count| of |MojoResult|. It will be
// populated with the wait result of the corresponding handle in |*handles|.
// Care should be taken that if a handle is closed on another thread, the handle
// would be invalid, but the result may not be |MOJO_RESULT_CANCELLED|. See
// documentation for |MojoWait()| for possible results.
//
// |signals_state| (optional) if non-null, must point to an array of size
// |*count| of |MojoHandleSignalsState|. It will be populated with the signals
// state of the corresponding handle in |*handles|. See documentation for
// |MojoHandleSignalsState| for more details about the meaning of each array
// entry. The array will always be updated for every returned handle.
//
// Mojo signals and satisfiability are logically 'level-triggered'. Therefore,
// if a signal continues to be satisfied and is not removed from the wait set,
// subsequent calls to |MojoGetReadyHandles()| will return the same handle.
//
// If multiple handles have their signals satisfied, the order in which handles
// are returned is undefined. The same handle, if not removed, may be returned
// in consecutive calls. Callers must not rely on any fairness and handles
// could be starved if not acted on.
//
// Returns:
//   |MOJO_RESULT_OK| if ready handles are available.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |wait_set_handle| is not a valid wait
//       set, if |*count| is 0, or if either |count|, |handles|, or |results| is
//       null.
//   |MOJO_RESULT_SHOULD_WAIT| if there are no ready handles.
MOJO_SYSTEM_EXPORT MojoResult MojoGetReadyHandles(
    MojoHandle wait_set_handle,
    uint32_t* count,                                 // In/out.
    MojoHandle* handles,                             // Out.
    MojoResult* results,                             // Out.
    struct MojoHandleSignalsState *signals_states);  // Optional out.

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_WAIT_SET_H_
