// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_GPU_SERVICE_MUS_H_
#define COMPONENTS_MUS_GPU_GPU_SERVICE_MUS_H_

#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace IPC {
struct ChannelHandle;
}

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferFactory;
class SyncPointManager;
}

namespace media {
class MediaService;
}

namespace mus {

class MusGpuMemoryBufferManager;

// TODO(fsamuel): GpuServiceMus is intended to be the Gpu thread within Mus.
// Similar to GpuChildThread, it is a GpuChannelManagerDelegate and will have a
// GpuChannelManager.
class GpuServiceMus : public gpu::GpuChannelManagerDelegate,
                      public gpu::GpuChannelHostFactory,
                      public base::NonThreadSafe {
 public:
  typedef base::Callback<void(int client_id, const IPC::ChannelHandle&)>
      EstablishGpuChannelCallback;
  void EstablishGpuChannel(uint64_t client_tracing_id,
                           bool preempts,
                           bool allow_view_command_buffers,
                           bool allow_real_time_streams,
                           const EstablishGpuChannelCallback& callback);
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      gpu::SurfaceHandle surface_handle);
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferFromeHandle(
      gfx::GpuMemoryBufferHandle buffer_handle,
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token);

  gpu::GpuChannelManager* gpu_channel_manager() const {
    return gpu_channel_manager_.get();
  }

  gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory() const {
    return gpu_memory_buffer_factory_.get();
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_local() const {
    return gpu_channel_local_;
  }

  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }

  // GpuChannelManagerDelegate overrides:
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void GpuMemoryUmaStats(const gpu::GPUMemoryUmaStats& params) override;
  void StoreShaderToDisk(int client_id,
                         const std::string& key,
                         const std::string& shader) override;
#if defined(OS_WIN)
  void SendAcceleratedSurfaceCreatedChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  void SetActiveURL(const GURL& url) override;

  // GpuChannelHostFactory overrides:
  bool IsMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() override;
  std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) override;

  static GpuServiceMus* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<GpuServiceMus>;

  GpuServiceMus();
  ~GpuServiceMus() override;

  void Initialize();
  void InitializeOnGpuThread(IPC::ChannelHandle* channel_handle,
                             base::WaitableEvent* event);
  void EstablishGpuChannelOnGpuThread(int client_id,
                                      uint64_t client_tracing_id,
                                      bool preempts,
                                      bool allow_view_command_buffers,
                                      bool allow_real_time_streams,
                                      IPC::ChannelHandle* channel_handle);

  // The next client id.
  int next_client_id_;

  // The main thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // The main thread for GpuService.
  base::Thread gpu_thread_;

  // The thread that handles IO events for GpuService.
  base::Thread io_thread_;

  std::unique_ptr<gpu::SyncPointManager> owned_sync_point_manager_;

  std::unique_ptr<gpu::GpuChannelManager> gpu_channel_manager_;

  std::unique_ptr<media::MediaService> media_service_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  // A GPU memory buffer manager used locally.
  std::unique_ptr<MusGpuMemoryBufferManager> gpu_memory_buffer_manager_local_;

  // A GPU channel used locally.
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_local_;

  gpu::GpuPreferences gpu_preferences_;

  // Information about the GPU, such as device and vendor ID.
  gpu::GPUInfo gpu_info_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceMus);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GPU_GPU_SERVICE_MUS_H_
