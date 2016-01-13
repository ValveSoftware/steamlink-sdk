// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_platform_file.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

namespace IPC {

PlatformFileForTransit GetFileHandleForProcess(base::PlatformFile handle,
                                               base::ProcessHandle process,
                                               bool close_source_handle) {
  IPC::PlatformFileForTransit out_handle;
#if defined(OS_WIN)
  DWORD options = DUPLICATE_SAME_ACCESS;
  if (close_source_handle)
    options |= DUPLICATE_CLOSE_SOURCE;
  if (handle == INVALID_HANDLE_VALUE ||
      !::DuplicateHandle(::GetCurrentProcess(),
                         handle,
                         process,
                         &out_handle,
                         0,
                         FALSE,
                         options)) {
    out_handle = IPC::InvalidPlatformFileForTransit();
  }
#elif defined(OS_POSIX)
  // If asked to close the source, we can simply re-use the source fd instead of
  // dup()ing and close()ing.
  // When we're not closing the source, we need to duplicate the handle and take
  // ownership of that. The reason is that this function is often used to
  // generate IPC messages, and the handle must remain valid until it's sent to
  // the other process from the I/O thread. Without the dup, calling code might
  // close the source handle before the message is sent, creating a race
  // condition.
  int fd = close_source_handle ? handle : ::dup(handle);
  out_handle = base::FileDescriptor(fd, true);
#else
  #error Not implemented.
#endif
  return out_handle;
}

PlatformFileForTransit TakeFileHandleForProcess(base::File file,
                                                base::ProcessHandle process) {
  return GetFileHandleForProcess(file.TakePlatformFile(), process, true);
}

}  // namespace IPC
