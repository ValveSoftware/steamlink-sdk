// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_driver.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/mus/common/mojo_buffer_backing.h"
#include "components/mus/gles2/gl_surface_adapter.h"
#include "components/mus/gles2/gpu_memory_tracker.h"
#include "components/mus/gles2/gpu_state.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/command_executor.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "ui/gl/gl_image_ozone_native_pixmap.h"
#endif

namespace mus {

namespace {

// The first time polling a fence, delay some extra time to allow other
// stubs to process some work, or else the timing of the fences could
// allow a pattern of alternating fast and slow frames to occur.
const int64_t kHandleMoreWorkPeriodMs = 2;
const int64_t kHandleMoreWorkPeriodBusyMs = 1;

// Prevents idle work from being starved.
const int64_t kMaxTimeSinceIdleMs = 10;

}  // namespace

CommandBufferDriver::Client::~Client() {}

CommandBufferDriver::CommandBufferDriver(
    gpu::CommandBufferNamespace command_buffer_namespace,
    gpu::CommandBufferId command_buffer_id,
    gfx::AcceleratedWidget widget,
    scoped_refptr<GpuState> gpu_state)
    : command_buffer_namespace_(command_buffer_namespace),
      command_buffer_id_(command_buffer_id),
      widget_(widget),
      client_(nullptr),
      gpu_state_(gpu_state),
      previous_processed_num_(0),
      weak_factory_(this) {
  DCHECK_EQ(base::ThreadTaskRunnerHandle::Get(),
            gpu_state_->command_buffer_task_runner()->task_runner());
}

CommandBufferDriver::~CommandBufferDriver() {
  DCHECK(CalledOnValidThread());
  DestroyDecoder();
}

bool CommandBufferDriver::Initialize(
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs) {
  DCHECK(CalledOnValidThread());
  gpu::gles2::ContextCreationAttribHelper attrib_helper;
  if (!attrib_helper.Parse(attribs.storage()))
    return false;
  // TODO(piman): attribs can't currently represent gpu_preference.

  const bool offscreen = widget_ == gfx::kNullAcceleratedWidget;
  if (offscreen) {
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size(0, 0));
  } else {
#if defined(USE_OZONE)
    scoped_refptr<gl::GLSurface> underlying_surface =
        gl::init::CreateSurfacelessViewGLSurface(widget_);
    if (!underlying_surface)
      underlying_surface = gl::init::CreateViewGLSurface(widget_);
#else
    scoped_refptr<gl::GLSurface> underlying_surface =
        gl::init::CreateViewGLSurface(widget_);
#endif
    scoped_refptr<GLSurfaceAdapterMus> surface_adapter =
        new GLSurfaceAdapterMus(underlying_surface);
    surface_adapter->SetGpuCompletedSwapBuffersCallback(
        base::Bind(&CommandBufferDriver::OnGpuCompletedSwapBuffers,
                   weak_factory_.GetWeakPtr()));
    surface_ = surface_adapter;

    gfx::VSyncProvider* vsync_provider =
        surface_ ? surface_->GetVSyncProvider() : nullptr;
    if (vsync_provider) {
      vsync_provider->GetVSyncParameters(
          base::Bind(&CommandBufferDriver::OnUpdateVSyncParameters,
                     weak_factory_.GetWeakPtr()));
    }
  }

  if (!surface_.get())
    return false;

  // TODO(piman): virtual contexts.
  context_ = gl::init::CreateGLContext(
      gpu_state_->share_group(), surface_.get(), attrib_helper.gpu_preference);
  if (!context_.get())
    return false;

  if (!context_->MakeCurrent(surface_.get()))
    return false;

  // TODO(piman): ShaderTranslatorCache is currently per-ContextGroup but
  // only needs to be per-thread.
  const bool bind_generates_resource = attrib_helper.bind_generates_resource;
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info =
      new gpu::gles2::FeatureInfo(gpu_state_->gpu_driver_bug_workarounds());
  // TODO(erikchen): The ContextGroup needs a reference to the
  // GpuMemoryBufferManager.
  scoped_refptr<gpu::gles2::ContextGroup> context_group =
      new gpu::gles2::ContextGroup(
          gpu_state_->gpu_preferences(), gpu_state_->mailbox_manager(),
          new GpuMemoryTracker,
          new gpu::gles2::ShaderTranslatorCache(gpu_state_->gpu_preferences()),
          new gpu::gles2::FramebufferCompletenessCache, feature_info,
          bind_generates_resource, nullptr);

