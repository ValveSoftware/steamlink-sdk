// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/command_buffer_client_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_restrictions.h"
#include "components/mus/common/gpu_type_converters.h"
#include "components/mus/common/mojo_buffer_backing.h"
#include "components/mus/common/mojo_gpu_memory_buffer.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mus {

namespace {

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

void MakeProgressCallback(gpu::CommandBuffer::State* output,
                          const gpu::CommandBuffer::State& input) {
  *output = input;
}

void InitializeCallback(mus::mojom::CommandBufferInitializeResultPtr* output,
                        mus::mojom::CommandBufferInitializeResultPtr input) {
  *output = std::move(input);
}

}  // namespace

CommandBufferClientImpl::CommandBufferClientImpl(
    const std::vector<int32_t>& attribs,
    mus::mojom::CommandBufferPtr command_buffer_ptr)
    : gpu_control_client_(nullptr),
      destroyed_(false),
      attribs_(attribs),
      client_binding_(this),
      command_buffer_(std::move(command_buffer_ptr)),
      command_buffer_id_(),
      last_put_offset_(-1),
      next_transfer_buffer_id_(0),
      next_image_id_(0),
      next_fence_sync_release_(1),
      flushed_fence_sync_release_(0) {
  command_buffer_.set_connection_error_handler(
      base::Bind(&CommandBufferClientImpl::Destroyed, base::Unretained(this),
                 gpu::error::kUnknown, gpu::error::kLostContext));
}

CommandBufferClientImpl::~CommandBufferClientImpl() {}

bool CommandBufferClientImpl::Initialize() {
  const size_t kSharedStateSize = sizeof(gpu::CommandBufferSharedState);
  mojo::ScopedSharedBufferHandle handle;
  bool result =
      CreateAndMapSharedBuffer(kSharedStateSize, &shared_state_, &handle);
  if (!result)
    return false;

  shared_state()->Initialize();

  mus::mojom::CommandBufferClientPtr client_ptr;
  client_binding_.Bind(GetProxy(&client_ptr));

  mus::mojom::CommandBufferInitializeResultPtr initialize_result;
  command_buffer_->Initialize(
      std::move(client_ptr), std::move(handle),
      mojo::Array<int32_t>::From(attribs_),
      base::Bind(&InitializeCallback, &initialize_result));

  base::ThreadRestrictions::ScopedAllowWait wait;
  if (!command_buffer_.WaitForIncomingResponse()) {
    VLOG(1) << "Channel encountered error while creating command buffer.";
    return false;
  }

  if (!initialize_result) {
    VLOG(1) << "Command buffer cannot be initialized successfully.";
    return false;
  }

  DCHECK_EQ(gpu::CommandBufferNamespace::MOJO,
            initialize_result->command_buffer_namespace);
  command_buffer_id_ = gpu::CommandBufferId::FromUnsafeValue(
      initialize_result->command_buffer_id);
  capabilities_ = initialize_result->capabilities;
  return true;
}

gpu::CommandBuffer::State CommandBufferClientImpl::GetLastState() {
  return last_state_;
}

int32_t CommandBufferClientImpl::GetLastToken() {
  TryUpdateState();
  return last_state_.token;
}

void CommandBufferClientImpl::Flush(int32_t put_offset) {
  if (last_put_offset_ == put_offset)
    return;

  last_put_offset_ = put_offset;
  command_buffer_->Flush(put_offset);
  flushed_fence_sync_release_ = next_fence_sync_release_ - 1;
}

void CommandBufferClientImpl::OrderingBarrier(int32_t put_offset) {
  // TODO(jamesr): Implement this more efficiently.
  Flush(put_offset);
}

void CommandBufferClientImpl::WaitForTokenInRange(int32_t start, int32_t end) {
  TryUpdateState();
  while (!InRange(start, end, last_state_.token) &&
         last_state_.error == gpu::error::kNoError) {
    MakeProgressAndUpdateState();
  }
}

