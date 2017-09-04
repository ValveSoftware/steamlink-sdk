// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_COMMON_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_COMMON_H_

#include <fcntl.h>
#include <stddef.h>

namespace sandbox {

namespace syscall_broker {

const size_t kMaxMessageLength = 4096;

// Some flags are local to the current process and cannot be sent over a Unix
// socket. They need special treatment from the client.
// O_CLOEXEC is tricky because in theory another thread could call execve()
// before special treatment is made on the client, so a client needs to call
// recvmsg(2) with MSG_CMSG_CLOEXEC.
// To make things worse, there are two CLOEXEC related flags, FD_CLOEXEC (see
// F_GETFD in fcntl(2)) and O_CLOEXEC (see F_GETFL in fcntl(2)). O_CLOEXEC
// doesn't affect the semantics on execve(), it's merely a note that the
// descriptor was originally opened with O_CLOEXEC as a flag. And it is sent
// over unix sockets just fine, so a receiver that would (incorrectly) look at
// O_CLOEXEC instead of FD_CLOEXEC may be tricked in thinking that the file
// descriptor will or won't be closed on execve().
const int kCurrentProcessOpenFlagsMask = O_CLOEXEC;

enum IPCCommand {
  COMMAND_INVALID = 0,
  COMMAND_OPEN,
  COMMAND_ACCESS,
};

}  // namespace syscall_broker

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SYSCALL_BROKER_BROKER_COMMON_H_
