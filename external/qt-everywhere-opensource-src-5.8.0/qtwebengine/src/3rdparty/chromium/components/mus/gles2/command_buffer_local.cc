// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_local.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/mus/common/gpu_type_converters.h"
#include "components/mus/common/mojo_buffer_backing.h"
#include "components/mus/common/mojo_gpu_memory_buffer.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/gles2/command_buffer_local_client.h"
#include "components/mus/gles2/gpu_memory_tracker.h"
#include "components/mus/gles2/gpu_state.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"

namespace mus {

namespace {

uint64_t g_next_command_buffer_id = 0;

bool CreateAndMapSharedBuffer(size_t size,
                              mojo::ScopedSharedBufferMapping* mapping,
                              mojo::ScopedSharedBufferHandle* handle) {
  *handle = mojo::SharedBufferHandle::Create(size);
  if (!handle->is_valid())
    return false;

  *mapping = (*handle)->Map(size);
  if (!*mapping)
    return false;

  return true;
}

void PostTask(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
              const base::Closure& callback) {
  task_runner->PostTask(FROM_HERE, callback);
}
}

const unsigned int GL_READ_WRITE_CHROMIUM = 0x78F2;

CommandBufferLocal::CommandBufferLocal(CommandBufferLocalClient* client,
                                       gfx::AcceleratedWidget widget,
                                       const scoped_refptr<GpuState>& gpu_state)
    : widget_(widget),
      gpu_state_(gpu_state),
      client_(client),
      client_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      gpu_control_client_(nullptr),
      next_transfer_buffer_id_(0),
      next_image_id_(0),
      next_fence_sync_release_(1),
      flushed_fence_sync_release_(0),
      lost_context_(false),
      sync_point_client_waiter_(
          gpu_state->sync_point_manager()->CreateSyncPointClientWaiter()),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

void CommandBufferLocal::Destroy() {
  DCHECK(CalledOnValidThread());
  // After this |Destroy()| call, this object will not be used by client anymore
  // and it will be deleted on the GPU thread. So we have to detach it from the
  // client thread first.
  DetachFromThread();

  weak_factory_.InvalidateWeakPtrs();
  // CommandBufferLocal is initialized on the GPU thread with
  // InitializeOnGpuThread(), so we need delete memebers on the GPU thread
  // too. Additionally we need to make sure we are deleted before returning,
  // otherwise we may attempt to use the AcceleratedWidget which has since been
  // destroyed.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::DeleteOnGpuThread,
                                base::Unretained(this), &event));
  event.Wait();
}

bool CommandBufferLocal::Initialize() {
  DCHECK(CalledOnValidThread());
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result = false;
  gpu_state_->command_buffer_task_runner()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferLocal::InitializeOnGpuThread,
                 base::Unretained(this), base::Unretained(&event),
                 base::Unretained(&result)));
  event.Wait();
  return result;
}

gpu::CommandBuffer::State CommandBufferLocal::GetLastState() {
  DCHECK(CalledOnValidThread());
  return last_state_;
}

int32_t CommandBufferLocal::GetLastToken() {
  DCHECK(CalledOnValidThread());
  TryUpdateState();
  return last_state_.token;
}

void CommandBufferLocal::Flush(int32_t put_offset) {
  DCHECK(CalledOnValidThread());
  if (last_put_offset_ == put_offset)
    return;

  last_put_offset_ = put_offset;
  gpu::SyncPointManager* sync_point_manager = gpu_state_->sync_point_manager();
  const uint32_t order_num =
      driver_->sync_point_order_data()->GenerateUnprocessedOrderNumber(
          sync_point_manager);
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::FlushOnGpuThread,
                                base::Unretained(this), put_offset, order_num));
  flushed_fence_sync_release_ = next_fence_sync_release_ - 1;
}

void CommandBufferLocal::OrderingBarrier(int32_t put_offset) {
  DCHECK(CalledOnValidThread());
  // TODO(penghuang): Implement this more efficiently.
  Flush(put_offset);
}

void CommandBufferLocal::WaitForTokenInRange(int32_t start, int32_t end) {
  DCHECK(CalledOnValidThread());
  TryUpdateState();
  while (!InRange(start, end, last_state_.token) &&
         last_state_.error == gpu::error::kNoError) {
    MakeProgressAndUpdateState();
  }
}

void CommandBufferLocal::WaitForGetOffsetInRange(int32_t start, int32_t end) {
  DCHECK(CalledOnValidThread());
  TryUpdateState();
  while (!InRange(start, end, last_state_.get_offset) &&
         last_state_.error == gpu::error::kNoError) {
    MakeProgressAndUpdateState();
  }
}

