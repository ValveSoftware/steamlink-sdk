// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/in_process_command_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <queue>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/command_executor.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_program_cache.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/process/process_handle.h"
#endif

namespace gpu {

namespace {

base::StaticAtomicSequenceNumber g_next_command_buffer_id;

template <typename T>
static void RunTaskWithResult(base::Callback<T(void)> task,
                              T* result,
                              base::WaitableEvent* completion) {
  *result = task.Run();
  completion->Signal();
}

struct ScopedOrderNumberProcessor {
  ScopedOrderNumberProcessor(SyncPointOrderData* order_data, uint32_t order_num)
      : order_data_(order_data), order_num_(order_num) {
    order_data_->BeginProcessingOrderNumber(order_num_);
  }

  ~ScopedOrderNumberProcessor() {
    order_data_->FinishProcessingOrderNumber(order_num_);
  }

 private:
  SyncPointOrderData* order_data_;
  uint32_t order_num_;
};

struct GpuInProcessThreadHolder {
  GpuInProcessThreadHolder()
      : sync_point_manager(new SyncPointManager(false)),
        gpu_thread(new GpuInProcessThread(sync_point_manager.get())) {}
  std::unique_ptr<SyncPointManager> sync_point_manager;
  scoped_refptr<InProcessCommandBuffer::Service> gpu_thread;
};

base::LazyInstance<GpuInProcessThreadHolder> g_default_service =
    LAZY_INSTANCE_INITIALIZER;

class ScopedEvent {
 public:
  explicit ScopedEvent(base::WaitableEvent* event) : event_(event) {}
  ~ScopedEvent() { event_->Signal(); }

 private:
  base::WaitableEvent* event_;
};

base::SharedMemoryHandle ShareToGpuThread(
    base::SharedMemoryHandle source_handle) {
  return base::SharedMemory::DuplicateHandle(source_handle);
}

gfx::GpuMemoryBufferHandle ShareGpuMemoryBufferToGpuThread(
    const gfx::GpuMemoryBufferHandle& source_handle,
    bool* requires_sync_point) {
  switch (source_handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      gfx::GpuMemoryBufferHandle handle;
      handle.type = gfx::SHARED_MEMORY_BUFFER;
      handle.handle = ShareToGpuThread(source_handle.handle);
      handle.offset = source_handle.offset;
      handle.stride = source_handle.stride;
      *requires_sync_point = false;
      return handle;
    }
    case gfx::IO_SURFACE_BUFFER:
    case gfx::SURFACE_TEXTURE_BUFFER:
    case gfx::OZONE_NATIVE_PIXMAP:
      *requires_sync_point = true;
      return source_handle;
    default:
      NOTREACHED();
      return gfx::GpuMemoryBufferHandle();
  }
}

scoped_refptr<InProcessCommandBuffer::Service> GetInitialService(
    const scoped_refptr<InProcessCommandBuffer::Service>& service) {
  if (service)
    return service;

  // Call base::ThreadTaskRunnerHandle::IsSet() to ensure that it is
  // instantiated before we create the GPU thread, otherwise shutdown order will
  // delete the ThreadTaskRunnerHandle before the GPU thread's message loop,
  // and when the message loop is shutdown, it will recreate
  // ThreadTaskRunnerHandle, which will re-add a new task to the, AtExitManager,
  // which causes a deadlock because it's already locked.
  base::ThreadTaskRunnerHandle::IsSet();
  return g_default_service.Get().gpu_thread;
}

}  // anonyous namespace

InProcessCommandBuffer::Service::Service()
    : gpu_driver_bug_workarounds_(base::CommandLine::ForCurrentProcess()) {}

InProcessCommandBuffer::Service::Service(const GpuPreferences& gpu_preferences)
    : gpu_preferences_(gpu_preferences),
      gpu_driver_bug_workarounds_(base::CommandLine::ForCurrentProcess()) {}

InProcessCommandBuffer::Service::~Service() {}

const gpu::GpuPreferences&
InProcessCommandBuffer::Service::gpu_preferences() {
  return gpu_preferences_;
}

const gpu::GpuDriverBugWorkarounds&
InProcessCommandBuffer::Service::gpu_driver_bug_workarounds() {
  return gpu_driver_bug_workarounds_;
}

scoped_refptr<gl::GLShareGroup> InProcessCommandBuffer::Service::share_group() {
  if (!share_group_.get())
    share_group_ = new gl::GLShareGroup;
  return share_group_;
}

