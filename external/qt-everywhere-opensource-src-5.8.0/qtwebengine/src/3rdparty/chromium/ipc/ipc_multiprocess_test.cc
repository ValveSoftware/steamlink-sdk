// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "ipc/ipc_channel.h"
#include "ipc/ipc_multiprocess_test.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "ipc/ipc_descriptors.h"
#endif

namespace internal {

void MultiProcessTestIPCSetUp() {
#if defined(OS_ANDROID)
  // On Android we can't 'exec'. So for simple multi-process tests
  // we need to reset some global data after forking to get the same
  // behavior in simple multi-process tests.
  IPC::Channel::NotifyProcessForkedForTesting();
#endif
#if defined(OS_POSIX)
  base::GlobalDescriptors::GetInstance()->Set(kPrimaryIPCChannel,
      kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
#endif
}

}  // namespace internal
