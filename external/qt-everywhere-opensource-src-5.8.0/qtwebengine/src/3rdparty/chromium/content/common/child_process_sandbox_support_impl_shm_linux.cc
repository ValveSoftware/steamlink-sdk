// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/child_process_sandbox_support_impl_linux.h"

#include <stddef.h>
#include <stdint.h>

#include "base/pickle.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"

namespace content {

int MakeSharedMemorySegmentViaIPC(size_t length, bool executable) {
  base::Pickle request;
  request.WriteInt(LinuxSandbox::METHOD_MAKE_SHARED_MEMORY_SEGMENT);
  request.WriteUInt32(length);
  request.WriteBool(executable);
  uint8_t reply_buf[10];
  int result_fd;
  ssize_t result = base::UnixDomainSocket::SendRecvMsg(
      GetSandboxFD(), reply_buf, sizeof(reply_buf), &result_fd, request);
  if (result == -1)
    return -1;
  return result_fd;
}

}  // namespace content
