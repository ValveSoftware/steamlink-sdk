// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_

#include "ipc/ipc_descriptors.h"

// This is a list of global descriptor keys to be used with the
// base::GlobalDescriptors object (see base/posix/global_descriptors.h)
enum {
  kCrashDumpSignal = kIPCDescriptorMax,
  kSandboxIPCChannel,  // http://code.google.com/p/chromium/LinuxSandboxIPC

#if defined(OS_ANDROID)
  kAndroidPropertyDescriptor,
  kAndroidICUDataDescriptor,
#endif

  // The first key that embedders can use to register descriptors (see
  // base/posix/global_descriptors.h).
  kContentIPCDescriptorMax
};

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_
