// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_SURFACES_CONTEXT_PROVIDER_H_
#define SERVICES_UI_SURFACES_SURFACES_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/context_provider.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {

class CommandBufferProxyImpl;
class GpuChannelHost;
struct GpuProcessHostedCALayerTreeParamsMac;
class TransferBuffer;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}

}  // namespace gpu

namespace ui {
class LatencyInfo;
}

namespace ui {

class CommandBufferDriver;
class CommandBufferImpl;
class SurfacesContextProviderDelegate;

// TODO(fsamuel): This can probably be merged with ContextProviderCommandBuffer.
class SurfacesContextProvider : public cc::ContextProvider,
                                public base::NonThreadSafe {
 public:
  SurfacesContextProvider(gfx::AcceleratedWidget widget,
                          scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  void SetDelegate(SurfacesContextProviderDelegate* delegate);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  cc::ContextCacheController* CacheController() override;
  void InvalidateGrContext(uint32_t state) override;
  gpu::Capabilities ContextCapabilities() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;
  base::Lock* GetLock() override;

  // SurfacesContextProvider API.
  void SetSwapBuffersCompletionCallback(
      gl::GLSurface::SwapCompletionCallback callback);

 protected:
  friend class base::RefCountedThreadSafe<SurfacesContextProvider>;
  ~SurfacesContextProvider() override;

 private:
  // Callbacks for CommandBufferProxyImpl:
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac);
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);

  // From GLES2Context:
  // Initialized in BindToCurrentThread.
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> implementation_;
  std::unique_ptr<cc::ContextCacheController> cache_controller_;

  gpu::Capabilities capabilities_;
  LostContextCallback lost_context_callback_;

  SurfacesContextProviderDelegate* delegate_;
  gfx::AcceleratedWidget widget_;
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_proxy_impl_;
  gl::GLSurface::SwapCompletionCallback swap_buffers_completion_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesContextProvider);
};

}  // namespace ui

#endif  // SERVICES_UI_SURFACES_SURFACES_CONTEXT_PROVIDER_H_
