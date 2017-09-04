// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_CONTEXT_PROVIDER_FACTORY_H_
#define UI_ANDROID_CONTEXT_PROVIDER_FACTORY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cc/surfaces/frame_sink_id.h"
#include "ui/android/ui_android_export.h"

namespace cc {
class ContextProvider;
class VulkanContextProvider;
class SurfaceManager;
}

namespace gpu {
namespace gles2 {
struct ContextCreationAttribHelper;
}  // namespace gles

struct SharedMemoryLimits;
class GpuChannelHost;
class GpuMemoryBufferManager;
}  // namespace gpu

namespace ui {

// This class is not thread-safe and should only be accessed from the UI thread.
class UI_ANDROID_EXPORT ContextProviderFactory {
 public:
  enum class GpuChannelHostResult {
    FAILURE_GPU_PROCESS_INITIALIZATION_FAILED,

    // Used when the factory is shutting down. No more requests should be made
    // to the factory after this response is dispatched.
    FAILURE_FACTORY_SHUTDOWN,

    // Set if the Context creation was successful.
    SUCCESS,
  };

  using GpuChannelHostCallback =
      base::Callback<void(scoped_refptr<gpu::GpuChannelHost>,
                          GpuChannelHostResult)>;

  enum class ContextType {
    BLIMP_RENDER_COMPOSITOR_CONTEXT,
    BLIMP_RENDER_WORKER_CONTEXT,
  };

  static ContextProviderFactory* GetInstance();

  // This should only be called once, on startup. Ownership remains with the
  // caller.
  static void SetInstance(ContextProviderFactory* context_provider_factory);

  virtual ~ContextProviderFactory(){};

  virtual scoped_refptr<cc::VulkanContextProvider>
  GetSharedVulkanContextProvider() = 0;

  // The callback may be triggered synchronously if possible. If the creation
  // fails, a null host is passed with the specified reason.
  virtual void RequestGpuChannelHost(GpuChannelHostCallback callback) = 0;

  // Creates an offscreen ContextProvider for the compositor. Any shared
  // contexts passed here *must* have been created using this factory.
  virtual scoped_refptr<cc::ContextProvider> CreateOffscreenContextProvider(
      ContextType context_type,
      gpu::SharedMemoryLimits shared_memory_limits,
      gpu::gles2::ContextCreationAttribHelper attributes,
      bool support_locking,
      bool automatic_flushes,
      cc::ContextProvider* shared_context_provider,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) = 0;

  virtual cc::SurfaceManager* GetSurfaceManager() = 0;

  virtual cc::FrameSinkId AllocateFrameSinkId() = 0;

  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
};

}  // namespace ui

#endif  // UI_ANDROID_CONTEXT_PROVIDER_FACTORY_H_