void CommandBufferLocal::SetGetBuffer(int32_t buffer) {
  DCHECK(CalledOnValidThread());
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::SetGetBufferOnGpuThread,
                                base::Unretained(this), buffer));
  last_put_offset_ = -1;
}

scoped_refptr<gpu::Buffer> CommandBufferLocal::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  DCHECK(CalledOnValidThread());
  if (size >= std::numeric_limits<uint32_t>::max())
    return nullptr;

  mojo::ScopedSharedBufferMapping mapping;
  mojo::ScopedSharedBufferHandle handle;
  if (!CreateAndMapSharedBuffer(size, &mapping, &handle)) {
    if (last_state_.error == gpu::error::kNoError)
      last_state_.error = gpu::error::kLostContext;
    return nullptr;
  }

  *id = ++next_transfer_buffer_id_;

  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferLocal::RegisterTransferBufferOnGpuThread,
                 base::Unretained(this), *id, base::Passed(&handle),
                 static_cast<uint32_t>(size)));
  std::unique_ptr<gpu::BufferBacking> backing(
      new mus::MojoBufferBacking(std::move(mapping), size));
  scoped_refptr<gpu::Buffer> buffer(new gpu::Buffer(std::move(backing)));
  return buffer;
}

void CommandBufferLocal::DestroyTransferBuffer(int32_t id) {
  DCHECK(CalledOnValidThread());
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferLocal::DestroyTransferBufferOnGpuThread,
                 base::Unretained(this), id));
}

void CommandBufferLocal::SetGpuControlClient(gpu::GpuControlClient* client) {
  gpu_control_client_ = client;
}

gpu::Capabilities CommandBufferLocal::GetCapabilities() {
  DCHECK(CalledOnValidThread());
  return capabilities_;
}

int32_t CommandBufferLocal::CreateImage(ClientBuffer buffer,
                                        size_t width,
                                        size_t height,
                                        unsigned internal_format) {
  DCHECK(CalledOnValidThread());
  int32_t new_id = ++next_image_id_;
  gfx::Size size(static_cast<int32_t>(width), static_cast<int32_t>(height));

  mus::MojoGpuMemoryBufferImpl* gpu_memory_buffer =
      mus::MojoGpuMemoryBufferImpl::FromClientBuffer(buffer);

  bool requires_sync_point = false;

  if (gpu_memory_buffer->GetBufferType() == gfx::SHARED_MEMORY_BUFFER) {
    gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->GetHandle();
    // TODO(rjkroege): Verify that this is required and update appropriately.
    base::SharedMemoryHandle dupd_handle =
        base::SharedMemory::DuplicateHandle(handle.handle);
#if defined(OS_WIN)
    HANDLE platform_file = dupd_handle.GetHandle();
#else
    int platform_file = dupd_handle.fd;
#endif

    mojo::ScopedHandle scoped_handle = mojo::WrapPlatformFile(platform_file);
    const int32_t format = static_cast<int32_t>(gpu_memory_buffer->GetFormat());
    gpu_state_->command_buffer_task_runner()->PostTask(
        driver_.get(),
        base::Bind(&CommandBufferLocal::CreateImageOnGpuThread,
                   base::Unretained(this), new_id, base::Passed(&scoped_handle),
                   handle.type, base::Passed(&size), format, internal_format));
#if defined(USE_OZONE)
  } else if (gpu_memory_buffer->GetBufferType() == gfx::OZONE_NATIVE_PIXMAP) {
    gpu_state_->command_buffer_task_runner()->PostTask(
        driver_.get(),
        base::Bind(&CommandBufferLocal::CreateImageNativeOzoneOnGpuThread,
                   base::Unretained(this), new_id,
                   gpu_memory_buffer->GetBufferType(),
                   gpu_memory_buffer->GetSize(), gpu_memory_buffer->GetFormat(),
                   internal_format,
                   base::RetainedRef(gpu_memory_buffer->GetNativePixmap())));
#endif
  } else {
    NOTIMPLEMENTED();
    return -1;
  }

  if (requires_sync_point) {
    NOTIMPLEMENTED() << "Require sync points";
    // TODO(jam): need to support this if we support types other than
    // SHARED_MEMORY_BUFFER.
    // gpu_memory_buffer_manager->SetDestructionSyncPoint(gpu_memory_buffer,
    // InsertSyncPoint());
  }

  return new_id;
}

void CommandBufferLocal::DestroyImage(int32_t id) {
  DCHECK(CalledOnValidThread());
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::DestroyImageOnGpuThread,
                                base::Unretained(this), id));
}

