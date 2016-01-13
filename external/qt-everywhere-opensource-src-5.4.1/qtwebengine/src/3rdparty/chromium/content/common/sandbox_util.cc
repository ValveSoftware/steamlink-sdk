// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_util.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "content/public/common/sandbox_init.h"

namespace content {

IPC::PlatformFileForTransit BrokerGetFileHandleForProcess(
    base::PlatformFile handle,
    base::ProcessId target_process_id,
    bool should_close_source) {
  IPC::PlatformFileForTransit out_handle;
#if defined(OS_WIN)
  DWORD options = DUPLICATE_SAME_ACCESS;
  if (should_close_source)
    options |= DUPLICATE_CLOSE_SOURCE;
  if (!content::BrokerDuplicateHandle(handle, target_process_id, &out_handle,
                                      0, options)) {
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
  int fd = should_close_source ? handle : ::dup(handle);
  out_handle = base::FileDescriptor(fd, true);
#else
  #error Not implemented.
#endif
  return out_handle;
}

}  // namespace content

