// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/scoped_input_device.h"

#include <errno.h>
#include <unistd.h>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace ui {
namespace internal {

// static
void ScopedInputDeviceCloseTraits::Free(int fd) {
  // It's important to crash here.
  // There are security implications to not closing a file descriptor
  // properly. As file descriptors are "capabilities", keeping them open
  // would make the current process keep access to a resource. Much of
  // Chrome relies on being able to "drop" such access.
  // It's especially problematic on Linux with the setuid sandbox, where
  // a single open directory would bypass the entire security model.
  int ret = IGNORE_EINTR(close(fd));
  PCHECK(0 == ret || errno != EBADF);
}

}  // namespace internal
}  // namespace ui