scoped_refptr<gles2::MailboxManager>
InProcessCommandBuffer::Service::mailbox_manager() {
  if (!mailbox_manager_.get()) {
    mailbox_manager_ = gles2::MailboxManager::Create(gpu_preferences());
  }
  return mailbox_manager_;
}

gpu::gles2::ProgramCache* InProcessCommandBuffer::Service::program_cache() {
  if (!program_cache_.get() &&
      (gl::g_driver_gl.ext.b_GL_ARB_get_program_binary ||
       gl::g_driver_gl.ext.b_GL_OES_get_program_binary) &&
      !gpu_preferences().disable_gpu_program_cache) {
    program_cache_.reset(new gpu::gles2::MemoryProgramCache(
          gpu_preferences().gpu_program_cache_size,
          gpu_preferences().disable_gpu_shader_disk_cache));
  }
  return program_cache_.get();
}

InProcessCommandBuffer::InProcessCommandBuffer(
    const scoped_refptr<Service>& service)
    : command_buffer_id_(
          CommandBufferId::FromUnsafeValue(g_next_command_buffer_id.GetNext())),
      delayed_work_pending_(false),
      image_factory_(nullptr),
      gpu_control_client_(nullptr),
#if DCHECK_IS_ON()
      context_lost_(false),
#endif
      last_put_offset_(-1),
      gpu_memory_buffer_manager_(nullptr),
      next_fence_sync_release_(1),
      flushed_fence_sync_release_(0),
      flush_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      service_(GetInitialService(service)),
      fence_sync_wait_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),
      client_thread_weak_ptr_factory_(this),
      gpu_thread_weak_ptr_factory_(this) {
  DCHECK(service_.get());
  next_image_id_.GetNext();
}

InProcessCommandBuffer::~InProcessCommandBuffer() {
  Destroy();
}

bool InProcessCommandBuffer::MakeCurrent() {
  CheckSequencedThread();
  command_buffer_lock_.AssertAcquired();

  if (error::IsError(command_buffer_->GetLastState().error)) {
    DLOG(ERROR) << "MakeCurrent failed because context lost.";
    return false;
  }
  if (!decoder_->MakeCurrent()) {
    DLOG(ERROR) << "Context lost because MakeCurrent failed.";
    command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
    command_buffer_->SetParseError(gpu::error::kLostContext);
    return false;
  }
  return true;
}

void InProcessCommandBuffer::PumpCommandsOnGpuThread() {
  CheckSequencedThread();
  command_buffer_lock_.AssertAcquired();

  if (!MakeCurrent())
    return;

  executor_->PutChanged();
}

bool InProcessCommandBuffer::Initialize(
    scoped_refptr<gl::GLSurface> surface,
    bool is_offscreen,
    gfx::AcceleratedWidget window,
    const gles2::ContextCreationAttribHelper& attribs,
    InProcessCommandBuffer* share_group,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory) {
  DCHECK(!share_group || service_.get() == share_group->service_.get());

  if (surface) {
    // GPU thread must be the same as client thread due to GLSurface not being
    // thread safe.
    sequence_checker_.reset(new base::SequenceChecker);
    surface_ = surface;
  } else {
    origin_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    client_thread_weak_ptr_ = client_thread_weak_ptr_factory_.GetWeakPtr();
  }

  gpu::Capabilities capabilities;
  InitializeOnGpuThreadParams params(is_offscreen, window, attribs,
                                     &capabilities, share_group, image_factory);

  base::Callback<bool(void)> init_task =
      base::Bind(&InProcessCommandBuffer::InitializeOnGpuThread,
                 base::Unretained(this), params);

  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result = false;
  QueueTask(
      base::Bind(&RunTaskWithResult<bool>, init_task, &result, &completion));
  completion.Wait();

  gpu_memory_buffer_manager_ = gpu_memory_buffer_manager;

  if (result) {
    capabilities_ = capabilities;
    capabilities_.image = capabilities_.image && gpu_memory_buffer_manager_;
  }

  return result;
}