int32_t CommandBufferLocal::CreateGpuMemoryBufferImage(size_t width,
                                                       size_t height,
                                                       unsigned internal_format,
                                                       unsigned usage) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(usage, static_cast<unsigned>(GL_READ_WRITE_CHROMIUM));
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(MojoGpuMemoryBufferImpl::Create(
      gfx::Size(static_cast<int>(width), static_cast<int>(height)),
      gpu::DefaultBufferFormatForImageFormat(internal_format),
      gfx::BufferUsage::SCANOUT));
  if (!buffer)
    return -1;
  return CreateImage(buffer->AsClientBuffer(), width, height, internal_format);
}

int32_t CommandBufferLocal::GetImageGpuMemoryBufferId(unsigned image_id) {
  // TODO(erikchen): Once this class supports IOSurface GpuMemoryBuffer backed
  // images, it will also need to keep a local cache from image id to
  // GpuMemoryBuffer id.
  NOTIMPLEMENTED();
  return -1;
}

void CommandBufferLocal::SignalQuery(uint32_t query_id,
                                     const base::Closure& callback) {
  DCHECK(CalledOnValidThread());

  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::SignalQueryOnGpuThread,
                                base::Unretained(this), query_id, callback));
}

void CommandBufferLocal::SetLock(base::Lock* lock) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void CommandBufferLocal::EnsureWorkVisible() {
  // This is only relevant for out-of-process command buffers.
}

gpu::CommandBufferNamespace CommandBufferLocal::GetNamespaceID() const {
  DCHECK(CalledOnValidThread());
  return gpu::CommandBufferNamespace::MOJO_LOCAL;
}

gpu::CommandBufferId CommandBufferLocal::GetCommandBufferID() const {
  DCHECK(CalledOnValidThread());
  return driver_->GetCommandBufferID();
}

int32_t CommandBufferLocal::GetExtraCommandBufferData() const {
  DCHECK(CalledOnValidThread());
  return 0;
}

uint64_t CommandBufferLocal::GenerateFenceSyncRelease() {
  DCHECK(CalledOnValidThread());
  return next_fence_sync_release_++;
}

bool CommandBufferLocal::IsFenceSyncRelease(uint64_t release) {
  DCHECK(CalledOnValidThread());
  return release != 0 && release < next_fence_sync_release_;
}

bool CommandBufferLocal::IsFenceSyncFlushed(uint64_t release) {
  DCHECK(CalledOnValidThread());
  return release != 0 && release <= flushed_fence_sync_release_;
}

bool CommandBufferLocal::IsFenceSyncFlushReceived(uint64_t release) {
  DCHECK(CalledOnValidThread());
  return IsFenceSyncFlushed(release);
}

void CommandBufferLocal::SignalSyncToken(const gpu::SyncToken& sync_token,
                                         const base::Closure& callback) {
  DCHECK(CalledOnValidThread());
  scoped_refptr<gpu::SyncPointClientState> release_state =
      gpu_state_->sync_point_manager()->GetSyncPointClientState(
          sync_token.namespace_id(), sync_token.command_buffer_id());
  if (!release_state ||
      release_state->IsFenceSyncReleased(sync_token.release_count())) {
    callback.Run();
    return;
  }

  sync_point_client_waiter_->WaitOutOfOrderNonThreadSafe(
      release_state.get(), sync_token.release_count(),
      client_thread_task_runner_, callback);
}

bool CommandBufferLocal::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken* sync_token) {
  DCHECK(CalledOnValidThread());
  // Right now, MOJO_LOCAL is only used by trusted code, so it is safe to wait
  // on a sync token in MOJO_LOCAL command buffer.
  return sync_token->namespace_id() == gpu::CommandBufferNamespace::MOJO_LOCAL;
}

void CommandBufferLocal::DidLoseContext(uint32_t reason) {
  if (client_) {
  driver_->set_client(nullptr);
    client_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CommandBufferLocal::DidLoseContextOnClientThread,
                   weak_ptr_, reason));
  }
}

void CommandBufferLocal::UpdateVSyncParameters(
    const base::TimeTicks& timebase,
    const base::TimeDelta& interval) {
  if (client_) {
    client_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CommandBufferLocal::UpdateVSyncParametersOnClientThread,
                   weak_ptr_, timebase, interval));
  }
}

void CommandBufferLocal::OnGpuCompletedSwapBuffers(gfx::SwapResult result) {
  if (client_) {
    client_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CommandBufferLocal::OnGpuCompletedSwapBuffersOnClientThread,
                   weak_ptr_, result));
  }
}

CommandBufferLocal::~CommandBufferLocal() {}

void CommandBufferLocal::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

