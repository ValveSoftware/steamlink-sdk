// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains basic functions common to different Mojo system APIs.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_
#define MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_

#include <stddef.h>
#include <stdint.h>

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// A callback used to notify watchers registered via |MojoWatch()|. Called when
// some watched signals are satisfied or become unsatisfiable. See the
// documentation for |MojoWatch()| for more details.
typedef void (*MojoWatchCallback)(uintptr_t context,
                                  MojoResult result,
                                  struct MojoHandleSignalsState signals_state,
                                  MojoWatchNotificationFlags flags);

// Note: Pointer parameters that are labelled "optional" may be null (at least
// under some circumstances). Non-const pointer parameters are also labeled
// "in", "out", or "in/out", to indicate how they are used. (Note that how/if
// such a parameter is used may depend on other parameters or the requested
// operation's success/failure. E.g., a separate |flags| parameter may control
// whether a given "in/out" parameter is used for input, output, or both.)

// Returns the time, in microseconds, since some undefined point in the past.
// The values are only meaningful relative to other values that were obtained
// from the same device without an intervening system restart. Such values are
// guaranteed to be monotonically non-decreasing with the passage of real time.
// Although the units are microseconds, the resolution of the clock may vary and
// is typically in the range of ~1-15 ms.
MOJO_SYSTEM_EXPORT MojoTimeTicks MojoGetTimeTicksNow(void);

// Closes the given |handle|.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle.
//
// Concurrent operations on |handle| may succeed (or fail as usual) if they
// happen before the close, be cancelled with result |MOJO_RESULT_CANCELLED| if
// they properly overlap (this is likely the case with |MojoWait()|, etc.), or
// fail with |MOJO_RESULT_INVALID_ARGUMENT| if they happen after.
MOJO_SYSTEM_EXPORT MojoResult MojoClose(MojoHandle handle);

// Waits on the given handle until one of the following happens:
//   - A signal indicated by |signals| is satisfied.
//   - It becomes known that no signal indicated by |signals| will ever be
//     satisfied. (See the description of the |MOJO_RESULT_CANCELLED| and
//     |MOJO_RESULT_FAILED_PRECONDITION| return values below.)
//   - Until |deadline| has passed.
//
// If |deadline| is |MOJO_DEADLINE_INDEFINITE|, this will wait "forever" (until
// one of the other wait termination conditions is satisfied). If |deadline| is
// 0, this will return |MOJO_RESULT_DEADLINE_EXCEEDED| only if one of the other
// termination conditions (e.g., a signal is satisfied, or all signals are
// unsatisfiable) is not already satisfied.
//
// |signals_state| (optional): See documentation for |MojoHandleSignalsState|.
//
// Returns:
//   |MOJO_RESULT_OK| if some signal in |signals| was satisfied (or is already
//       satisfied).
//   |MOJO_RESULT_CANCELLED| if |handle| was closed (necessarily from another
//       thread) during the wait.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle (e.g., if
//       it has already been closed). The |signals_state| value is unchanged.
//   |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//       the signals being satisfied.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it becomes known that none of the
//       signals in |signals| can ever be satisfied (e.g., when waiting on one
//       end of a message pipe and the other end is closed).
//
// If there are multiple waiters (on different threads, obviously) waiting on
// the same handle and signal, and that signal becomes satisfied, all waiters
// will be awoken.
MOJO_SYSTEM_EXPORT MojoResult
MojoWait(MojoHandle handle,
         MojoHandleSignals signals,
         MojoDeadline deadline,
         struct MojoHandleSignalsState* signals_state);  // Optional out.

