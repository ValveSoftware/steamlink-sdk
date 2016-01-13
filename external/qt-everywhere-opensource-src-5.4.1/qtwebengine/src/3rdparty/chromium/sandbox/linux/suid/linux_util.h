// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following is duplicated from base/linux_utils.h.
// We shouldn't link against C++ code in a setuid binary.

#ifndef SANDBOX_LINUX_SUID_LINUX_UTIL_H_
#define SANDBOX_LINUX_SUID_LINUX_UTIL_H_

#include <stdbool.h>
#include <sys/types.h>

static const char kFindInodeSwitch[] = "--find-inode";
static const char kSuidSandboxGetApiSwitch[] = "--get-api";

// Find the process which holds the given socket, named by inode number. If
// multiple processes hold the socket, this function returns false.
bool FindProcessHoldingSocket(pid_t* pid_out, ino_t socket_inode);

#endif  // SANDBOX_LINUX_SUID_LINUX_UTIL_H_
