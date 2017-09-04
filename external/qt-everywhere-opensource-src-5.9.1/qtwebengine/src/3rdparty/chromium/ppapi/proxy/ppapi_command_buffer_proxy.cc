// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_command_buffer_proxy.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

PpapiCommandBufferProxy::PpapiCommandBufferProxy(
    const ppapi::HostResource& resource,
    PluginDispatcher* dispatcher,
    const gpu::Capabilities& capabilities,
    const SerializedHandle& shared_state,
    gpu::CommandBufferId command_buffer_id)
    : command_buffer_id_(command_buffer_id),
      capabilities_(capabilities),
      resource_(resource),
      dispatcher_(dispatcher),
      next_fence_sync_release_(1),
      pending_fence_sync_release_(0),
      flushed_fence_sync_release_(0),
      validated_fence_sync_release_(0) {
  shared_state_shm_.reset(
      new base::SharedMemory(shared_state.shmem(), false));
  shared_state_shm_->Map(shared_state.size());
  InstanceData* data = dispatcher->GetInstanceData(resource.instance());
  flush_info_ = &data->flush_info_;
}

PpapiCommandBufferProxy::~PpapiCommandBufferProxy() {
  // gpu::Buffers are no longer referenced, allowing shared memory objects to be
  // deleted, closing the handle in this process.
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::GetLastState() {
  ppapi::ProxyLock::AssertAcquiredDebugOnly();
  return last_state_;
}

int32_t PpapiCommandBufferProxy::GetLastToken() {
  ppapi::ProxyLock::AssertAcquiredDebugOnly();
  TryUpdateState();
  return last_state_.token;
}

void PpapiCommandBufferProxy::Flush(int32_t put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  OrderingBarrier(put_offset);
  FlushInternal();
}

void PpapiCommandBufferProxy::OrderingBarrier(int32_t put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  if (flush_info_->flush_pending && flush_info_->resource != resource_) {
    FlushInternal();
  }

  flush_info_->flush_pending = true;
  flush_info_->resource = resource_;
  flush_info_->put_offset = put_offset;
  pending_fence_sync_release_ = next_fence_sync_release_ - 1;
}

void PpapiCommandBufferProxy::WaitForTokenInRange(int32_t start, int32_t end) {
  TryUpdateState();
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    bool success = false;
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBGraphics3D_WaitForTokenInRange(
             ppapi::API_ID_PPB_GRAPHICS_3D,
             resource_,
             start,
             end,
             &state,
             &success)))
      UpdateState(state, success);
  }
  DCHECK(InRange(start, end, last_state_.token) ||
         last_state_.error != gpu::error::kNoError);
}

void PpapiCommandBufferProxy::WaitForGetOffsetInRange(int32_t start,
                                                      int32_t end) {
  TryUpdateState();
  if (!InRange(start, end, last_state_.get_offset) &&
      last_state_.error == gpu::error::kNoError) {
    bool success = false;
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBGraphics3D_WaitForGetOffsetInRange(
             ppapi::API_ID_PPB_GRAPHICS_3D,
             resource_,
             start,
             end,
             &state,
             &success)))
      UpdateState(state, success);
  }
  DCHECK(InRange(start, end, last_state_.get_offset) ||
         last_state_.error != gpu::error::kNoError);
}

void PpapiCommandBufferProxy::SetGetBuffer(int32_t transfer_buffer_id) {
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_SetGetBuffer(
         ppapi::API_ID_PPB_GRAPHICS_3D, resource_, transfer_buffer_id));
  }
}

scoped_refptr<gpu::Buffer> PpapiCommandBufferProxy::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  *id = -1;

  if (last_state_.error != gpu::error::kNoError)
    return NULL;

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  ppapi::proxy::SerializedHandle handle(
      ppapi::proxy::SerializedHandle::SHARED_MEMORY);
  if (!Send(new PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer(
            ppapi::API_ID_PPB_GRAPHICS_3D, resource_,
            base::checked_cast<uint32_t>(size), id, &handle))) {
    if (last_state_.error == gpu::error::kNoError)
      last_state_.error = gpu::error::kLostContext;
    return NULL;
  }

  if (*id <= 0 || !handle.is_shmem()) {
    if (last_state_.error == gpu::error::kNoError)
      last_state_.error = gpu::error::kOutOfBounds;
    return NULL;
  }

  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle.shmem(), false));

  // Map the shared memory on demand.
  if (!shared_memory->memory()) {
    if (!shared_memory->Map(handle.size())) {
      if (last_state_.error == gpu::error::kNoError)
        last_state_.error = gpu::error::kOutOfBounds;
      *id = -1;
      return NULL;
    }
  }

  return gpu::MakeBufferFromSharedMemory(std::move(shared_memory),
                                         handle.size());
}