bool InProcessCommandBuffer::InitializeOnGpuThread(
    const InitializeOnGpuThreadParams& params) {
  CheckSequencedThread();
  gpu_thread_weak_ptr_ = gpu_thread_weak_ptr_factory_.GetWeakPtr();

  TransferBufferManager* manager = new TransferBufferManager(nullptr);
  transfer_buffer_manager_ = manager;
  manager->Initialize();

  std::unique_ptr<CommandBufferService> command_buffer(
      new CommandBufferService(transfer_buffer_manager_.get()));
  command_buffer->SetPutOffsetChangeCallback(base::Bind(
      &InProcessCommandBuffer::PumpCommandsOnGpuThread, gpu_thread_weak_ptr_));
  command_buffer->SetParseErrorCallback(base::Bind(
      &InProcessCommandBuffer::OnContextLostOnGpuThread, gpu_thread_weak_ptr_));

  gl_share_group_ = params.context_group
                        ? params.context_group->gl_share_group_
                        : service_->share_group();

  bool bind_generates_resource = false;
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(service_->gpu_driver_bug_workarounds());
  decoder_.reset(gles2::GLES2Decoder::Create(
      params.context_group
          ? params.context_group->decoder_->GetContextGroup()
          : new gles2::ContextGroup(
                service_->gpu_preferences(), service_->mailbox_manager(), NULL,
                service_->shader_translator_cache(),
                service_->framebuffer_completeness_cache(), feature_info,
                bind_generates_resource, nullptr)));

  executor_.reset(new CommandExecutor(command_buffer.get(), decoder_.get(),
                                      decoder_.get()));
  command_buffer->SetGetBufferChangeCallback(base::Bind(
      &CommandExecutor::SetGetBuffer, base::Unretained(executor_.get())));
  command_buffer_ = std::move(command_buffer);

  decoder_->set_engine(executor_.get());

  if (!surface_.get()) {
    if (params.is_offscreen)
      surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    else
      surface_ = gl::init::CreateViewGLSurface(params.window);
  }

  if (!surface_.get()) {
    LOG(ERROR) << "Could not create GLSurface.";
    DestroyOnGpuThread();
    return false;
  }

  sync_point_order_data_ = SyncPointOrderData::Create();
  sync_point_client_ = service_->sync_point_manager()->CreateSyncPointClient(
      sync_point_order_data_, GetNamespaceID(), GetCommandBufferID());

  if (service_->UseVirtualizedGLContexts() ||
      decoder_->GetContextGroup()
          ->feature_info()
          ->workarounds()
          .use_virtualized_gl_contexts) {
    context_ = gl_share_group_->GetSharedContext();
    if (!context_.get()) {
      context_ = gl::init::CreateGLContext(
          gl_share_group_.get(), surface_.get(), params.attribs.gpu_preference);
      gl_share_group_->SetSharedContext(context_.get());
    }

    context_ = new GLContextVirtual(
        gl_share_group_.get(), context_.get(), decoder_->AsWeakPtr());
    if (context_->Initialize(surface_.get(), params.attribs.gpu_preference)) {
      VLOG(1) << "Created virtual GL context.";
    } else {
      context_ = NULL;
    }
  } else {
    context_ = gl::init::CreateGLContext(gl_share_group_.get(), surface_.get(),
                                         params.attribs.gpu_preference);
  }

  if (!context_.get()) {
    LOG(ERROR) << "Could not create GLContext.";
    DestroyOnGpuThread();
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Could not make context current.";
    DestroyOnGpuThread();
    return false;
  }

  if (!decoder_->GetContextGroup()->has_program_cache() &&
      !decoder_->GetContextGroup()
           ->feature_info()
           ->workarounds()
           .disable_program_cache) {
    decoder_->GetContextGroup()->set_program_cache(service_->program_cache());
  }

  gles2::DisallowedFeatures disallowed_features;
  disallowed_features.gpu_memory_manager = true;
  if (!decoder_->Initialize(surface_,
                            context_,
                            params.is_offscreen,
                            disallowed_features,
                            params.attribs)) {
    LOG(ERROR) << "Could not initialize decoder.";
    DestroyOnGpuThread();
    return false;
  }
  *params.capabilities = decoder_->GetCapabilities();

  decoder_->SetFenceSyncReleaseCallback(
      base::Bind(&InProcessCommandBuffer::FenceSyncReleaseOnGpuThread,
                 base::Unretained(this)));
  decoder_->SetWaitFenceSyncCallback(
      base::Bind(&InProcessCommandBuffer::WaitFenceSyncOnGpuThread,
                 base::Unretained(this)));
  decoder_->SetDescheduleUntilFinishedCallback(
      base::Bind(&InProcessCommandBuffer::DescheduleUntilFinishedOnGpuThread,
                 base::Unretained(this)));
  decoder_->SetRescheduleAfterFinishedCallback(
      base::Bind(&InProcessCommandBuffer::RescheduleAfterFinishedOnGpuThread,
                 base::Unretained(this)));

  image_factory_ = params.image_factory;

  return true;
}