// Waits on |handles[0]|, ..., |handles[num_handles-1]| until:
//   - (At least) one handle satisfies a signal indicated in its respective
//     |signals[0]|, ..., |signals[num_handles-1]|.
//   - It becomes known that no signal in some |signals[i]| will ever be
//     satisfied.
//   - |deadline| has passed.
//
// This means that |MojoWaitMany()| behaves as if |MojoWait()| were called on
// each handle/signals pair simultaneously, completing when the first
// |MojoWait()| would complete.
//
// See |MojoWait()| for more details about |deadline|.
//
// |result_index| (optional) is used to return the index of the handle that
//     caused the call to return. For example, the index |i| (from 0 to
//     |num_handles-1|) if |handle[i]| satisfies a signal from |signals[i]|. You
//     must manually initialize this to a suitable sentinel value (e.g. -1)
//     before you make this call because this value is not updated if there is
//     no specific handle that causes the function to return. Pass null if you
//     don't need this value to be returned.
//
// |signals_states| (optional) points to an array of size |num_handles| of
//     MojoHandleSignalsState. See |MojoHandleSignalsState| for more details
//     about the meaning of each array entry. This array is not an atomic
//     snapshot. The array will be updated if the function does not return
//     |MOJO_RESULT_INVALID_ARGUMENT| or |MOJO_RESULT_RESOURCE_EXHAUSTED|.
//
// Returns:
//   |MOJO_RESULT_CANCELLED| if some |handle[i]| was closed (necessarily from
//       another thread) during the wait.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if there are too many handles. The
//       |signals_state| array is unchanged.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some |handle[i]| is not a valid handle
//       (e.g., if it is zero or if it has already been closed). The
//       |signals_state| array is unchanged.
//   |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//       handles satisfying any of its signals.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it is or becomes impossible that SOME
//       |handle[i]| will ever satisfy any of the signals in |signals[i]|.
MOJO_SYSTEM_EXPORT MojoResult
MojoWaitMany(const MojoHandle* handles,
             const MojoHandleSignals* signals,
             uint32_t num_handles,
             MojoDeadline deadline,
             uint32_t* result_index,                          // Optional out
             struct MojoHandleSignalsState* signals_states);  // Optional out

// Watches the given handle for one of the following events to happen:
//   - A signal indicated by |signals| is satisfied.
//   - It becomes known that no signal indicated by |signals| will ever be
//     satisfied. (See the description of the |MOJO_RESULT_CANCELLED| and
//     |MOJO_RESULT_FAILED_PRECONDITION| return values below.)
//   - The handle is closed.
//
// |handle|: The handle to watch. Must be an open message pipe or data pipe
//     handle.
// |signals|: The signals to watch for.
// |callback|: A function to be called any time one of the above events happens.
//     The function must be safe to call from any thread at any time.
// |context|: User-provided context passed to |callback| when called. |context|
//     is used to uniquely identify a registered watch and can be used to cancel
//     the watch later using |MojoCancelWatch()|.
//
// Returns:
//   |MOJO_RESULT_OK| if the watch has been successfully registered. Note that
//       if the signals are already satisfied this may synchronously invoke
//       |callback| before returning.
//   |MOJO_RESULT_CANCELLED| if the watch was cancelled. In this case it is not
//       necessary to explicitly call |MojoCancelWatch()|, and in fact it may be
//       an error to do so as the handle may have been closed.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not an open message pipe
//       handle.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it is already known that |signals| can
//       never be satisfied.
//   |MOJO_RESULT_ALREADY_EXISTS| if there is already a watch registered for
//       the same combination of |handle| and |context|.
//
// Callback result codes:
//   The callback may be called at any time on any thread with one of the
//   following result codes to indicate various events:
//
//   |MOJO_RESULT_OK| indicates that some signal in |signals| has been
//       satisfied.
//   |MOJO_RESULT_FAILED_PRECONDITION| indicates that no signals in |signals|
//       can ever be satisfied again.
//   |MOJO_RESULT_CANCELLED| indicates that the handle has been closed. In this
//       case the watch is implicitly cancelled and there is no need to call
//       |MojoCancelWatch()|.
MOJO_SYSTEM_EXPORT MojoResult
MojoWatch(MojoHandle handle,
          MojoHandleSignals signals,
          MojoWatchCallback callback,
          uintptr_t context);

// Cancels a handle watch corresponding to some prior call to |MojoWatch()|.
//
// NOTE: If the watch callback corresponding to |context| is currently running
// this will block until the callback completes execution. It is therefore
// illegal to call |MojoCancelWatch()| on a given |handle| and |context| from
// within the associated callback itself, as this will always deadlock.
//
// After |MojoCancelWatch()| function returns, the watch's associated callback
// will NEVER be called again by Mojo.
//
// |context|: The same user-provided context given to some prior call to
//     |MojoWatch()|. Only the watch corresponding to this context will be
//     cancelled.
//
// Returns:
//     |MOJO_RESULT_OK| if the watch corresponding to |context| was cancelled.
//     |MOJO_RESULT_INVALID_ARGUMENT| if no watch was registered with |context|
//         for the given |handle|, or if |handle| is invalid.
MOJO_SYSTEM_EXPORT MojoResult
MojoCancelWatch(MojoHandle handle, uintptr_t context);

// Retrieves system properties. See the documentation for |MojoPropertyType| for
// supported property types and their corresponding output value type.
//
// Returns:
//     |MOJO_RESULT_OK| on success.
//     |MOJO_RESULT_INVALID_ARGUMENT| if |type| is not recognized. In this case,
//         |value| is untouched.
MOJO_SYSTEM_EXPORT MojoResult MojoGetProperty(MojoPropertyType type,
                                              void* value);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_