void CommandBufferClientImpl::WaitForGetOffsetInRange(int32_t start,
                                                      int32_t end) {
  TryUpdateState();
  while (!InRange(start, end, last_state_.get_offset) &&
         last_state_.error == gpu::error::kNoError) {
    MakeProgressAndUpdateState();
  }
}

void CommandBufferClientImpl::SetGetBuffer(int32_t shm_id) {
  command_buffer_->SetGetBuffer(shm_id);
  last_put_offset_ = -1;
}

scoped_refptr<gpu::Buffer> CommandBufferClientImpl::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  if (size >= std::numeric_limits<uint32_t>::max())
    return NULL;

  mojo::ScopedSharedBufferMapping mapping;
  mojo::ScopedSharedBufferHandle handle;
  if (!CreateAndMapSharedBuffer(size, &mapping, &handle)) {
    if (last_state_.error == gpu::error::kNoError)
      last_state_.error = gpu::error::kLostContext;
    return NULL;
  }

  *id = ++next_transfer_buffer_id_;

  command_buffer_->RegisterTransferBuffer(*id, std::move(handle),
                                          static_cast<uint32_t>(size));

  std::unique_ptr<gpu::BufferBacking> backing(
      new mus::MojoBufferBacking(std::move(mapping), size));
  scoped_refptr<gpu::Buffer> buffer(new gpu::Buffer(std::move(backing)));
  return buffer;
}

