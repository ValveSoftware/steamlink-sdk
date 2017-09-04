// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_GPU_SERVICE_INTERNAL_H_
#define SERVICES_UI_GPU_GPU_SERVICE_INTERNAL_H_

#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferFactory;
class GpuWatchdogThread;
class SyncPointManager;
}

namespace media {
class MediaGpuChannelManager;
}

namespace ui {

class GpuMain;

// This runs in the GPU process, and communicates with the gpu host (which is
// the window server) over the mojom APIs. This is responsible for setting up
// the connection to clients, allocating/free'ing gpu memory etc.
class GpuServiceInternal : public gpu::GpuChannelManagerDelegate,
                           public mojom::GpuServiceInternal,
                           public base::NonThreadSafe {
 public:
  ~GpuServiceInternal() override;

  void Add(mojom::GpuServiceInternalRequest request);

 private:
  friend class GpuMain;

  GpuServiceInternal(const gpu::GPUInfo& gpu_info,
                     std::unique_ptr<gpu::GpuWatchdogThread> watchdog,
                     gpu::GpuMemoryBufferFactory* memory_buffer_factory,
                     scoped_refptr<base::SingleThreadTaskRunner> io_runner);

  gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferFromeHandle(
      gfx::GpuMemoryBufferHandle buffer_handle,
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id);

  // gpu::GpuChannelManagerDelegate:
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void StoreShaderToDisk(int client_id,
                         const std::string& key,
                         const std::string& shader) override;
#if defined(OS_WIN)
  void SendAcceleratedSurfaceCreatedChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  void SetActiveURL(const GURL& url) override;

  // mojom::GpuServiceInternal:
  void Initialize(const InitializeCallback& callback) override;
  void EstablishGpuChannel(
      int32_t client_id,
      uint64_t client_tracing_id,
      bool is_gpu_host,
      const EstablishGpuChannelCallback& callback) override;
  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      gpu::SurfaceHandle surface_handle,
      const CreateGpuMemoryBufferCallback& callback) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token) override;

  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;

  gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory_;

  gpu::GpuPreferences gpu_preferences_;

  // Information about the GPU, such as device and vendor ID.
  gpu::GPUInfo gpu_info_;

  std::unique_ptr<gpu::SyncPointManager> owned_sync_point_manager_;
  std::unique_ptr<gpu::GpuChannelManager> gpu_channel_manager_;
  std::unique_ptr<media::MediaGpuChannelManager> media_gpu_channel_manager_;
  mojo::Binding<mojom::GpuServiceInternal> binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceInternal);
};

}  // namespace ui

#endif  // SERVICES_UI_GPU_GPU_SERVICE_INTERNAL_H_