  command_buffer_.reset(
      new gpu::CommandBufferService(context_group->transfer_buffer_manager()));

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group.get()));
  executor_.reset(new gpu::CommandExecutor(command_buffer_.get(),
                                           decoder_.get(), decoder_.get()));
  sync_point_order_data_ = gpu::SyncPointOrderData::Create();
  sync_point_client_ = gpu_state_->sync_point_manager()->CreateSyncPointClient(
      sync_point_order_data_, GetNamespaceID(), command_buffer_id_);
  decoder_->set_engine(executor_.get());
  decoder_->SetFenceSyncReleaseCallback(base::Bind(
      &CommandBufferDriver::OnFenceSyncRelease, base::Unretained(this)));
  decoder_->SetWaitFenceSyncCallback(base::Bind(
      &CommandBufferDriver::OnWaitFenceSync, base::Unretained(this)));
  decoder_->SetDescheduleUntilFinishedCallback(base::Bind(
      &CommandBufferDriver::OnDescheduleUntilFinished, base::Unretained(this)));
  decoder_->SetRescheduleAfterFinishedCallback(base::Bind(
      &CommandBufferDriver::OnRescheduleAfterFinished, base::Unretained(this)));

  gpu::gles2::DisallowedFeatures disallowed_features;

  if (!decoder_->Initialize(surface_, context_, offscreen, disallowed_features,
                            attrib_helper))
    return false;

  command_buffer_->SetPutOffsetChangeCallback(base::Bind(
      &gpu::CommandExecutor::PutChanged, base::Unretained(executor_.get())));
  command_buffer_->SetGetBufferChangeCallback(base::Bind(
      &gpu::CommandExecutor::SetGetBuffer, base::Unretained(executor_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&CommandBufferDriver::OnParseError, base::Unretained(this)));

  // TODO(piman): other callbacks

  const size_t kSize = sizeof(gpu::CommandBufferSharedState);
  std::unique_ptr<gpu::BufferBacking> backing(
      MojoBufferBacking::Create(std::move(shared_state), kSize));
  if (!backing)
    return false;

  command_buffer_->SetSharedStateBuffer(std::move(backing));
  gpu_state_->driver_manager()->AddDriver(this);
  return true;
}

void CommandBufferDriver::SetGetBuffer(int32_t buffer) {
  DCHECK(CalledOnValidThread());
  command_buffer_->SetGetBuffer(buffer);
}

void CommandBufferDriver::Flush(int32_t put_offset) {
  DCHECK(CalledOnValidThread());
  if (!MakeCurrent())
    return;

  command_buffer_->Flush(put_offset);
  ProcessPendingAndIdleWork();
}

void CommandBufferDriver::RegisterTransferBuffer(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  DCHECK(CalledOnValidThread());
  // Take ownership of the memory and map it into this process.
  // This validates the size.
  std::unique_ptr<gpu::BufferBacking> backing(
      MojoBufferBacking::Create(std::move(transfer_buffer), size));
  if (!backing) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }
  command_buffer_->RegisterTransferBuffer(id, std::move(backing));
}

void CommandBufferDriver::DestroyTransferBuffer(int32_t id) {
  DCHECK(CalledOnValidThread());
  command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferDriver::CreateImage(int32_t id,
                                      mojo::ScopedHandle memory_handle,
                                      int32_t type,
                                      const gfx::Size& size,
                                      int32_t format,
                                      int32_t internal_format) {
  DCHECK(CalledOnValidThread());
  if (!MakeCurrent())
    return;

  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  gfx::BufferFormat gpu_format = static_cast<gfx::BufferFormat>(format);
  if (!gpu::IsGpuMemoryBufferFormatSupported(gpu_format,
                                             decoder_->GetCapabilities())) {
    LOG(ERROR) << "Format is not supported.";
    return;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, gpu_format)) {
    LOG(ERROR) << "Invalid image size for format.";
    return;
  }

  if (!gpu::IsImageFormatCompatibleWithGpuMemoryBufferFormat(internal_format,
                                                             gpu_format)) {
    LOG(ERROR) << "Incompatible image format.";
    return;
  }

  if (type != gfx::SHARED_MEMORY_BUFFER) {
    NOTIMPLEMENTED();
    return;
  }

  base::PlatformFile platform_file;
  MojoResult unwrap_result = mojo::UnwrapPlatformFile(std::move(memory_handle),
                                                      &platform_file);
  if (unwrap_result != MOJO_RESULT_OK) {
    NOTREACHED();
    return;
  }

#if defined(OS_WIN)
  base::SharedMemoryHandle handle(platform_file, base::GetCurrentProcId());
#else
  base::FileDescriptor handle(platform_file, false);
#endif

  scoped_refptr<gl::GLImageSharedMemory> image =
      new gl::GLImageSharedMemory(size, internal_format);
  // TODO(jam): also need a mojo enum for this enum
  if (!image->Initialize(
          handle, gfx::GpuMemoryBufferId(id), gpu_format, 0,
          gfx::RowSizeForBufferFormat(size.width(), gpu_format, 0))) {
    NOTREACHED();
    return;
  }

  image_manager->AddImage(image.get(), id);
}

