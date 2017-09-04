// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

namespace {

uint64_t PlatformHandleValueFromPlatformFile(base::PlatformFile file) {
#if defined(OS_WIN)
  return reinterpret_cast<uint64_t>(file);
#else
  return static_cast<uint64_t>(file);
#endif
}

base::PlatformFile PlatformFileFromPlatformHandleValue(uint64_t value) {
#if defined(OS_WIN)
  return reinterpret_cast<base::PlatformFile>(value);
#else
  return static_cast<base::PlatformFile>(value);
#endif
}

}  // namespace

ScopedHandle WrapPlatformFile(base::PlatformFile platform_file) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  platform_handle.type = kPlatformFileHandleType;
  platform_handle.value = PlatformHandleValueFromPlatformFile(platform_file);

  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformHandle(&platform_handle, &mojo_handle);
  CHECK_EQ(result, MOJO_RESULT_OK);

  return ScopedHandle(Handle(mojo_handle));
}

MojoResult UnwrapPlatformFile(ScopedHandle handle, base::PlatformFile* file) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  MojoResult result = MojoUnwrapPlatformHandle(handle.release().value(),
                                               &platform_handle);
  if (result != MOJO_RESULT_OK)
    return result;

  if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_INVALID) {
    *file = base::kInvalidPlatformFile;
  } else {
    CHECK_EQ(platform_handle.type, kPlatformFileHandleType);
    *file = PlatformFileFromPlatformHandleValue(platform_handle.value);
  }

  return MOJO_RESULT_OK;
}

ScopedSharedBufferHandle WrapSharedMemoryHandle(
    const base::SharedMemoryHandle& memory_handle,
    size_t size,
    bool read_only) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  platform_handle.type = kPlatformSharedBufferHandleType;
#if defined(OS_MACOSX) && !defined(OS_IOS)
  platform_handle.value =
      static_cast<uint64_t>(memory_handle.GetMemoryObject());
#elif defined(OS_POSIX)
  platform_handle.value = PlatformHandleValueFromPlatformFile(memory_handle.fd);
#elif defined(OS_WIN)
  platform_handle.value =
      PlatformHandleValueFromPlatformFile(memory_handle.GetHandle());
#endif

  MojoPlatformSharedBufferHandleFlags flags =
      MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_NONE;
  if (read_only)
    flags |= MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_READ_ONLY;

  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformSharedBufferHandle(
      &platform_handle, size, flags, &mojo_handle);
  CHECK_EQ(result, MOJO_RESULT_OK);

  return ScopedSharedBufferHandle(SharedBufferHandle(mojo_handle));
}

MojoResult UnwrapSharedMemoryHandle(ScopedSharedBufferHandle handle,
                                    base::SharedMemoryHandle* memory_handle,
                                    size_t* size,
                                    bool* read_only) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);

  MojoPlatformSharedBufferHandleFlags flags;
  size_t num_bytes;
  MojoResult result = MojoUnwrapPlatformSharedBufferHandle(
      handle.release().value(), &platform_handle, &num_bytes, &flags);
  if (result != MOJO_RESULT_OK)
    return result;

  if (size)
    *size = num_bytes;

  if (read_only)
    *read_only = flags & MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_READ_ONLY;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  CHECK_EQ(platform_handle.type, MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT);
  *memory_handle = base::SharedMemoryHandle(
      static_cast<mach_port_t>(platform_handle.value), num_bytes,
      base::GetCurrentProcId());
#elif defined(OS_POSIX)
  CHECK_EQ(platform_handle.type, MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR);
  *memory_handle = base::SharedMemoryHandle(
      static_cast<int>(platform_handle.value), false);
#elif defined(OS_WIN)
  CHECK_EQ(platform_handle.type, MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE);
  *memory_handle = base::SharedMemoryHandle(
      reinterpret_cast<HANDLE>(platform_handle.value),
      base::GetCurrentProcId());
#endif

  return MOJO_RESULT_OK;
}

}  // namespace mojo