void InProcessCommandBuffer::Destroy() {
  CheckSequencedThread();
  client_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  gpu_control_client_ = nullptr;
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result = false;
  base::Callback<bool(void)> destroy_task = base::Bind(
      &InProcessCommandBuffer::DestroyOnGpuThread, base::Unretained(this));
  QueueTask(
      base::Bind(&RunTaskWithResult<bool>, destroy_task, &result, &completion));
  completion.Wait();
}

bool InProcessCommandBuffer::DestroyOnGpuThread() {
  CheckSequencedThread();
  gpu_thread_weak_ptr_factory_.InvalidateWeakPtrs();
  command_buffer_.reset();
  // Clean up GL resources if possible.
  bool have_context = context_.get() && context_->MakeCurrent(surface_.get());
  if (decoder_) {
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
  context_ = nullptr;
  surface_ = nullptr;
  sync_point_client_ = nullptr;
  if (sync_point_order_data_) {
    sync_point_order_data_->Destroy();
    sync_point_order_data_ = nullptr;
  }
  gl_share_group_ = nullptr;

  return true;
}

void InProcessCommandBuffer::CheckSequencedThread() {
  DCHECK(!sequence_checker_ ||
         sequence_checker_->CalledOnValidSequencedThread());
}

void InProcessCommandBuffer::OnContextLostOnGpuThread() {
  if (!origin_task_runner_)
    return OnContextLost();  // Just kidding, we're on the client thread.
  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&InProcessCommandBuffer::OnContextLost,
                            client_thread_weak_ptr_));
}

void InProcessCommandBuffer::OnContextLost() {
  CheckSequencedThread();

#if DCHECK_IS_ON()
  // This method shouldn't be called more than once.
  DCHECK(!context_lost_);
  context_lost_ = true;
#endif

  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContext();
}

CommandBuffer::State InProcessCommandBuffer::GetStateFast() {
  CheckSequencedThread();
  base::AutoLock lock(state_after_last_flush_lock_);
  if (state_after_last_flush_.generation - last_state_.generation < 0x80000000U)
    last_state_ = state_after_last_flush_;
  return last_state_;
}

CommandBuffer::State InProcessCommandBuffer::GetLastState() {
  CheckSequencedThread();
  return last_state_;
}

int32_t InProcessCommandBuffer::GetLastToken() {
  CheckSequencedThread();
  GetStateFast();
  return last_state_.token;
}

void InProcessCommandBuffer::FlushOnGpuThread(int32_t put_offset,
                                              uint32_t order_num) {
  CheckSequencedThread();
  ScopedEvent handle_flush(&flush_event_);
  base::AutoLock lock(command_buffer_lock_);

  {
    ScopedOrderNumberProcessor scoped_order_num(sync_point_order_data_.get(),
                                                order_num);
    command_buffer_->Flush(put_offset);
    {
      // Update state before signaling the flush event.
      base::AutoLock lock(state_after_last_flush_lock_);
      state_after_last_flush_ = command_buffer_->GetLastState();
    }

    // Currently the in process command buffer does not support being
    // descheduled, if it does we would need to back off on calling the finish
    // processing number function until the message is rescheduled and finished
    // processing. This DCHECK is to enforce this.
    DCHECK(error::IsError(state_after_last_flush_.error) ||
           put_offset == state_after_last_flush_.get_offset);
  }

  // If we've processed all pending commands but still have pending queries,
  // pump idle work until the query is passed.
  if (put_offset == state_after_last_flush_.get_offset &&
      (executor_->HasMoreIdleWork() || executor_->HasPendingQueries())) {
    ScheduleDelayedWorkOnGpuThread();
  }
}

void InProcessCommandBuffer::PerformDelayedWorkOnGpuThread() {
  CheckSequencedThread();
  delayed_work_pending_ = false;
  base::AutoLock lock(command_buffer_lock_);
  if (MakeCurrent()) {
    executor_->PerformIdleWork();
    executor_->ProcessPendingQueries();
    if (executor_->HasMoreIdleWork() || executor_->HasPendingQueries()) {
      ScheduleDelayedWorkOnGpuThread();
    }
  }
}

