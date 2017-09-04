// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/mojo_gpu_memory_buffer_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/gfx/buffer_format_util.h"

namespace ui {

namespace {

void OnGpuMemoryBufferAllocated(gfx::GpuMemoryBufferHandle* ret_handle,
                                base::WaitableEvent* wait,
                                const gfx::GpuMemoryBufferHandle& handle) {
  *ret_handle = handle;
  wait->Signal();
}

}  // namespace

MojoGpuMemoryBufferManager::MojoGpuMemoryBufferManager(
    service_manager::Connector* connector)
    : thread_("GpuMemoryThread"),
      connector_(connector->Clone()),
      weak_ptr_factory_(this) {
  CHECK(thread_.Start());
  // The thread is owned by this object. Which means the test will not run if
  // the object has been destroyed. So Unretained() is safe.
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoGpuMemoryBufferManager::InitThread,
                            base::Unretained(this)));
}

MojoGpuMemoryBufferManager::~MojoGpuMemoryBufferManager() {
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoGpuMemoryBufferManager::TearDownThread,
                            base::Unretained(this)));
  thread_.Stop();
}

void MojoGpuMemoryBufferManager::InitThread() {
  connector_->ConnectToInterface(ui::mojom::kServiceName, &gpu_service_);
}

void MojoGpuMemoryBufferManager::TearDownThread() {
  gpu_service_.reset();
}

void MojoGpuMemoryBufferManager::AllocateGpuMemoryBufferOnThread(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle,
    base::WaitableEvent* wait) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());
  // |handle| and |wait| are both on the stack, and will be alive until |wait|
  // is signaled. So it is safe for OnGpuMemoryBufferAllocated() to operate on
  // these.
  gpu_service_->CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId(++counter_), size, format, usage,
      base::Bind(&OnGpuMemoryBufferAllocated, handle, wait));
}

void MojoGpuMemoryBufferManager::DeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gpu::SyncToken& sync_token) {
  if (!thread_.task_runner()->BelongsToCurrentThread()) {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&MojoGpuMemoryBufferManager::DeletedGpuMemoryBuffer,
                   base::Unretained(this), id, sync_token));
    return;
  }
  gpu_service_->DestroyGpuMemoryBuffer(id, sync_token);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MojoGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  // Note: this can be called from multiple threads at the same time. Some of
  // those threads may not have a TaskRunner set.
  DCHECK_EQ(gpu::kNullSurfaceHandle, surface_handle);
  CHECK(!thread_.task_runner()->BelongsToCurrentThread());
  gfx::GpuMemoryBufferHandle gmb_handle;
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MojoGpuMemoryBufferManager::AllocateGpuMemoryBufferOnThread,
                 base::Unretained(this), size, format, usage, &gmb_handle,
                 &wait));
  wait.Wait();
  if (gmb_handle.is_null())
    return nullptr;
  std::unique_ptr<gpu::GpuMemoryBufferImpl> buffer(
      gpu::GpuMemoryBufferImpl::CreateFromHandle(
          gmb_handle, size, format, usage,
          base::Bind(&MojoGpuMemoryBufferManager::DeletedGpuMemoryBuffer,
                     weak_ptr_factory_.GetWeakPtr(), gmb_handle.id)));
  if (!buffer) {
    DeletedGpuMemoryBuffer(gmb_handle.id, gpu::SyncToken());
    return nullptr;
  }
  return std::move(buffer);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MojoGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  NOTIMPLEMENTED();
  return nullptr;
}

void MojoGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

}  // namespace ui
