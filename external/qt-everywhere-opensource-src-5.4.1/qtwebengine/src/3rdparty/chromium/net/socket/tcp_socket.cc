// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_socket.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/worker_pool.h"

namespace net {

namespace {

bool g_tcp_fastopen_enabled = false;

#if defined(OS_LINUX) || defined(OS_ANDROID)

typedef base::RefCountedData<bool> SharedBoolean;

// Checks to see if the system supports TCP FastOpen. Notably, it requires
// kernel support. Additionally, this checks system configuration to ensure that
// it's enabled.
void SystemSupportsTCPFastOpen(scoped_refptr<SharedBoolean> supported) {
  supported->data = false;
  static const base::FilePath::CharType kTCPFastOpenProcFilePath[] =
      "/proc/sys/net/ipv4/tcp_fastopen";
  std::string system_enabled_tcp_fastopen;
  if (!base::ReadFileToString(base::FilePath(kTCPFastOpenProcFilePath),
                              &system_enabled_tcp_fastopen)) {
    return;
  }

  // As per http://lxr.linux.no/linux+v3.7.7/include/net/tcp.h#L225
  // TFO_CLIENT_ENABLE is the LSB
  if (system_enabled_tcp_fastopen.empty() ||
      (system_enabled_tcp_fastopen[0] & 0x1) == 0) {
    return;
  }

  supported->data = true;
}

void EnableCallback(scoped_refptr<SharedBoolean> supported) {
  g_tcp_fastopen_enabled = supported->data;
}

// This is asynchronous because it needs to do file IO, and it isn't allowed to
// do that on the IO thread.
void EnableFastOpenIfSupported() {
  scoped_refptr<SharedBoolean> supported = new SharedBoolean;
  base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(SystemSupportsTCPFastOpen, supported),
      base::Bind(EnableCallback, supported),
      false);
}

#else

void EnableFastOpenIfSupported() {
  g_tcp_fastopen_enabled = false;
}

#endif

}  // namespace

void SetTCPFastOpenEnabled(bool value) {
  if (value) {
    EnableFastOpenIfSupported();
  } else {
    g_tcp_fastopen_enabled = false;
  }
}

bool IsTCPFastOpenEnabled() {
  return g_tcp_fastopen_enabled;
}

}  // namespace net