void InProcessCommandBuffer::ScheduleDelayedWorkOnGpuThread() {
  CheckSequencedThread();
  if (delayed_work_pending_)
    return;
  delayed_work_pending_ = true;
  service_->ScheduleDelayedWork(
      base::Bind(&InProcessCommandBuffer::PerformDelayedWorkOnGpuThread,
                 gpu_thread_weak_ptr_));
}

void InProcessCommandBuffer::Flush(int32_t put_offset) {
  CheckSequencedThread();
  if (last_state_.error != gpu::error::kNoError)
    return;

  if (last_put_offset_ == put_offset)
    return;

  SyncPointManager* sync_manager = service_->sync_point_manager();
  const uint32_t order_num =
      sync_point_order_data_->GenerateUnprocessedOrderNumber(sync_manager);
  last_put_offset_ = put_offset;
  base::Closure task = base::Bind(&InProcessCommandBuffer::FlushOnGpuThread,
                                  gpu_thread_weak_ptr_,
                                  put_offset,
                                  order_num);
  QueueTask(task);

  flushed_fence_sync_release_ = next_fence_sync_release_ - 1;
}

void InProcessCommandBuffer::OrderingBarrier(int32_t put_offset) {
  Flush(put_offset);
}

void InProcessCommandBuffer::WaitForTokenInRange(int32_t start, int32_t end) {
  CheckSequencedThread();
  while (!InRange(start, end, GetLastToken()) &&
         last_state_.error == gpu::error::kNoError)
    flush_event_.Wait();
}

void InProcessCommandBuffer::WaitForGetOffsetInRange(int32_t start,
                                                     int32_t end) {
  CheckSequencedThread();

  GetStateFast();
  while (!InRange(start, end, last_state_.get_offset) &&
         last_state_.error == gpu::error::kNoError) {
    flush_event_.Wait();
    GetStateFast();
  }
}

void InProcessCommandBuffer::SetGetBuffer(int32_t shm_id) {
  CheckSequencedThread();
  if (last_state_.error != gpu::error::kNoError)
    return;

  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::Closure task =
      base::Bind(&InProcessCommandBuffer::SetGetBufferOnGpuThread,
                 base::Unretained(this), shm_id, &completion);
  QueueTask(task);
  completion.Wait();

  {
    base::AutoLock lock(state_after_last_flush_lock_);
    state_after_last_flush_ = command_buffer_->GetLastState();
  }
}

void InProcessCommandBuffer::SetGetBufferOnGpuThread(
    int32_t shm_id,
    base::WaitableEvent* completion) {
  base::AutoLock lock(command_buffer_lock_);
  command_buffer_->SetGetBuffer(shm_id);
  last_put_offset_ = 0;
  completion->Signal();
}

scoped_refptr<Buffer> InProcessCommandBuffer::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  CheckSequencedThread();
  base::AutoLock lock(command_buffer_lock_);
  return command_buffer_->CreateTransferBuffer(size, id);
}

void InProcessCommandBuffer::DestroyTransferBuffer(int32_t id) {
  CheckSequencedThread();
  base::Closure task =
      base::Bind(&InProcessCommandBuffer::DestroyTransferBufferOnGpuThread,
                 base::Unretained(this),
                 id);

  QueueTask(task);
}

void InProcessCommandBuffer::DestroyTransferBufferOnGpuThread(int32_t id) {
  base::AutoLock lock(command_buffer_lock_);
  command_buffer_->DestroyTransferBuffer(id);
}

void InProcessCommandBuffer::SetGpuControlClient(GpuControlClient* client) {
  gpu_control_client_ = client;
}

gpu::Capabilities InProcessCommandBuffer::GetCapabilities() {
  return capabilities_;
}