// TODO(rjkroege): It is conceivable that this code belongs in
// ozone_gpu_memory_buffer.cc
void CommandBufferDriver::CreateImageNativeOzone(int32_t id,
                                                 int32_t type,
                                                 gfx::Size size,
                                                 gfx::BufferFormat format,
                                                 uint32_t internal_format,
                                                 ui::NativePixmap* pixmap) {
#if defined(USE_OZONE)
  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  scoped_refptr<gl::GLImageOzoneNativePixmap> image =
      new gl::GLImageOzoneNativePixmap(size, internal_format);
  if (!image->Initialize(pixmap, format)) {
    NOTREACHED();
    return;
  }

  image_manager->AddImage(image.get(), id);
#endif
}

void CommandBufferDriver::DestroyImage(int32_t id) {
  DCHECK(CalledOnValidThread());
  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }
  if (!MakeCurrent())
    return;
  image_manager->RemoveImage(id);
}

bool CommandBufferDriver::IsScheduled() const {
  DCHECK(CalledOnValidThread());
  DCHECK(executor_);
  return executor_->scheduled();
}

bool CommandBufferDriver::HasUnprocessedCommands() const {
  DCHECK(CalledOnValidThread());
  if (command_buffer_) {
    gpu::CommandBuffer::State state = GetLastState();
    return command_buffer_->GetPutOffset() != state.get_offset &&
        !gpu::error::IsError(state.error);
  }
  return false;
}

gpu::Capabilities CommandBufferDriver::GetCapabilities() const {
  DCHECK(CalledOnValidThread());
  return decoder_->GetCapabilities();
}

gpu::CommandBuffer::State CommandBufferDriver::GetLastState() const {
  DCHECK(CalledOnValidThread());
  return command_buffer_->GetLastState();
}

uint32_t CommandBufferDriver::GetUnprocessedOrderNum() const {
  DCHECK(CalledOnValidThread());
  return sync_point_order_data_->unprocessed_order_num();
}

uint32_t CommandBufferDriver::GetProcessedOrderNum() const {
  DCHECK(CalledOnValidThread());
  return sync_point_order_data_->processed_order_num();
}

bool CommandBufferDriver::MakeCurrent() {
  DCHECK(CalledOnValidThread());
  if (!decoder_)
    return false;
  if (decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  gpu::error::ContextLostReason reason =
      static_cast<gpu::error::ContextLostReason>(
          decoder_->GetContextLostReason());
  command_buffer_->SetContextLostReason(reason);
  command_buffer_->SetParseError(gpu::error::kLostContext);
  OnContextLost(reason);
  return false;
}

void CommandBufferDriver::ProcessPendingAndIdleWork() {
  DCHECK(CalledOnValidThread());
  executor_->ProcessPendingQueries();
  ScheduleDelayedWork(
      base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodMs));
}

void CommandBufferDriver::ScheduleDelayedWork(base::TimeDelta delay) {
  DCHECK(CalledOnValidThread());
  const bool has_more_work =
      executor_->HasPendingQueries() || executor_->HasMoreIdleWork();
  if (!has_more_work) {
    last_idle_time_ = base::TimeTicks();
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  // |process_delayed_work_time_| is set if processing of delayed work is
  // already scheduled. Just update the time if already scheduled.
  if (!process_delayed_work_time_.is_null()) {
    process_delayed_work_time_ = current_time + delay;
    return;
  }

  // Idle when no messages are processed between now and when PollWork is
  // called.
  previous_processed_num_ =
      gpu_state_->driver_manager()->GetProcessedOrderNum();

  if (last_idle_time_.is_null())
    last_idle_time_ = current_time;

    // scheduled() returns true after passing all unschedule fences and this is
    // when we can start performing idle work. Idle work is done synchronously
    // so we can set delay to 0 and instead poll for more work at the rate idle
    // work is performed. This also ensures that idle work is done as
    // efficiently as possible without any unnecessary delays.
  if (executor_->scheduled() && executor_->HasMoreIdleWork())
    delay = base::TimeDelta();

  process_delayed_work_time_ = current_time + delay;
  gpu_state_->command_buffer_task_runner()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CommandBufferDriver::PollWork, weak_factory_.GetWeakPtr()),
      delay);
}

