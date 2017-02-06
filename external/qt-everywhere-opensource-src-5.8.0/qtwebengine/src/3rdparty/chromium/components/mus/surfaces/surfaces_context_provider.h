// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_H_
#define COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/context_provider.h"
#include "components/mus/gles2/command_buffer_local_client.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

class CommandBufferProxyImpl;
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

namespace mus {

class CommandBufferDriver;
class CommandBufferImpl;
class CommandBufferLocal;
class GpuState;
class SurfacesContextProviderDelegate;

class SurfacesContextProvider : public cc::ContextProvider,
                                public CommandBufferLocalClient,
                                public base::NonThreadSafe {
 public:
  SurfacesContextProvider(gfx::AcceleratedWidget widget,
                          const scoped_refptr<GpuState>& state);

  void SetDelegate(SurfacesContextProviderDelegate* delegate);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  gpu::Capabilities ContextCapabilities() override;
  void DeleteCachedResources() override {}
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
  // CommandBufferLocalClient:
  void UpdateVSyncParameters(const base::TimeTicks& timebase,
                             const base::TimeDelta& interval) override;
  void GpuCompletedSwapBuffers(gfx::SwapResult result) override;

  // Callbacks for CommandBufferProxyImpl:
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac);
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);

  bool use_chrome_gpu_command_buffer_;

  // From GLES2Context:
  // Initialized in BindToCurrentThread.
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> implementation_;

  gpu::Capabilities capabilities_;
  LostContextCallback lost_context_callback_;

  SurfacesContextProviderDelegate* delegate_;
  gfx::AcceleratedWidget widget_;
  CommandBufferLocal* command_buffer_local_;
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_proxy_impl_;
  gl::GLSurface::SwapCompletionCallback swap_buffers_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesContextProvider);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_H_
