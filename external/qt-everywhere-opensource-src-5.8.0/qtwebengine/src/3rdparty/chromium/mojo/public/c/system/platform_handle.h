// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/functions and constants for platform handle wrapping
// and unwrapping APIs.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_PLATFORM_HANDLE_H_
#define MOJO_PUBLIC_C_SYSTEM_PLATFORM_HANDLE_H_

#include <stdint.h>

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// |MojoPlatformHandleType|: A value indicating the specific type of platform
//     handle encapsulated by a MojoPlatformHandle (see below.) This is stored
//     in the MojoPlatformHandle's |type| field and determines how the |value|
//     field is interpreted.
//
//   |MOJO_PLATFORM_HANDLE_TYPE_INVALID| - An invalid platform handle.
//   |MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR| - A file descriptor. Only valid
//       on POSIX systems.
//   |MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT| - A Mach port. Only valid on OS X.
//   |MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE| - A Windows HANDLE value. Only
//       valid on Windows.

typedef uint32_t MojoPlatformHandleType;

#ifdef __cplusplus
const MojoPlatformHandleType MOJO_PLATFORM_HANDLE_TYPE_INVALID = 0;
const MojoPlatformHandleType MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR = 1;
const MojoPlatformHandleType MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT = 2;
const MojoPlatformHandleType MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE = 3;
#else
#define MOJO_PLATFORM_HANDLE_TYPE_INVALID ((MojoPlatformHandleType)0)
#define MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR ((MojoPlatformHandleType)1)
#define MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT ((MojoPlatformHandleType)2)
#define MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE ((MojoPlatformHandleType)3)
#endif

// |MojoPlatformHandle|: A handle to an OS object.
//     |uint32_t struct_size|: The size of this structure. Used for versioning
//         to allow for future extensions.
//     |MojoPlatformHandleType type|: The type of handle stored in |value|.
//     |uint64_t value|: The value of this handle. Ignored if |type| is
//         MOJO_PLATFORM_HANDLE_TYPE_INVALID.
//

struct MOJO_ALIGNAS(8) MojoPlatformHandle {
  uint32_t struct_size;
  MojoPlatformHandleType type;
  uint64_t value;
};
MOJO_STATIC_ASSERT(sizeof(MojoPlatformHandle) == 16,
                   "MojoPlatformHandle has wrong size");

// |MojoPlatformSharedBufferHandleFlags|: Flags relevant to wrapped platform
//     shared buffers.
//
//   |MOJO_PLATFORM_SHARED_BUFFER_HANDLE_NONE| - No flags.
//   |MOJO_PLATFORM_SHARED_BUFFER_HANDLE_READ_ONLY| - Indicates that the wrapped
//       buffer handle may only be mapped for reading.

typedef uint32_t MojoPlatformSharedBufferHandleFlags;

#ifdef __cplusplus
const MojoPlatformSharedBufferHandleFlags
MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_NONE = 0;

const MojoPlatformSharedBufferHandleFlags
MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_READ_ONLY = 1 << 0;
#else
#define MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_NONE \
    ((MojoPlatformSharedBufferHandleFlags)0)

#define MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_READ_ONLY \
    ((MojoPlatformSharedBufferHandleFlags)1 << 0)
#endif

// Wraps a generic platform handle as a Mojo handle which can be transferred
// over a message pipe. Takes ownership of the underlying platform object.
//
// |platform_handle|: The platform handle to wrap.
//
// Returns:
//     |MOJO_RESULT_OK| if the handle was successfully wrapped. In this case
//         |*mojo_handle| contains the Mojo handle of the wrapped object.
//     |MOJO_RESULT_RESOURCE_EXHAUSTED| if the system is out of handles.
//     |MOJO_RESULT_INVALID_ARGUMENT| if |platform_handle| was not a valid
//          platform handle.
//
// NOTE: It is not always possible to detect if |platform_handle| is valid,
// particularly when |platform_handle->type| is valid but
// |platform_handle->value| does not represent a valid platform object.
MOJO_SYSTEM_EXPORT MojoResult
MojoWrapPlatformHandle(const struct MojoPlatformHandle* platform_handle,
                       MojoHandle* mojo_handle);  // Out