int32_t InProcessCommandBuffer::CreateImage(ClientBuffer buffer,
                                            size_t width,
                                            size_t height,
                                            unsigned internalformat) {
  CheckSequencedThread();

  DCHECK(gpu_memory_buffer_manager_);
  gfx::GpuMemoryBuffer* gpu_memory_buffer =
      gpu_memory_buffer_manager_->GpuMemoryBufferFromClientBuffer(buffer);
  DCHECK(gpu_memory_buffer);

  int32_t new_id = next_image_id_.GetNext();

  DCHECK(gpu::IsGpuMemoryBufferFormatSupported(gpu_memory_buffer->GetFormat(),
                                               capabilities_));
  DCHECK(gpu::IsImageFormatCompatibleWithGpuMemoryBufferFormat(
      internalformat, gpu_memory_buffer->GetFormat()));

  DCHECK(image_gmb_ids_map_.find(new_id) == image_gmb_ids_map_.end());
  image_gmb_ids_map_[new_id] = gpu_memory_buffer->GetId().id;

  // This handle is owned by the GPU thread and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // queuing of the CreateImage task below.
  bool requires_sync_point = false;
  gfx::GpuMemoryBufferHandle handle =
      ShareGpuMemoryBufferToGpuThread(gpu_memory_buffer->GetHandle(),
                                      &requires_sync_point);

  SyncPointManager* sync_manager = service_->sync_point_manager();
  const uint32_t order_num =
      sync_point_order_data_->GenerateUnprocessedOrderNumber(sync_manager);

  uint64_t fence_sync = 0;
  if (requires_sync_point) {
    fence_sync = GenerateFenceSyncRelease();

    // Previous fence syncs should be flushed already.
    DCHECK_EQ(fence_sync - 1, flushed_fence_sync_release_);
  }

  QueueTask(base::Bind(&InProcessCommandBuffer::CreateImageOnGpuThread,
                       base::Unretained(this), new_id, handle,
                       gfx::Size(width, height), gpu_memory_buffer->GetFormat(),
                       internalformat, order_num, fence_sync));

  if (fence_sync) {
    flushed_fence_sync_release_ = fence_sync;
    SyncToken sync_token(GetNamespaceID(), GetExtraCommandBufferData(),
                         GetCommandBufferID(), fence_sync);
    sync_token.SetVerifyFlush();
    gpu_memory_buffer_manager_->SetDestructionSyncToken(gpu_memory_buffer,
                                                        sync_token);
  }

  return new_id;
}

void InProcessCommandBuffer::CreateImageOnGpuThread(
    int32_t id,
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint32_t internalformat,
    uint32_t order_num,
    uint64_t fence_sync) {
  ScopedOrderNumberProcessor scoped_order_num(sync_point_order_data_.get(),
                                              order_num);
  if (!decoder_)
    return;

  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      if (!base::IsValueInRangeForNumericType<size_t>(handle.stride)) {
        LOG(ERROR) << "Invalid stride for image.";
        return;
      }
      scoped_refptr<gl::GLImageSharedMemory> image(
          new gl::GLImageSharedMemory(size, internalformat));
      if (!image->Initialize(handle.handle, handle.id, format, handle.offset,
                             handle.stride)) {
        LOG(ERROR) << "Failed to initialize image.";
        return;
      }

      image_manager->AddImage(image.get(), id);
      break;
    }
    default: {
      if (!image_factory_) {
        LOG(ERROR) << "Image factory missing but required by buffer type.";
        return;
      }

      // Note: this assumes that client ID is always 0.
      const int kClientId = 0;

      scoped_refptr<gl::GLImage> image =
          image_factory_->CreateImageForGpuMemoryBuffer(
              handle, size, format, internalformat, kClientId,
              kNullSurfaceHandle);
      if (!image.get()) {
        LOG(ERROR) << "Failed to create image for buffer.";
        return;
      }

      image_manager->AddImage(image.get(), id);
      break;
    }
  }

  if (fence_sync) {
    sync_point_client_->ReleaseFenceSync(fence_sync);
  }
}

void InProcessCommandBuffer::DestroyImage(int32_t id) {
  CheckSequencedThread();

  auto it = image_gmb_ids_map_.find(id);
  if (it != image_gmb_ids_map_.end())
    image_gmb_ids_map_.erase(it);

  QueueTask(base::Bind(&InProcessCommandBuffer::DestroyImageOnGpuThread,
                       base::Unretained(this),
                       id));
}

void InProcessCommandBuffer::DestroyImageOnGpuThread(int32_t id) {
  if (!decoder_)
    return;

  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }

  image_manager->RemoveImage(id);
}

int32_t InProcessCommandBuffer::CreateGpuMemoryBufferImage(
    size_t width,
    size_t height,
    unsigned internalformat,
    unsigned usage) {
  CheckSequencedThread();

  DCHECK(gpu_memory_buffer_manager_);
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(
      gpu_memory_buffer_manager_->AllocateGpuMemoryBuffer(
          gfx::Size(width, height),
          gpu::DefaultBufferFormatForImageFormat(internalformat),
          gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle));
  if (!buffer)
    return -1;

  return CreateImage(buffer->AsClientBuffer(), width, height, internalformat);
}

