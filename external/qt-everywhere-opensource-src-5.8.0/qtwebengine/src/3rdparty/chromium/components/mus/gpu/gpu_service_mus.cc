// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/gpu_service_mus.h"

#include "base/memory/shared_memory.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/mus/gpu/mus_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"
#include "media/gpu/ipc/service/media_service.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace mus {
namespace {

const int kLocalGpuChannelClientId = 1;
const uint64_t kLocalGpuChannelClientTracingId = 1;

void EstablishGpuChannelDone(
    int client_id,
    const IPC::ChannelHandle* channel_handle,
    const GpuServiceMus::EstablishGpuChannelCallback& callback) {
  callback.Run(channel_handle ? client_id : -1, *channel_handle);
}
}

GpuServiceMus::GpuServiceMus()
    : next_client_id_(kLocalGpuChannelClientId),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      gpu_thread_("GpuThread"),
      io_thread_("GpuIOThread") {
  Initialize();
}

GpuServiceMus::~GpuServiceMus() {
  // Signal this event before destroying the child process.  That way all
  // background threads can cleanup.
  // For example, in the renderer the RenderThread instances will be able to
  // notice shutdown before the render process begins waiting for them to exit.
  shutdown_event_.Signal();
  io_thread_.Stop();
}

void GpuServiceMus::EstablishGpuChannel(
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams,
    const EstablishGpuChannelCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (!gpu_channel_manager_) {
    callback.Run(-1, IPC::ChannelHandle());
    return;
  }

  const int client_id = ++next_client_id_;
  IPC::ChannelHandle* channel_handle = new IPC::ChannelHandle;
  gpu_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GpuServiceMus::EstablishGpuChannelOnGpuThread,
                 base::Unretained(this), client_id, client_tracing_id, preempts,
                 allow_view_command_buffers, allow_real_time_streams,
                 base::Unretained(channel_handle)),
      base::Bind(&EstablishGpuChannelDone, client_id,
                 base::Owned(channel_handle), callback));
}

gfx::GpuMemoryBufferHandle GpuServiceMus::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle) {
  DCHECK(CalledOnValidThread());
  return gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_handle);
}

void GpuServiceMus::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                           int client_id,
                                           const gpu::SyncToken& sync_token) {
  DCHECK(CalledOnValidThread());

  if (gpu_channel_manager_)
    gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void GpuServiceMus::DidCreateOffscreenContext(const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::DidDestroyChannel(int client_id) {
  media_service_->RemoveChannel(client_id);
  NOTIMPLEMENTED();
}

void GpuServiceMus::DidDestroyOffscreenContext(const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::DidLoseContext(bool offscreen,
                                   gpu::error::ContextLostReason reason,
                                   const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::GpuMemoryUmaStats(const gpu::GPUMemoryUmaStats& params) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::StoreShaderToDisk(int client_id,
                                      const std::string& key,
                                      const std::string& shader) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void GpuServiceMus::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  NOTIMPLEMENTED();
}
#endif

void GpuServiceMus::SetActiveURL(const GURL& url) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::Initialize() {
  DCHECK(CalledOnValidThread());
  base::Thread::Options thread_options(base::MessageLoop::TYPE_DEFAULT, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
  CHECK(gpu_thread_.StartWithOptions(thread_options));

  thread_options = base::Thread::Options(base::MessageLoop::TYPE_IO, 0);
  thread_options.priority = base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID)
  // TODO(reveman): Remove this in favor of setting it explicitly for each type
  // of process.
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  CHECK(io_thread_.StartWithOptions(thread_options));

  IPC::ChannelHandle channel_handle;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&GpuServiceMus::InitializeOnGpuThread,
                            base::Unretained(this), &channel_handle, &event));
  event.Wait();

  gpu_memory_buffer_manager_local_.reset(
      new MusGpuMemoryBufferManager(this, kLocalGpuChannelClientId));
  gpu_channel_local_ = gpu::GpuChannelHost::Create(
      this, kLocalGpuChannelClientId, gpu_info_, channel_handle,
      &shutdown_event_, gpu_memory_buffer_manager_local_.get());
}

void GpuServiceMus::InitializeOnGpuThread(IPC::ChannelHandle* channel_handle,
                                          base::WaitableEvent* event) {
  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences_);
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoEncodeAccelerator::GetSupportedProfiles(gpu_preferences_);
  gpu_info_.jpeg_decode_accelerator_supported =
      media::GpuJpegDecodeAccelerator::IsSupported();

#if defined(USE_OZONE)
  ui::OzonePlatform::InitializeForGPU();
#endif

  if (gpu::GetNativeGpuMemoryBufferType() != gfx::EMPTY_BUFFER) {
    gpu_memory_buffer_factory_ =
        gpu::GpuMemoryBufferFactory::CreateNativeType();
  }

  if (!gl::init::InitializeGLOneOff())
    VLOG(1) << "gl::init::InitializeGLOneOff failed";

  DCHECK(!owned_sync_point_manager_);
  const bool allow_threaded_wait = false;
  owned_sync_point_manager_.reset(
      new gpu::SyncPointManager(allow_threaded_wait));

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  // TODO(penghuang): implement a watchdog.
  gpu::GpuWatchdog* watchdog = nullptr;
  gpu_channel_manager_.reset(new gpu::GpuChannelManager(
      gpu_preferences_, this, watchdog,
      base::ThreadTaskRunnerHandle::Get().get(), io_thread_.task_runner().get(),
      &shutdown_event_, owned_sync_point_manager_.get(),
      gpu_memory_buffer_factory_.get()));

  media_service_.reset(new media::MediaService(gpu_channel_manager_.get()));

  const bool preempts = true;
  const bool allow_view_command_buffers = true;
  const bool allow_real_time_streams = true;
  EstablishGpuChannelOnGpuThread(
      kLocalGpuChannelClientId, kLocalGpuChannelClientTracingId, preempts,
      allow_view_command_buffers, allow_real_time_streams, channel_handle);
  event->Signal();
}

void GpuServiceMus::EstablishGpuChannelOnGpuThread(
    int client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams,
    IPC::ChannelHandle* channel_handle) {
  if (gpu_channel_manager_) {
    *channel_handle = gpu_channel_manager_->EstablishChannel(
        client_id, client_tracing_id, preempts, allow_view_command_buffers,
        allow_real_time_streams);
    media_service_->AddChannel(client_id);
  }
}

bool GpuServiceMus::IsMainThread() {
  return main_task_runner_->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
GpuServiceMus::GetIOThreadTaskRunner() {
  return io_thread_.task_runner();
}

std::unique_ptr<base::SharedMemory> GpuServiceMus::AllocateSharedMemory(
    size_t size) {
  std::unique_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAnonymous(size))
    return std::unique_ptr<base::SharedMemory>();
  return shm;
}

// static
GpuServiceMus* GpuServiceMus::GetInstance() {
  return base::Singleton<GpuServiceMus,
                         base::LeakySingletonTraits<GpuServiceMus>>::get();
}

}  // namespace mus
