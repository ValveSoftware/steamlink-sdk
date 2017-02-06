// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_H_
#define MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_H_

#include "build/build_config.h"
#include "mojo/edk/system/system_impl_export.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/process/process_handle.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach.h>
#endif

namespace mojo {
namespace edk {

#if defined(OS_POSIX)
struct MOJO_SYSTEM_IMPL_EXPORT PlatformHandle {
  PlatformHandle() {}
  explicit PlatformHandle(int handle) : handle(handle) {}
#if defined(OS_MACOSX) && !defined(OS_IOS)
  explicit PlatformHandle(mach_port_t port)
      : type(Type::MACH), port(port) {}
#endif

  void CloseIfNecessary();

  bool is_valid() const {
#if defined(OS_MACOSX) && !defined(OS_IOS)
    if (type == Type::MACH || type == Type::MACH_NAME)
      return port != MACH_PORT_NULL;
#endif
    return handle != -1;
  }

  enum class Type {
    POSIX,
#if defined(OS_MACOSX) && !defined(OS_IOS)
    MACH,
    // MACH_NAME isn't a real Mach port. But rather the "name" of one that can
    // be resolved to a real port later. This distinction is needed so that the
    // "port" doesn't try to be closed if CloseIfNecessary() is called. Having
    // this also allows us to do checks in other places.
    MACH_NAME,
#endif
  };
  Type type = Type::POSIX;

  int handle = -1;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  mach_port_t port = MACH_PORT_NULL;
#endif
};
#elif defined(OS_WIN)
struct MOJO_SYSTEM_IMPL_EXPORT PlatformHandle {
  PlatformHandle() : PlatformHandle(INVALID_HANDLE_VALUE) {}
  explicit PlatformHandle(HANDLE handle)
      : handle(handle), owning_process(base::GetCurrentProcessHandle()) {}

  void CloseIfNecessary();

  bool is_valid() const { return handle != INVALID_HANDLE_VALUE; }

  HANDLE handle;

  // A Windows HANDLE may be duplicated to another process but not yet sent to
  // that process. This tracks the handle's owning process.
  base::ProcessHandle owning_process;

  // A Windows HANDLE may be an unconnected named pipe. In this case, we need to
  // wait for a connection before communicating on the pipe.
  bool needs_connection = false;
};
#else
#error "Platform not yet supported."
#endif

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_H_
