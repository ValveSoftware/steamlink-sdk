// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_command_buffer_proxy.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/proxy_channel.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

PpapiCommandBufferProxy::PpapiCommandBufferProxy(
    const ppapi::HostResource& resource,
    ProxyChannel* channel)
    : resource_(resource),
      channel_(channel) {
}

PpapiCommandBufferProxy::~PpapiCommandBufferProxy() {
  // gpu::Buffers are no longer referenced, allowing shared memory objects to be
  // deleted, closing the handle in this process.
}

bool PpapiCommandBufferProxy::Initialize() {
  return true;
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::GetLastState() {
  ppapi::ProxyLock::AssertAcquiredDebugOnly();
  return last_state_;
}

int32 PpapiCommandBufferProxy::GetLastToken() {
  ppapi::ProxyLock::AssertAcquiredDebugOnly();
  return last_state_.token;
}

void PpapiCommandBufferProxy::Flush(int32 put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  IPC::Message* message = new PpapiHostMsg_PPBGraphics3D_AsyncFlush(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_, put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);
  Send(message);
}

void PpapiCommandBufferProxy::WaitForTokenInRange(int32 start, int32 end) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  bool success;
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

void PpapiCommandBufferProxy::WaitForGetOffsetInRange(int32 start, int32 end) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  bool success;
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

void PpapiCommandBufferProxy::SetGetBuffer(int32 transfer_buffer_id) {
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_SetGetBuffer(
         ppapi::API_ID_PPB_GRAPHICS_3D, resource_, transfer_buffer_id));
  }
}

scoped_refptr<gpu::Buffer> PpapiCommandBufferProxy::CreateTransferBuffer(
    size_t size,
    int32* id) {
  *id = -1;

  if (last_state_.error != gpu::error::kNoError)
    return NULL;

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  ppapi::proxy::SerializedHandle handle(
      ppapi::proxy::SerializedHandle::SHARED_MEMORY);
  if (!Send(new PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer(
            ppapi::API_ID_PPB_GRAPHICS_3D, resource_, size, id, &handle))) {
    return NULL;
  }

  if (*id <= 0 || !handle.is_shmem())
    return NULL;

  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle.shmem(), false));

  // Map the shared memory on demand.
  if (!shared_memory->memory()) {
    if (!shared_memory->Map(handle.size())) {
      *id = -1;
      return NULL;
    }
  }

  return gpu::MakeBufferFromSharedMemory(shared_memory.Pass(), handle.size());
}

void PpapiCommandBufferProxy::DestroyTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_, id));
}

void PpapiCommandBufferProxy::Echo(const base::Closure& callback) {
  NOTREACHED();
}

uint32 PpapiCommandBufferProxy::CreateStreamTexture(uint32 texture_id) {
  NOTREACHED();
  return 0;
}

uint32 PpapiCommandBufferProxy::InsertSyncPoint() {
  uint32 sync_point = 0;
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_InsertSyncPoint(
         ppapi::API_ID_PPB_GRAPHICS_3D, resource_, &sync_point));
  }
  return sync_point;
}

void PpapiCommandBufferProxy::SignalSyncPoint(uint32 sync_point,
                                              const base::Closure& callback) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SignalQuery(uint32 query,
                                          const base::Closure& callback) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SetSurfaceVisible(bool visible) {
  NOTREACHED();
}

gpu::Capabilities PpapiCommandBufferProxy::GetCapabilities() {
  // TODO(boliu): Need to implement this to use cc in Pepper. Tracked in
  // crbug.com/325391.
  return gpu::Capabilities();
}

gfx::GpuMemoryBuffer* PpapiCommandBufferProxy::CreateGpuMemoryBuffer(
    size_t width,
    size_t height,
    unsigned internalformat,
    unsigned usage,
    int32* id) {
  NOTREACHED();
  return NULL;
}

void PpapiCommandBufferProxy::DestroyGpuMemoryBuffer(int32 id) {
  NOTREACHED();
}

bool PpapiCommandBufferProxy::Send(IPC::Message* msg) {
  DCHECK(last_state_.error == gpu::error::kNoError);

  if (channel_->Send(msg))
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

}  // namespace proxy
}  // namespace ppapi