int32_t InProcessCommandBuffer::GetImageGpuMemoryBufferId(unsigned image_id) {
  CheckSequencedThread();
  auto it = image_gmb_ids_map_.find(image_id);
  if (it != image_gmb_ids_map_.end())
    return it->second;
  return -1;
}

void InProcessCommandBuffer::FenceSyncReleaseOnGpuThread(uint64_t release) {
  DCHECK(!sync_point_client_->client_state()->IsFenceSyncReleased(release));
  gles2::MailboxManager* mailbox_manager =
      decoder_->GetContextGroup()->mailbox_manager();
  if (mailbox_manager->UsesSync()) {
    SyncToken sync_token(GetNamespaceID(), GetExtraCommandBufferData(),
                         GetCommandBufferID(), release);
    mailbox_manager->PushTextureUpdates(sync_token);
  }

  sync_point_client_->ReleaseFenceSync(release);
}

bool InProcessCommandBuffer::WaitFenceSyncOnGpuThread(
    gpu::CommandBufferNamespace namespace_id,
    gpu::CommandBufferId command_buffer_id,
    uint64_t release) {
  gpu::SyncPointManager* sync_point_manager = service_->sync_point_manager();
  DCHECK(sync_point_manager);

  scoped_refptr<gpu::SyncPointClientState> release_state =
      sync_point_manager->GetSyncPointClientState(namespace_id,
                                                  command_buffer_id);

  if (!release_state)
    return true;

  if (!release_state->IsFenceSyncReleased(release)) {
    // Use waitable event which is signalled when the release fence is released.
    sync_point_client_->Wait(
        release_state.get(), release,
        base::Bind(&base::WaitableEvent::Signal,
                   base::Unretained(&fence_sync_wait_event_)));
    fence_sync_wait_event_.Wait();
  }

  gles2::MailboxManager* mailbox_manager =
      decoder_->GetContextGroup()->mailbox_manager();
  SyncToken sync_token(namespace_id, 0, command_buffer_id, release);
  mailbox_manager->PullTextureUpdates(sync_token);
  return true;
}

void InProcessCommandBuffer::DescheduleUntilFinishedOnGpuThread() {
  NOTIMPLEMENTED();
}

void InProcessCommandBuffer::RescheduleAfterFinishedOnGpuThread() {
  NOTIMPLEMENTED();
}

void InProcessCommandBuffer::SignalSyncTokenOnGpuThread(
    const SyncToken& sync_token, const base::Closure& callback) {
  gpu::SyncPointManager* sync_point_manager = service_->sync_point_manager();
  DCHECK(sync_point_manager);

  scoped_refptr<gpu::SyncPointClientState> release_state =
      sync_point_manager->GetSyncPointClientState(
          sync_token.namespace_id(), sync_token.command_buffer_id());

  if (!release_state) {
    callback.Run();
    return;
  }

  sync_point_client_->WaitOutOfOrder(
      release_state.get(), sync_token.release_count(), WrapCallback(callback));
}

void InProcessCommandBuffer::SignalQuery(unsigned query_id,
                                         const base::Closure& callback) {
  CheckSequencedThread();
  QueueTask(base::Bind(&InProcessCommandBuffer::SignalQueryOnGpuThread,
                       base::Unretained(this),
                       query_id,
                       WrapCallback(callback)));
}

void InProcessCommandBuffer::SignalQueryOnGpuThread(
    unsigned query_id,
    const base::Closure& callback) {
  gles2::QueryManager* query_manager_ = decoder_->GetQueryManager();
  DCHECK(query_manager_);

  gles2::QueryManager::Query* query = query_manager_->GetQuery(query_id);
  if (!query)
    callback.Run();
  else
    query->AddCallback(callback);
}

void InProcessCommandBuffer::SetLock(base::Lock*) {
  // No support for using on multiple threads.
  NOTREACHED();
}

void InProcessCommandBuffer::EnsureWorkVisible() {
  // This is only relevant for out-of-process command buffers.
}

CommandBufferNamespace InProcessCommandBuffer::GetNamespaceID() const {
  return CommandBufferNamespace::IN_PROCESS;
}

