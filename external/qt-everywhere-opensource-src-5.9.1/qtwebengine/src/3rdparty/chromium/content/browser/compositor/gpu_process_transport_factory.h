// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ui/compositor/compositor.h"

namespace base {
class Thread;
}

namespace cc {
class SingleThreadTaskGraphRunner;
class SoftwareOutputDevice;
class SurfaceManager;
class VulkanInProcessContextProvider;
}

namespace content {
class ContextProviderCommandBuffer;
class OutputDeviceBacking;

class GpuProcessTransportFactory
    : public ui::ContextFactory,
      public ImageTransportFactory {
 public:
  GpuProcessTransportFactory();

  ~GpuProcessTransportFactory() override;

  // ui::ContextFactory implementation.
  void CreateCompositorFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  std::unique_ptr<ui::Reflector> CreateReflector(ui::Compositor* source,
                                                 ui::Layer* target) override;
  void RemoveReflector(ui::Reflector* reflector) override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  bool DoesCreateTestContexts() override;
  uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                 gfx::BufferUsage usage) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  cc::FrameSinkId AllocateFrameSinkId() override;
  void SetDisplayVisible(ui::Compositor* compositor, bool visible) override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;
  void SetDisplayColorSpace(ui::Compositor* compositor,
                            const gfx::ColorSpace& color_space) override;
  void SetAuthoritativeVSyncInterval(ui::Compositor* compositor,
                                     base::TimeDelta interval) override;
  void SetDisplayVSyncParameters(ui::Compositor* compositor,
                                 base::TimeTicks timebase,
                                 base::TimeDelta interval) override;
  void SetOutputIsSecure(ui::Compositor* compositor, bool secure) override;
  void AddObserver(ui::ContextFactoryObserver* observer) override;
  void RemoveObserver(ui::ContextFactoryObserver* observer) override;

  // ImageTransportFactory implementation.
  ui::ContextFactory* GetContextFactory() override;
  cc::SurfaceManager* GetSurfaceManager() override;
  display_compositor::GLHelper* GetGLHelper() override;
  void SetGpuChannelEstablishFactory(
      gpu::GpuChannelEstablishFactory* factory) override;
#if defined(OS_MACOSX)
  void SetCompositorSuspendedForRecycle(ui::Compositor* compositor,
                                        bool suspended) override;
#endif

 private:
  struct PerCompositorData;

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor);
  std::unique_ptr<cc::SoftwareOutputDevice> CreateSoftwareOutputDevice(
      ui::Compositor* compositor);
  void EstablishedGpuChannel(
      base::WeakPtr<ui::Compositor> compositor,
      bool create_gpu_output_surface,
      int num_attempts,
      scoped_refptr<gpu::GpuChannelHost> established_channel_host);

  void OnLostMainThreadSharedContextInsideCallback();
  void OnLostMainThreadSharedContext();

  scoped_refptr<cc::VulkanInProcessContextProvider>
  SharedVulkanContextProvider();

  std::unique_ptr<cc::SurfaceManager> surface_manager_;
  uint32_t next_sink_id_ = 1u;

#if defined(OS_WIN)
  // Used by output surface, stored in PerCompositorData.
  std::unique_ptr<OutputDeviceBacking> software_backing_;
#endif

  // Depends on SurfaceManager.
  typedef std::map<ui::Compositor*, std::unique_ptr<PerCompositorData>>
      PerCompositorDataMap;
  PerCompositorDataMap per_compositor_data_;

  scoped_refptr<ContextProviderCommandBuffer> shared_main_thread_contexts_;
  std::unique_ptr<display_compositor::GLHelper> gl_helper_;
  base::ObserverList<ui::ContextFactoryObserver> observer_list_;
  std::unique_ptr<cc::SingleThreadTaskGraphRunner> task_graph_runner_;
  scoped_refptr<ContextProviderCommandBuffer> shared_worker_context_provider_;

  bool shared_vulkan_context_provider_initialized_ = false;
  scoped_refptr<cc::VulkanInProcessContextProvider>
      shared_vulkan_context_provider_;

  gpu::GpuChannelEstablishFactory* gpu_channel_factory_ = nullptr;

  base::WeakPtrFactory<GpuProcessTransportFactory> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_PROCESS_TRANSPORT_FACTORY_H_