// Unwraps a generic platform handle from a Mojo handle. If this call succeeds,
// ownership of the underlying platform object is bound to the returned platform
// handle and becomes the caller's responsibility. The Mojo handle is always
// closed regardless of success or failure.
//
// |mojo_handle|: The Mojo handle from which to unwrap the platform handle.
//
// Returns:
//     |MOJO_RESULT_OK| if the handle was successfully unwrapped. In this case
//         |*platform_handle| contains the unwrapped platform handle.
//     |MOJO_RESULT_INVALID_ARGUMENT| if |mojo_handle| was not a valid Mojo
//         handle wrapping a platform handle.
MOJO_SYSTEM_EXPORT MojoResult
MojoUnwrapPlatformHandle(MojoHandle mojo_handle,
                         struct MojoPlatformHandle* platform_handle);  // Out

// Wraps a platform shared buffer handle as a Mojo shared buffer handle which
// can be transferred over a message pipe. Takes ownership of the platform
// shared buffer handle.
//
// |platform_handle|: The platform handle to wrap. Must be a handle to a
//     shared buffer object.
// |num_bytes|: The size of the shared buffer in bytes.
// |flags|: Flags which influence the treatment of the shared buffer object. See
//     below.
//
// Flags:
//    |MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_NONE| indicates default behavior.
//        No flags set.
//    |MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_READ_ONLY| indicates that the
//        buffer handled to be wrapped may only be mapped as read-only. This
//        flag does NOT change the access control of the buffer in any way.
//
// Returns:
//     |MOJO_RESULT_OK| if the handle was successfully wrapped. In this case
//         |*mojo_handle| contains a Mojo shared buffer handle.
//     |MOJO_RESULT_INVALID_ARGUMENT| if |platform_handle| was not a valid
//         platform shared buffer handle.
MOJO_SYSTEM_EXPORT MojoResult
MojoWrapPlatformSharedBufferHandle(
    const struct MojoPlatformHandle* platform_handle,
    size_t num_bytes,
    MojoPlatformSharedBufferHandleFlags flags,
    MojoHandle* mojo_handle);  // Out

// Unwraps a platform shared buffer handle from a Mojo shared buffer handle.
// If this call succeeds, ownership of the underlying shared buffer object is
// bound to the returned platform handle and becomes the caller's
// responsibility. The Mojo handle is always closed regardless of success or
// failure.
//
// |mojo_handle|: The Mojo shared buffer handle to unwrap.
//
// |platform_handle|, |num_bytes| and |flags| are used to receive output values
// and MUST always be non-null.
//
// Returns:
//    |MOJO_RESULT_OK| if the handle was successfully unwrapped. In this case
//        |*platform_handle| contains a platform shared buffer handle,
//        |*num_bytes| contains the size of the shared buffer object, and
//        |*flags| indicates flags relevant to the wrapped buffer (see below).
//    |MOJO_RESULT_INVALID_ARGUMENT| if |mojo_handle| is not a valid Mojo
//        shared buffer handle.
//
// Flags which may be set in |*flags| upon success:
//    |MOJO_PLATFORM_SHARED_BUFFER_FLAG_READ_ONLY| is set iff the unwrapped
//        shared buffer handle may only be mapped as read-only.
MOJO_SYSTEM_EXPORT MojoResult
MojoUnwrapPlatformSharedBufferHandle(
    MojoHandle mojo_handle,
    struct MojoPlatformHandle* platform_handle,
    size_t* num_bytes,
    MojoPlatformSharedBufferHandleFlags* flags);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_PLATFORM_HANDLE_H_
