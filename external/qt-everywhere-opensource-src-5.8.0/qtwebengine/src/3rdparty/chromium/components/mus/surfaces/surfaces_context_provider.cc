// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/surfaces_context_provider.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/mus/common/switches.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/gles2/command_buffer_impl.h"
#include "components/mus/gles2/command_buffer_local.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/gpu/gpu_service_mus.h"
#include "components/mus/surfaces/surfaces_context_provider_delegate.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "ui/gl/gpu_preference.h"

namespace mus {

SurfacesContextProvider::SurfacesContextProvider(
    gfx::AcceleratedWidget widget,
    const scoped_refptr<GpuState>& state)
    : use_chrome_gpu_command_buffer_(false),
      delegate_(nullptr),
      widget_(widget),
      command_buffer_local_(nullptr) {
// TODO(penghuang): Kludge: Use mojo command buffer when running on Windows
// since Chrome command buffer breaks unit tests
#if defined(OS_WIN)
  use_chrome_gpu_command_buffer_ = false;
#else
  use_chrome_gpu_command_buffer_ =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMojoGpuCommandBufferInMus);
#endif
  if (!use_chrome_gpu_command_buffer_) {
    command_buffer_local_ = new CommandBufferLocal(this, widget_, state);
  } else {
    GpuServiceMus* service = GpuServiceMus::GetInstance();
    gpu::CommandBufferProxyImpl* shared_command_buffer = nullptr;
    gpu::GpuStreamId stream_id = gpu::GpuStreamId::GPU_STREAM_DEFAULT;
    gpu::GpuStreamPriority stream_priority = gpu::GpuStreamPriority::NORMAL;
    gpu::gles2::ContextCreationAttribHelper attributes;
    attributes.alpha_size = -1;
    attributes.depth_size = 0;
    attributes.stencil_size = 0;
    attributes.samples = 0;
    attributes.sample_buffers = 0;
    attributes.bind_generates_resource = false;
    attributes.lose_context_when_out_of_memory = true;
    GURL active_url;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        base::ThreadTaskRunnerHandle::Get();
    command_buffer_proxy_impl_ = gpu::CommandBufferProxyImpl::Create(
        service->gpu_channel_local(), widget, shared_command_buffer, stream_id,
        stream_priority, attributes, active_url, task_runner);
    command_buffer_proxy_impl_->SetSwapBuffersCompletionCallback(
        base::Bind(&SurfacesContextProvider::OnGpuSwapBuffersCompleted,
                   base::Unretained(this)));
    command_buffer_proxy_impl_->SetUpdateVSyncParametersCallback(
        base::Bind(&SurfacesContextProvider::OnUpdateVSyncParameters,
                   base::Unretained(this)));
  }
}

void SurfacesContextProvider::SetDelegate(
    SurfacesContextProviderDelegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

// This routine needs to be safe to call more than once.
// This is called when we have an accelerated widget.
bool SurfacesContextProvider::BindToCurrentThread() {
  if (implementation_)
    return true;

  // SurfacesContextProvider should always live on the same thread as the
  // Window Manager.
  DCHECK(CalledOnValidThread());
  gpu::GpuControl* gpu_control = nullptr;
  gpu::CommandBuffer* command_buffer = nullptr;
  if (!use_chrome_gpu_command_buffer_) {
    if (!command_buffer_local_->Initialize())
      return false;
    gpu_control = command_buffer_local_;
    command_buffer = command_buffer_local_;
  } else {
    if (!command_buffer_proxy_impl_)
      return false;
    gpu_control = command_buffer_proxy_impl_.get();
    command_buffer = command_buffer_proxy_impl_.get();
  }

  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer));
  constexpr gpu::SharedMemoryLimits default_limits;
  if (!gles2_helper_->Initialize(default_limits.command_buffer_size))
    return false;
  gles2_helper_->SetAutomaticFlushes(false);
  transfer_buffer_.reset(new gpu::TransferBuffer(gles2_helper_.get()));
  capabilities_ = gpu_control->GetCapabilities();
  bool bind_generates_resource =
      !!capabilities_.bind_generates_resource_chromium;
  // TODO(piman): Some contexts (such as compositor) want this to be true, so
  // this needs to be a public parameter.
  bool lose_context_when_out_of_memory = false;
  bool support_client_side_arrays = false;
  implementation_.reset(new gpu::gles2::GLES2Implementation(
      gles2_helper_.get(), NULL, transfer_buffer_.get(),
      bind_generates_resource, lose_context_when_out_of_memory,
      support_client_side_arrays, gpu_control));
  return implementation_->Initialize(
      default_limits.start_transfer_buffer_size,
      default_limits.min_transfer_buffer_size,
      default_limits.max_transfer_buffer_size,
      default_limits.mapped_memory_reclaim_limit);
}

gpu::gles2::GLES2Interface* SurfacesContextProvider::ContextGL() {
  DCHECK(implementation_);
  return implementation_.get();
}

gpu::ContextSupport* SurfacesContextProvider::ContextSupport() {
  return implementation_.get();
}

class GrContext* SurfacesContextProvider::GrContext() {
  return NULL;
}

void SurfacesContextProvider::InvalidateGrContext(uint32_t state) {}

gpu::Capabilities SurfacesContextProvider::ContextCapabilities() {
  return capabilities_;
}

base::Lock* SurfacesContextProvider::GetLock() {
  // This context provider is not used on multiple threads.
  NOTREACHED();
  return nullptr;
}

void SurfacesContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  implementation_->SetLostContextCallback(lost_context_callback);
}

SurfacesContextProvider::~SurfacesContextProvider() {
  implementation_->Flush();
  implementation_.reset();
  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_proxy_impl_.reset();
  if (command_buffer_local_) {
    command_buffer_local_->Destroy();
    command_buffer_local_ = nullptr;
  }
}

void SurfacesContextProvider::UpdateVSyncParameters(
    const base::TimeTicks& timebase,
    const base::TimeDelta& interval) {
  if (delegate_)
    delegate_->OnVSyncParametersUpdated(timebase, interval);
}

void SurfacesContextProvider::GpuCompletedSwapBuffers(gfx::SwapResult result) {
  if (!swap_buffers_completion_callback_.is_null()) {
    swap_buffers_completion_callback_.Run(result);
  }
}

void SurfacesContextProvider::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result,
    const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
  if (!swap_buffers_completion_callback_.is_null()) {
    swap_buffers_completion_callback_.Run(result);
  }
}

void SurfacesContextProvider::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (delegate_)
    delegate_->OnVSyncParametersUpdated(timebase, interval);
}

void SurfacesContextProvider::SetSwapBuffersCompletionCallback(
    gl::GLSurface::SwapCompletionCallback callback) {
  swap_buffers_completion_callback_ = callback;
}

}  // namespace mus
