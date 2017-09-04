// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CONTEXT_PROVIDER_FACTORY_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_CONTEXT_PROVIDER_FACTORY_IMPL_ANDROID_H_

#include <queue>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/command_buffer_metrics.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/android/context_provider_factory.h"

namespace gpu {
class GpuChannelHost;
class GpuChannelEstablishFactory;
}

namespace content {
class ContextProviderCommandBuffer;

class CONTENT_EXPORT ContextProviderFactoryImpl
    : public ui::ContextProviderFactory {
 public:
  // The factory must outlive the ContextProviderFactoryImpl instance, which
  // will be destroyed when terminate is called.
  static void Initialize(gpu::GpuChannelEstablishFactory* gpu_channel_factory);

  static void Terminate();

  static ContextProviderFactoryImpl* GetInstance();

  ~ContextProviderFactoryImpl() override;

  scoped_refptr<cc::ContextProvider> CreateDisplayContextProvider(
      gpu::SurfaceHandle surface_handle,
      gpu::SharedMemoryLimits shared_memory_limits,
      gpu::gles2::ContextCreationAttribHelper attributes,
      bool support_locking,
      bool automatic_flushes,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);

  // ContextProviderFactory implementation.
  scoped_refptr<cc::VulkanContextProvider> GetSharedVulkanContextProvider()
      override;
  void RequestGpuChannelHost(GpuChannelHostCallback callback) override;
  scoped_refptr<cc::ContextProvider> CreateOffscreenContextProvider(
      ContextType context_type,
      gpu::SharedMemoryLimits shared_memory_limits,
      gpu::gles2::ContextCreationAttribHelper attributes,
      bool support_locking,
      bool automatic_flushes,
      cc::ContextProvider* shared_context_provider,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) override;
  cc::SurfaceManager* GetSurfaceManager() override;
  cc::FrameSinkId AllocateFrameSinkId() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;

 private:
  ContextProviderFactoryImpl(
      gpu::GpuChannelEstablishFactory* gpu_channel_factory);

  scoped_refptr<cc::ContextProvider> CreateContextProviderInternal(
      command_buffer_metrics::ContextType context_type,
      gpu::SurfaceHandle surface_handle,
      gpu::SharedMemoryLimits shared_memory_limits,
      gpu::gles2::ContextCreationAttribHelper attributes,
      bool support_locking,
      bool automatic_flushes,
      cc::ContextProvider* shared_context_provider,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);

  // Will return nullptr if the Gpu channel has not been established.
  void EstablishGpuChannel();
  void OnGpuChannelEstablished(scoped_refptr<gpu::GpuChannelHost> gpu_channel);
  void OnGpuChannelTimeout();

  void HandlePendingRequests(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      GpuChannelHostResult result);

  gpu::GpuChannelEstablishFactory* gpu_channel_factory_;

  std::queue<GpuChannelHostCallback> gpu_channel_requests_;

  scoped_refptr<ContextProviderCommandBuffer> shared_worker_context_provider_;

  scoped_refptr<cc::VulkanContextProvider> shared_vulkan_context_provider_;

  bool in_handle_pending_requests_;

  bool in_shutdown_;

  base::OneShotTimer establish_gpu_channel_timeout_;

  std::unique_ptr<cc::SurfaceManager> surface_manager_;
  uint32_t next_client_id_;

  base::WeakPtrFactory<ContextProviderFactoryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CONTEXT_PROVIDER_FACTORY_IMPL_ANDROID_H_
