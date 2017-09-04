// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_service_internal.h"

#include "base/memory/shared_memory.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

namespace ui {

GpuServiceInternal::GpuServiceInternal(
    const gpu::GPUInfo& gpu_info,
    std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : io_runner_(std::move(io_runner)),
      shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      watchdog_thread_(std::move(watchdog_thread)),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      gpu_info_(gpu_info),
      binding_(this) {}

GpuServiceInternal::~GpuServiceInternal() {
  binding_.Close();
  media_gpu_channel_manager_.reset();
  gpu_channel_manager_.reset();
  owned_sync_point_manager_.reset();

  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();
}

void GpuServiceInternal::Add(mojom::GpuServiceInternalRequest request) {
  binding_.Close();
  binding_.Bind(std::move(request));
}

void GpuServiceInternal::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle,
    const CreateGpuMemoryBufferCallback& callback) {
  DCHECK(CalledOnValidThread());
  callback.Run(gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_handle));
}

void GpuServiceInternal::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  DCHECK(CalledOnValidThread());
  if (gpu_channel_manager_)
    gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void GpuServiceInternal::DidCreateOffscreenContext(const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceInternal::DidDestroyChannel(int client_id) {
  media_gpu_channel_manager_->RemoveChannel(client_id);
  NOTIMPLEMENTED();
}

void GpuServiceInternal::DidDestroyOffscreenContext(const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceInternal::DidLoseContext(bool offscreen,
                                        gpu::error::ContextLostReason reason,
                                        const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceInternal::StoreShaderToDisk(int client_id,
                                           const std::string& key,
                                           const std::string& shader) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void GpuServiceInternal::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  ::SetParent(child_window, parent_window);
}
#endif

void GpuServiceInternal::SetActiveURL(const GURL& url) {
  // TODO(penghuang): implement this function.
}

void GpuServiceInternal::Initialize(const InitializeCallback& callback) {
  DCHECK(CalledOnValidThread());
  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences_);
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoEncodeAccelerator::GetSupportedProfiles(gpu_preferences_);
  gpu_info_.jpeg_decode_accelerator_supported =
      media::GpuJpegDecodeAccelerator::IsSupported();

  DCHECK(!owned_sync_point_manager_);
  const bool allow_threaded_wait = false;
  owned_sync_point_manager_.reset(
      new gpu::SyncPointManager(allow_threaded_wait));

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_.reset(new gpu::GpuChannelManager(
      gpu_preferences_, this, watchdog_thread_.get(),
      base::ThreadTaskRunnerHandle::Get().get(), io_runner_.get(),
      &shutdown_event_, owned_sync_point_manager_.get(),
      gpu_memory_buffer_factory_));

  media_gpu_channel_manager_.reset(
      new media::MediaGpuChannelManager(gpu_channel_manager_.get()));
  callback.Run(gpu_info_);
}

void GpuServiceInternal::EstablishGpuChannel(
    int32_t client_id,
    uint64_t client_tracing_id,
    bool is_gpu_host,
    const EstablishGpuChannelCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (!gpu_channel_manager_) {
    callback.Run(mojo::ScopedMessagePipeHandle());
    return;
  }

  const bool preempts = is_gpu_host;
  const bool allow_view_command_buffers = is_gpu_host;
  const bool allow_real_time_streams = is_gpu_host;
  mojo::ScopedMessagePipeHandle channel_handle;
  IPC::ChannelHandle handle = gpu_channel_manager_->EstablishChannel(
      client_id, client_tracing_id, preempts, allow_view_command_buffers,
      allow_real_time_streams);
  channel_handle.reset(handle.mojo_handle);
  media_gpu_channel_manager_->AddChannel(client_id);
  callback.Run(std::move(channel_handle));
}

}  // namespace ui
