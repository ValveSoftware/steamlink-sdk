// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/gles2_context.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "mojo/public/cpp/system/core.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/interfaces/gpu_service.mojom.h"
#include "url/gurl.h"

namespace ui {

GLES2Context::GLES2Context() {}

GLES2Context::~GLES2Context() {}

bool GLES2Context::Initialize(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(gpu_channel_host);
  gpu::SurfaceHandle surface_handle = gfx::kNullAcceleratedWidget;
  // TODO(penghuang): support shared group.
  gpu::CommandBufferProxyImpl* shared_command_buffer = nullptr;
  gpu::GpuStreamId stream_id = gpu::GpuStreamId::GPU_STREAM_DEFAULT;
  gpu::GpuStreamPriority stream_priority = gpu::GpuStreamPriority::NORMAL;
  gpu::gles2::ContextCreationAttribHelper attributes;
  // TODO(penghuang): figure a useful active_url.
  GURL active_url;
  command_buffer_proxy_impl_ = gpu::CommandBufferProxyImpl::Create(
      std::move(gpu_channel_host), surface_handle, shared_command_buffer,
      stream_id, stream_priority, attributes, active_url,
      std::move(task_runner));
  if (!command_buffer_proxy_impl_)
    return false;
  gpu::CommandBuffer* command_buffer = command_buffer_proxy_impl_.get();
  gpu::GpuControl* gpu_control = command_buffer_proxy_impl_.get();

  constexpr gpu::SharedMemoryLimits default_limits;
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer));
  if (!gles2_helper_->Initialize(default_limits.command_buffer_size))
    return false;
  gles2_helper_->SetAutomaticFlushes(false);
  transfer_buffer_.reset(new gpu::TransferBuffer(gles2_helper_.get()));
  gpu::Capabilities capabilities = gpu_control->GetCapabilities();
  bool bind_generates_resource =
      !!capabilities.bind_generates_resource_chromium;
  // TODO(piman): Some contexts (such as compositor) want this to be true, so
  // this needs to be a public parameter.
  bool lose_context_when_out_of_memory = false;
  bool support_client_side_arrays = false;
  implementation_.reset(new gpu::gles2::GLES2Implementation(
      gles2_helper_.get(), NULL, transfer_buffer_.get(),
      bind_generates_resource, lose_context_when_out_of_memory,
      support_client_side_arrays, gpu_control));
  if (!implementation_->Initialize(default_limits.start_transfer_buffer_size,
                                   default_limits.min_transfer_buffer_size,
                                   default_limits.max_transfer_buffer_size,
                                   default_limits.mapped_memory_reclaim_limit))
    return false;
  return true;
}

// static
std::unique_ptr<GLES2Context> GLES2Context::CreateOffscreenContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!gpu_channel_host)
    return nullptr;

  // Return the GLES2Context only if it is successfully initialized. If
  // initialization fails, then return null.
  std::unique_ptr<GLES2Context> gles2_context(new GLES2Context);
  if (!gles2_context->Initialize(std::move(gpu_channel_host),
                                 std::move(task_runner)))
    gles2_context.reset();
  return gles2_context;
}

}  // namespace ui
