// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_gpu_memory_buffer_manager.h"

#include <memory>
#include <utility>

#include "content/common/child_process_messages.h"
#include "content/common/generic_shared_memory_id_generator.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"

namespace content {
namespace {

void DeletedGpuMemoryBuffer(ThreadSafeSender* sender,
                            gfx::GpuMemoryBufferId id,
                            const gpu::SyncToken& sync_token) {
  TRACE_EVENT0("renderer",
               "ChildGpuMemoryBufferManager::DeletedGpuMemoryBuffer");
  sender->Send(new ChildProcessHostMsg_DeletedGpuMemoryBuffer(id, sync_token));
}

}  // namespace

ChildGpuMemoryBufferManager::ChildGpuMemoryBufferManager(
    ThreadSafeSender* sender)
    : sender_(sender) {
}

ChildGpuMemoryBufferManager::~ChildGpuMemoryBufferManager() {
}

std::unique_ptr<gfx::GpuMemoryBuffer>
ChildGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_EQ(gpu::kNullSurfaceHandle, surface_handle);
  TRACE_EVENT2("renderer",
               "ChildGpuMemoryBufferManager::AllocateGpuMemoryBuffer",
               "width",
               size.width(),
               "height",
               size.height());

  gfx::GpuMemoryBufferHandle handle;
  IPC::Message* message = new ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer(
      content::GetNextGenericSharedMemoryId(), size.width(), size.height(),
      format, usage, &handle);
  bool success = sender_->Send(message);
  if (!success || handle.is_null())
    return nullptr;

  std::unique_ptr<gpu::GpuMemoryBufferImpl> buffer(
      gpu::GpuMemoryBufferImpl::CreateFromHandle(
          handle, size, format, usage,
          base::Bind(&DeletedGpuMemoryBuffer, base::RetainedRef(sender_),
                     handle.id)));
  if (!buffer) {
    sender_->Send(new ChildProcessHostMsg_DeletedGpuMemoryBuffer(
        handle.id, gpu::SyncToken()));
    return nullptr;
  }

  return std::move(buffer);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
ChildGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::GpuMemoryBuffer*
ChildGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return gpu::GpuMemoryBufferImpl::FromClientBuffer(buffer);
}

void ChildGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

}  // namespace content