void CommandBufferDriver::PollWork() {
  DCHECK(CalledOnValidThread());
  // Post another delayed task if we have not yet reached the time at which
  // we should process delayed work.
  base::TimeTicks current_time = base::TimeTicks::Now();
  DCHECK(!process_delayed_work_time_.is_null());
  if (process_delayed_work_time_ > current_time) {
    gpu_state_->command_buffer_task_runner()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CommandBufferDriver::PollWork, weak_factory_.GetWeakPtr()),
      process_delayed_work_time_ - current_time);
    return;
  }
  process_delayed_work_time_ = base::TimeTicks();
  PerformWork();
}

void CommandBufferDriver::PerformWork() {
  DCHECK(CalledOnValidThread());
  if (!MakeCurrent())
    return;

  if (executor_) {
    const uint32_t current_unprocessed_num =
        gpu_state_->driver_manager()->GetUnprocessedOrderNum();
    // We're idle when no messages were processed or scheduled.
    bool is_idle = (previous_processed_num_ == current_unprocessed_num);
    if (!is_idle && !last_idle_time_.is_null()) {
      base::TimeDelta time_since_idle =
          base::TimeTicks::Now() - last_idle_time_;
      base::TimeDelta max_time_since_idle =
          base::TimeDelta::FromMilliseconds(kMaxTimeSinceIdleMs);
      // Force idle when it's been too long since last time we were idle.
      if (time_since_idle > max_time_since_idle)
        is_idle = true;
    }

    if (is_idle) {
      last_idle_time_ = base::TimeTicks::Now();
      executor_->PerformIdleWork();
    }
    executor_->ProcessPendingQueries();
  }

  ScheduleDelayedWork(
      base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodBusyMs));
}

void CommandBufferDriver::DestroyDecoder() {
  DCHECK(CalledOnValidThread());
  if (decoder_) {
    gpu_state_->driver_manager()->RemoveDriver(this);
    bool have_context = decoder_->MakeCurrent();
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
}

void CommandBufferDriver::OnUpdateVSyncParameters(
    const base::TimeTicks timebase,
    const base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->UpdateVSyncParameters(timebase, interval);
}

void CommandBufferDriver::OnFenceSyncRelease(uint64_t release) {
  DCHECK(CalledOnValidThread());
  if (!sync_point_client_->client_state()->IsFenceSyncReleased(release))
    sync_point_client_->ReleaseFenceSync(release);
}

bool CommandBufferDriver::OnWaitFenceSync(
    gpu::CommandBufferNamespace namespace_id,
    gpu::CommandBufferId command_buffer_id,
    uint64_t release) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsScheduled());
  gpu::SyncPointManager* sync_point_manager = gpu_state_->sync_point_manager();
  DCHECK(sync_point_manager);

  scoped_refptr<gpu::SyncPointClientState> release_state =
      sync_point_manager->GetSyncPointClientState(namespace_id,
                                                  command_buffer_id);

  if (!release_state)
    return true;

  executor_->SetScheduled(false);
  sync_point_client_->Wait(release_state.get(), release,
                           base::Bind(&gpu::CommandExecutor::SetScheduled,
                                      executor_->AsWeakPtr(), true));
  return executor_->scheduled();
}

void CommandBufferDriver::OnDescheduleUntilFinished() {
  DCHECK(CalledOnValidThread());
  DCHECK(IsScheduled());
  DCHECK(executor_->HasMoreIdleWork());

  executor_->SetScheduled(false);
}

void CommandBufferDriver::OnRescheduleAfterFinished() {
  DCHECK(CalledOnValidThread());
  DCHECK(!executor_->scheduled());

  executor_->SetScheduled(true);
}

void CommandBufferDriver::OnParseError() {
  DCHECK(CalledOnValidThread());
  gpu::CommandBuffer::State state = GetLastState();
  OnContextLost(state.context_lost_reason);
}

void CommandBufferDriver::OnContextLost(uint32_t reason) {
  DCHECK(CalledOnValidThread());
  if (client_)
    client_->DidLoseContext(reason);
}

void CommandBufferDriver::SignalQuery(uint32_t query_id,
                                      const base::Closure& callback) {
  DCHECK(CalledOnValidThread());

  gpu::gles2::QueryManager* query_manager = decoder_->GetQueryManager();
  gpu::gles2::QueryManager::Query* query = query_manager->GetQuery(query_id);
  if (query)
    query->AddCallback(callback);
  else
    callback.Run();
}

void CommandBufferDriver::OnGpuCompletedSwapBuffers(gfx::SwapResult result) {
  DCHECK(CalledOnValidThread());
  if (client_) {
    client_->OnGpuCompletedSwapBuffers(result);
  }
}

}  // namespace mus
