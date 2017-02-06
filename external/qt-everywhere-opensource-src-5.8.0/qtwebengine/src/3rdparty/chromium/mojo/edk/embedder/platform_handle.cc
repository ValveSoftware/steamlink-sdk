// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_handle.h"

#include "build/build_config.h"
#if defined(OS_POSIX)
#include <unistd.h>
#elif defined(OS_WIN)
#include <windows.h>
#else
#error "Platform not yet supported."
#endif

#include "base/logging.h"

namespace mojo {
namespace edk {

void PlatformHandle::CloseIfNecessary() {
  if (!is_valid())
    return;

#if defined(OS_POSIX)
  if (type == Type::POSIX) {
    bool success = (close(handle) == 0);
    DPCHECK(success);
    handle = -1;
  }
#if defined(OS_MACOSX) && !defined(OS_IOS)
  else if (type == Type::MACH) {
    kern_return_t rv = mach_port_deallocate(mach_task_self(), port);
    DPCHECK(rv == KERN_SUCCESS);
    port = MACH_PORT_NULL;
  }
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
#elif defined(OS_WIN)
  if (owning_process != base::GetCurrentProcessHandle()) {
    // This handle may have been duplicated to a new target process but not yet
    // sent there. In this case CloseHandle should NOT be called. From MSDN
    // documentation for DuplicateHandle[1]:
    //
    //    Normally the target process closes a duplicated handle when that
    //    process is finished using the handle. To close a duplicated handle
    //    from the source process, call DuplicateHandle with the following
    //    parameters:
    //
    //    * Set hSourceProcessHandle to the target process from the
    //      call that created the handle.
    //    * Set hSourceHandle to the duplicated handle to close.
    //    * Set lpTargetHandle to NULL.
    //    * Set dwOptions to DUPLICATE_CLOSE_SOURCE.
    //
    // [1] https://msdn.microsoft.com/en-us/library/windows/desktop/ms724251
    //
    // NOTE: It's possible for this operation to fail if the owning process
    // was terminated or is in the process of being terminated. Either way,
    // there is nothing we can reasonably do about failure, so we ignore it.
    DuplicateHandle(owning_process, handle, NULL, &handle, 0, FALSE,
                    DUPLICATE_CLOSE_SOURCE);
    return;
  }

  bool success = !!CloseHandle(handle);
  DPCHECK(success);
  handle = INVALID_HANDLE_VALUE;
#else
#error "Platform not yet supported."
#endif
}

}  // namespace edk
}  // namespace mojo
