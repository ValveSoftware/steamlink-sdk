// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_

#include "build/build_config.h"
#include "ipc/ipc_descriptors.h"

// This is a list of global descriptor keys to be used with the
// base::GlobalDescriptors object (see base/posix/global_descriptors.h)
enum {
  kCrashDumpSignal = kIPCDescriptorMax,
  kSandboxIPCChannel,  // https://chromium.googlesource.com/chromium/src/+/master/docs/linux_sandbox_ipc.md
  kMojoIPCChannel,

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  kV8NativesDataDescriptor,
#if defined(OS_ANDROID)
  kV8SnapshotDataDescriptor32,
  kV8SnapshotDataDescriptor64,
#else
  kV8SnapshotDataDescriptor,
#endif
#endif

#if defined(OS_ANDROID)
  kAndroidPropertyDescriptor,
  kAndroidICUDataDescriptor,
#endif

  // The first key that embedders can use to register descriptors (see
  // base/posix/global_descriptors.h).
  kContentIPCDescriptorMax
};

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTORS_H_