CommandBufferId InProcessCommandBuffer::GetCommandBufferID() const {
  return command_buffer_id_;
}

int32_t InProcessCommandBuffer::GetExtraCommandBufferData() const {
  return 0;
}

uint64_t InProcessCommandBuffer::GenerateFenceSyncRelease() {
  return next_fence_sync_release_++;
}

bool InProcessCommandBuffer::IsFenceSyncRelease(uint64_t release) {
  return release != 0 && release < next_fence_sync_release_;
}

bool InProcessCommandBuffer::IsFenceSyncFlushed(uint64_t release) {
  return release <= flushed_fence_sync_release_;
}

bool InProcessCommandBuffer::IsFenceSyncFlushReceived(uint64_t release) {
  return IsFenceSyncFlushed(release);
}

void InProcessCommandBuffer::SignalSyncToken(const SyncToken& sync_token,
                                             const base::Closure& callback) {
  CheckSequencedThread();
  QueueTask(base::Bind(&InProcessCommandBuffer::SignalSyncTokenOnGpuThread,
                       base::Unretained(this),
                       sync_token,
                       WrapCallback(callback)));
}

bool InProcessCommandBuffer::CanWaitUnverifiedSyncToken(
    const SyncToken* sync_token) {
  return sync_token->namespace_id() == GetNamespaceID();
}

gpu::error::Error InProcessCommandBuffer::GetLastError() {
  CheckSequencedThread();
  return last_state_.error;
}

namespace {

void PostCallback(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Closure& callback) {
  // The task_runner.get() check is to support using InProcessCommandBuffer on
  // a thread without a message loop.
  if (task_runner.get() && !task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(FROM_HERE, callback);
  } else {
    callback.Run();
  }
}

void RunOnTargetThread(std::unique_ptr<base::Closure> callback) {
  DCHECK(callback.get());
  callback->Run();
}

}  // anonymous namespace

base::Closure InProcessCommandBuffer::WrapCallback(
    const base::Closure& callback) {
  // Make sure the callback gets deleted on the target thread by passing
  // ownership.
  std::unique_ptr<base::Closure> scoped_callback(new base::Closure(callback));
  base::Closure callback_on_client_thread =
      base::Bind(&RunOnTargetThread, base::Passed(&scoped_callback));
  base::Closure wrapped_callback =
      base::Bind(&PostCallback, base::ThreadTaskRunnerHandle::IsSet()
                                    ? base::ThreadTaskRunnerHandle::Get()
                                    : nullptr,
                 callback_on_client_thread);
  return wrapped_callback;
}

GpuInProcessThread::GpuInProcessThread(SyncPointManager* sync_point_manager)
    : base::Thread("GpuThread"), sync_point_manager_(sync_point_manager) {
  Start();
}

GpuInProcessThread::~GpuInProcessThread() {
  Stop();
}

void GpuInProcessThread::AddRef() const {
  base::RefCountedThreadSafe<GpuInProcessThread>::AddRef();
}
void GpuInProcessThread::Release() const {
  base::RefCountedThreadSafe<GpuInProcessThread>::Release();
}

void GpuInProcessThread::ScheduleTask(const base::Closure& task) {
  task_runner()->PostTask(FROM_HERE, task);
}

void GpuInProcessThread::ScheduleDelayedWork(const base::Closure& callback) {
  // Match delay with GpuCommandBufferStub.
  task_runner()->PostDelayedTask(FROM_HERE, callback,
                                 base::TimeDelta::FromMilliseconds(2));
}

bool GpuInProcessThread::UseVirtualizedGLContexts() {
  return false;
}

scoped_refptr<gles2::ShaderTranslatorCache>
GpuInProcessThread::shader_translator_cache() {
  if (!shader_translator_cache_.get()) {
    shader_translator_cache_ =
        new gpu::gles2::ShaderTranslatorCache(gpu_preferences());
  }
  return shader_translator_cache_;
}

scoped_refptr<gles2::FramebufferCompletenessCache>
GpuInProcessThread::framebuffer_completeness_cache() {
  if (!framebuffer_completeness_cache_.get())
    framebuffer_completeness_cache_ =
        new gpu::gles2::FramebufferCompletenessCache;
  return framebuffer_completeness_cache_;
}

SyncPointManager* GpuInProcessThread::sync_point_manager() {
  return sync_point_manager_;
}

}  // namespace gpu
