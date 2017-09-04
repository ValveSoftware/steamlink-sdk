// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_
#define CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_

#include "base/metrics/field_trial.h"
#include "content/public/common/content_client.h"

namespace gpu {
class SyncPointManager;
struct GpuPreferences;
}

namespace service_manager {
class InterfaceProvider;
class InterfaceRegistry;
}

namespace content {

// Embedder API for participating in gpu logic.
class CONTENT_EXPORT ContentGpuClient {
 public:
  virtual ~ContentGpuClient() {}

  // Initializes the client. This sets up the field trial synchronization
  // mechanism, which will notify |observer| when a field trial is activated,
  // which should be used to inform the browser process of this state.
  virtual void Initialize(base::FieldTrialList::Observer* observer) {}

  // Allows the client to expose interfaces from the GPU process to the browser
  // process via |registry|.
  virtual void ExposeInterfacesToBrowser(
      service_manager::InterfaceRegistry* registry,
      const gpu::GpuPreferences& gpu_preferences) {}

  // Allow the client to bind interfaces exposed by the browser process.
  virtual void ConsumeInterfacesFromBrowser(
      service_manager::InterfaceProvider* provider) {}

  // Allows client to supply a SyncPointManager instance instead of having
  // content internally create one.
  virtual gpu::SyncPointManager* GetSyncPointManager();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_GPU_CONTENT_GPU_CLIENT_H_