void CommandBufferClientImpl::DestroyTransferBuffer(int32_t id) {
  command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferClientImpl::SetGpuControlClient(gpu::GpuControlClient* c) {
  gpu_control_client_ = c;
}

gpu::Capabilities CommandBufferClientImpl::GetCapabilities() {
  return capabilities_;
}

int32_t CommandBufferClientImpl::CreateImage(ClientBuffer buffer,
                                             size_t width,
                                             size_t height,
                                             unsigned internalformat) {
  int32_t new_id = ++next_image_id_;

  gfx::Size size(static_cast<int32_t>(width), static_cast<int32_t>(height));

  mus::MojoGpuMemoryBufferImpl* gpu_memory_buffer =
      mus::MojoGpuMemoryBufferImpl::FromClientBuffer(buffer);
  gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->GetHandle();

  bool requires_sync_point = false;
  if (handle.type != gfx::SHARED_MEMORY_BUFFER) {
    requires_sync_point = true;
    NOTIMPLEMENTED();
    return -1;
  }

  base::SharedMemoryHandle dupd_handle =
      base::SharedMemory::DuplicateHandle(handle.handle);
#if defined(OS_WIN)
  HANDLE platform_handle = dupd_handle.GetHandle();
#else
  int platform_handle = dupd_handle.fd;
#endif

  mojo::ScopedHandle scoped_handle = mojo::WrapPlatformFile(platform_handle);
  command_buffer_->CreateImage(
      new_id, std::move(scoped_handle), handle.type, std::move(size),
      static_cast<int32_t>(gpu_memory_buffer->GetFormat()), internalformat);
  if (requires_sync_point) {
    NOTIMPLEMENTED();
    // TODO(jam): need to support this if we support types other than
    // SHARED_MEMORY_BUFFER.
    // gpu_memory_buffer_manager->SetDestructionSyncPoint(gpu_memory_buffer,
    //                                                   InsertSyncPoint());
  }

  return new_id;
}

void CommandBufferClientImpl::DestroyImage(int32_t id) {
  command_buffer_->DestroyImage(id);
}

int32_t CommandBufferClientImpl::CreateGpuMemoryBufferImage(
    size_t width,
    size_t height,
    unsigned internalformat,
    unsigned usage) {
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(
      mus::MojoGpuMemoryBufferImpl::Create(
          gfx::Size(static_cast<int>(width), static_cast<int>(height)),
          gpu::DefaultBufferFormatForImageFormat(internalformat),
          gfx::BufferUsage::SCANOUT));
  if (!buffer)
    return -1;

  return CreateImage(buffer->AsClientBuffer(), width, height, internalformat);
}

int32_t CommandBufferClientImpl::GetImageGpuMemoryBufferId(unsigned image_id) {
  // TODO(erikchen): Once this class supports IOSurface GpuMemoryBuffer backed
  // images, it will also need to keep a local cache from image id to
  // GpuMemoryBuffer id.
  NOTIMPLEMENTED();
  return -1;
}

void CommandBufferClientImpl::SignalQuery(uint32_t query,
                                          const base::Closure& callback) {
  // TODO(piman)
  NOTIMPLEMENTED();
}

void CommandBufferClientImpl::Destroyed(int32_t lost_reason, int32_t error) {
  if (destroyed_)
    return;
  last_state_.context_lost_reason =
      static_cast<gpu::error::ContextLostReason>(lost_reason);
  last_state_.error = static_cast<gpu::error::Error>(error);
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContext();
  destroyed_ = true;
}

void CommandBufferClientImpl::SignalAck(uint32_t id) {}

void CommandBufferClientImpl::SwapBuffersCompleted(int32_t result) {}

void CommandBufferClientImpl::UpdateState(
    const gpu::CommandBuffer::State& state) {}

void CommandBufferClientImpl::UpdateVSyncParameters(int64_t timebase,
                                                    int64_t interval) {}

void CommandBufferClientImpl::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

void CommandBufferClientImpl::MakeProgressAndUpdateState() {
  gpu::CommandBuffer::State state;
  command_buffer_->MakeProgress(last_state_.get_offset,
                                base::Bind(&MakeProgressCallback, &state));

  base::ThreadRestrictions::ScopedAllowWait wait;
  if (!command_buffer_.WaitForIncomingResponse()) {
    VLOG(1) << "Channel encountered error while waiting for command buffer.";
    // TODO(piman): is it ok for this to re-enter?
    Destroyed(gpu::error::kUnknown, gpu::error::kLostContext);
    return;
  }

  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

void CommandBufferClientImpl::SetLock(base::Lock* lock) {}

void CommandBufferClientImpl::EnsureWorkVisible() {
  // This is only relevant for out-of-process command buffers.
}

gpu::CommandBufferNamespace CommandBufferClientImpl::GetNamespaceID() const {
  return gpu::CommandBufferNamespace::MOJO;
}

gpu::CommandBufferId CommandBufferClientImpl::GetCommandBufferID() const {
  return command_buffer_id_;
}

int32_t CommandBufferClientImpl::GetExtraCommandBufferData() const {
  return 0;
}

uint64_t CommandBufferClientImpl::GenerateFenceSyncRelease() {
  return next_fence_sync_release_++;
}

bool CommandBufferClientImpl::IsFenceSyncRelease(uint64_t release) {
  return release != 0 && release < next_fence_sync_release_;
}

bool CommandBufferClientImpl::IsFenceSyncFlushed(uint64_t release) {
  return release != 0 && release <= flushed_fence_sync_release_;
}

bool CommandBufferClientImpl::IsFenceSyncFlushReceived(uint64_t release) {
  return IsFenceSyncFlushed(release);
}

void CommandBufferClientImpl::SignalSyncToken(const gpu::SyncToken& sync_token,
                                              const base::Closure& callback) {
  // TODO(dyen)
  NOTIMPLEMENTED();
}

bool CommandBufferClientImpl::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken* sync_token) {
  // Right now, MOJO_LOCAL is only used by trusted code, so it is safe to wait
  // on a sync token in MOJO_LOCAL command buffer.
  if (sync_token->namespace_id() == gpu::CommandBufferNamespace::MOJO_LOCAL)
    return true;

  // It is also safe to wait on the same context.
  if (sync_token->namespace_id() == gpu::CommandBufferNamespace::MOJO &&
      sync_token->command_buffer_id() == GetCommandBufferID())
    return true;

  return false;
}

}  // namespace mus