void CommandBufferLocal::MakeProgressAndUpdateState() {
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  gpu::CommandBuffer::State state;
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferLocal::MakeProgressOnGpuThread,
                 base::Unretained(this), base::Unretained(&event),
                 base::Unretained(&state)));
  event.Wait();
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

void CommandBufferLocal::InitializeOnGpuThread(base::WaitableEvent* event,
                                               bool* result) {
  driver_.reset(new CommandBufferDriver(
      gpu::CommandBufferNamespace::MOJO_LOCAL,
      gpu::CommandBufferId::FromUnsafeValue(++g_next_command_buffer_id),
      widget_, gpu_state_));
  driver_->set_client(this);
  const size_t kSharedStateSize = sizeof(gpu::CommandBufferSharedState);
  mojo::ScopedSharedBufferMapping mapping;
  mojo::ScopedSharedBufferHandle handle;
  *result = CreateAndMapSharedBuffer(kSharedStateSize, &shared_state_, &handle);

  if (!*result) {
    event->Signal();
    return;
  }

  shared_state()->Initialize();

  *result =
      driver_->Initialize(std::move(handle), mojo::Array<int32_t>::New(0));
  if (*result)
    capabilities_ = driver_->GetCapabilities();
  event->Signal();
}

bool CommandBufferLocal::FlushOnGpuThread(int32_t put_offset,
                                          uint32_t order_num) {
  DCHECK(driver_->IsScheduled());
  driver_->sync_point_order_data()->BeginProcessingOrderNumber(order_num);
  driver_->Flush(put_offset);

  // Return false if the Flush is not finished, so the CommandBufferTaskRunner
  // will not remove this task from the task queue.
  const bool complete = !driver_->HasUnprocessedCommands();
  if (complete)
    driver_->sync_point_order_data()->FinishProcessingOrderNumber(order_num);
  return complete;
}

bool CommandBufferLocal::SetGetBufferOnGpuThread(int32_t buffer) {
  DCHECK(driver_->IsScheduled());
  driver_->SetGetBuffer(buffer);
  return true;
}

bool CommandBufferLocal::RegisterTransferBufferOnGpuThread(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  DCHECK(driver_->IsScheduled());
  driver_->RegisterTransferBuffer(id, std::move(transfer_buffer), size);
  return true;
}

bool CommandBufferLocal::DestroyTransferBufferOnGpuThread(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyTransferBuffer(id);
  return true;
}

bool CommandBufferLocal::CreateImageOnGpuThread(
    int32_t id,
    mojo::ScopedHandle memory_handle,
    int32_t type,
    const gfx::Size& size,
    int32_t format,
    int32_t internal_format) {
  DCHECK(driver_->IsScheduled());
  driver_->CreateImage(id, std::move(memory_handle), type, std::move(size),
                       format, internal_format);
  return true;
}

bool CommandBufferLocal::CreateImageNativeOzoneOnGpuThread(
    int32_t id,
    int32_t type,
    gfx::Size size,
    gfx::BufferFormat format,
    uint32_t internal_format,
    ui::NativePixmap* pixmap) {
  DCHECK(driver_->IsScheduled());
  driver_->CreateImageNativeOzone(id, type, size, format, internal_format,
                                  pixmap);
  return true;
}

bool CommandBufferLocal::DestroyImageOnGpuThread(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyImage(id);
  return true;
}

bool CommandBufferLocal::MakeProgressOnGpuThread(
    base::WaitableEvent* event,
    gpu::CommandBuffer::State* state) {
  DCHECK(driver_->IsScheduled());
  *state = driver_->GetLastState();
  event->Signal();
  return true;
}

bool CommandBufferLocal::DeleteOnGpuThread(base::WaitableEvent* event) {
  delete this;
  event->Signal();
  return true;
}

bool CommandBufferLocal::SignalQueryOnGpuThread(uint32_t query_id,
                                                const base::Closure& callback) {
  // |callback| should run on the client thread.
  driver_->SignalQuery(
      query_id, base::Bind(&PostTask, client_thread_task_runner_, callback));
  return true;
}

void CommandBufferLocal::DidLoseContextOnClientThread(uint32_t reason) {
  DCHECK(gpu_control_client_);
  if (!lost_context_)
    gpu_control_client_->OnGpuControlLostContext();
  lost_context_ = true;
}

void CommandBufferLocal::UpdateVSyncParametersOnClientThread(
    const base::TimeTicks& timebase,
    const base::TimeDelta& interval) {
  if (client_)
    client_->UpdateVSyncParameters(timebase, interval);
}

void CommandBufferLocal::OnGpuCompletedSwapBuffersOnClientThread(
    gfx::SwapResult result) {
  if (client_)
    client_->GpuCompletedSwapBuffers(result);
}

}  // namespace mus