void PpapiCommandBufferProxy::DestroyTransferBuffer(int32_t id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_, id));
}

void PpapiCommandBufferProxy::SetLock(base::Lock*) {
  NOTIMPLEMENTED();
}

void PpapiCommandBufferProxy::EnsureWorkVisible() {
  DCHECK_GE(flushed_fence_sync_release_, validated_fence_sync_release_);
  Send(new PpapiHostMsg_PPBGraphics3D_EnsureWorkVisible(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_));
  validated_fence_sync_release_ = flushed_fence_sync_release_;
}

gpu::CommandBufferNamespace PpapiCommandBufferProxy::GetNamespaceID() const {
  return gpu::CommandBufferNamespace::GPU_IO;
}

gpu::CommandBufferId PpapiCommandBufferProxy::GetCommandBufferID() const {
  return command_buffer_id_;
}

uint64_t PpapiCommandBufferProxy::GenerateFenceSyncRelease() {
  return next_fence_sync_release_++;
}

bool PpapiCommandBufferProxy::IsFenceSyncRelease(uint64_t release) {
  return release != 0 && release < next_fence_sync_release_;
}

bool PpapiCommandBufferProxy::IsFenceSyncFlushed(uint64_t release) {
  return release <= flushed_fence_sync_release_;
}

bool PpapiCommandBufferProxy::IsFenceSyncFlushReceived(uint64_t release) {
  if (!IsFenceSyncFlushed(release))
    return false;

  if (release <= validated_fence_sync_release_)
    return true;

  EnsureWorkVisible();
  return release <= validated_fence_sync_release_;
}

void PpapiCommandBufferProxy::SignalSyncToken(const gpu::SyncToken& sync_token,
                                              const base::Closure& callback) {
  NOTIMPLEMENTED();
}

bool PpapiCommandBufferProxy::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken* sync_token) {
  return false;
}

int32_t PpapiCommandBufferProxy::GetExtraCommandBufferData() const {
  return 0;
}

void PpapiCommandBufferProxy::SignalQuery(uint32_t query,
                                          const base::Closure& callback) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SetGpuControlClient(gpu::GpuControlClient*) {
  // TODO(piman): The lost context callback skips past here and goes directly
  // to the plugin instance. Make it more uniform and use the GpuControlClient.
}

gpu::Capabilities PpapiCommandBufferProxy::GetCapabilities() {
  return capabilities_;
}

int32_t PpapiCommandBufferProxy::CreateImage(ClientBuffer buffer,
                                             size_t width,
                                             size_t height,
                                             unsigned internalformat) {
  NOTREACHED();
  return -1;
}

void PpapiCommandBufferProxy::DestroyImage(int32_t id) {
  NOTREACHED();
}

int32_t PpapiCommandBufferProxy::CreateGpuMemoryBufferImage(
    size_t width,
    size_t height,
    unsigned internalformat,
    unsigned usage) {
  NOTREACHED();
  return -1;
}

bool PpapiCommandBufferProxy::Send(IPC::Message* msg) {
  DCHECK(last_state_.error == gpu::error::kNoError);

  // We need to hold the Pepper proxy lock for sync IPC, because the GPU command
  // buffer may use a sync IPC with another lock held which could lead to lock
  // and deadlock if we dropped the proxy lock here.
  // http://crbug.com/418651
  if (dispatcher_->SendAndStayLocked(msg))
    return true;

  last_state_.error = gpu::error::kLostContext;
  return false;
}

void PpapiCommandBufferProxy::UpdateState(
    const gpu::CommandBuffer::State& state,
    bool success) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (success) {
    if (state.generation - last_state_.generation < 0x80000000U) {
      last_state_ = state;
    }
  } else {
    last_state_.error = gpu::error::kLostContext;
    ++last_state_.generation;
  }
}

void PpapiCommandBufferProxy::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

gpu::CommandBufferSharedState* PpapiCommandBufferProxy::shared_state() const {
  return reinterpret_cast<gpu::CommandBufferSharedState*>(
      shared_state_shm_->memory());
}

void PpapiCommandBufferProxy::FlushInternal() {
  DCHECK(last_state_.error == gpu::error::kNoError);

  DCHECK(flush_info_->flush_pending);
  DCHECK_GE(pending_fence_sync_release_, flushed_fence_sync_release_);

  IPC::Message* message = new PpapiHostMsg_PPBGraphics3D_AsyncFlush(
      ppapi::API_ID_PPB_GRAPHICS_3D, flush_info_->resource,
      flush_info_->put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);
  Send(message);

  flush_info_->flush_pending = false;
  flush_info_->resource.SetHostResource(0, 0);
  flushed_fence_sync_release_ = pending_fence_sync_release_;
}

}  // namespace proxy
}  // namespace ppapi
