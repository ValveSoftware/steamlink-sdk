// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a C++ wrapping around the Mojo C API for platform handles,
// replacing the prefix of "Mojo" with a "mojo" namespace.
//
// Please see "mojo/public/c/system/platform_handle.h" for complete
// documentation of the API.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_PLATFORM_HANDLE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_PLATFORM_HANDLE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory_handle.h"
#include "base/process/process_handle.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace mojo {

#if defined(OS_POSIX)
const MojoPlatformHandleType kPlatformFileHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;

#if defined(OS_MACOSX) && !defined(OS_IOS)
const MojoPlatformHandleType kPlatformSharedBufferHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT;
#else
const MojoPlatformHandleType kPlatformSharedBufferHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

#elif defined(OS_WIN)
const MojoPlatformHandleType kPlatformFileHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;

const MojoPlatformHandleType kPlatformSharedBufferHandleType =
    MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;
#endif  // defined(OS_POSIX)

// Wraps a PlatformFile as a Mojo handle. Takes ownership of the file object.
ScopedHandle WrapPlatformFile(base::PlatformFile platform_file);

// Unwraps a PlatformFile from a Mojo handle.
MojoResult UnwrapPlatformFile(ScopedHandle handle, base::PlatformFile* file);

// Wraps a base::SharedMemoryHandle as a Mojo handle. Takes ownership of the
// SharedMemoryHandle. Note that |read_only| is only an indicator of whether
// |memory_handle| only supports read-only mapping. It does NOT have any
// influence on the access control of the shared buffer object.
ScopedSharedBufferHandle WrapSharedMemoryHandle(
    const base::SharedMemoryHandle& memory_handle,
    size_t size,
    bool read_only);

// Unwraps a base::SharedMemoryHandle from a Mojo handle. The caller assumes
// responsibility for the lifetime of the SharedMemoryHandle.
MojoResult UnwrapSharedMemoryHandle(ScopedSharedBufferHandle handle,
                                    base::SharedMemoryHandle* memory_handle,
                                    size_t* size,
                                    bool* read_only);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_PLATFORM_HANDLE_H_
