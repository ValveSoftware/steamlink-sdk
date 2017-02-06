// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_gpu_memory_buffer_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace blimp {
namespace client {

namespace {

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size,
                      gfx::BufferFormat format,
                      std::unique_ptr<base::SharedMemory> shared_memory,
                      size_t offset,
                      size_t stride)
      : size_(size),
        format_(format),
        shared_memory_(std::move(shared_memory)),
        offset_(offset),
        stride_(stride),
        mapped_(false) {}

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    DCHECK_EQ(stride_, gfx::RowSizeForBufferFormat(size_.width(),
                                                   format_, 0));
    if (!shared_memory_->Map(offset_ +
                             gfx::BufferSizeForBufferFormat(size_, format_)))
      return false;
    mapped_ = true;
    return true;
  }

  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
    return reinterpret_cast<uint8_t*>(shared_memory_->memory()) + offset_ +
        gfx::BufferOffsetForBufferFormat(size_, format_, plane);
  }

  void Unmap() override {
    DCHECK(mapped_);
    shared_memory_->Unmap();
    mapped_ = false;
  }

  gfx::Size GetSize() const override { return size_; }

  gfx::BufferFormat GetFormat() const override { return format_; }

  int stride(size_t plane) const override {
    DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
    return base::checked_cast<int>(gfx::RowSizeForBufferFormat(
        size_.width(), format_, static_cast<int>(plane)));
  }

  gfx::GpuMemoryBufferId GetId() const override {
    return gfx::GpuMemoryBufferId(0);
  }

  gfx::GpuMemoryBufferHandle GetHandle() const override {
    gfx::GpuMemoryBufferHandle handle;
    handle.type = gfx::SHARED_MEMORY_BUFFER;
    handle.handle = shared_memory_->handle();
    handle.offset = base::checked_cast<uint32_t>(offset_);
    handle.stride = base::checked_cast<int32_t>(stride_);
    return handle;
  }

  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }

 private:
  const gfx::Size size_;
  gfx::BufferFormat format_;
  std::unique_ptr<base::SharedMemory> shared_memory_;
  size_t offset_;
  size_t stride_;
  bool mapped_;
};

}  // namespace

BlimpGpuMemoryBufferManager::BlimpGpuMemoryBufferManager() {}

BlimpGpuMemoryBufferManager::~BlimpGpuMemoryBufferManager() {}

std::unique_ptr<gfx::GpuMemoryBuffer>
BlimpGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_EQ(gpu::kNullSurfaceHandle, surface_handle)
      << "Blimp should not be allocating scanout buffers";
  std::unique_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  const size_t buffer_size = gfx::BufferSizeForBufferFormat(size, format);
  if (!shared_memory->CreateAnonymous(buffer_size))
    return nullptr;
  return base::WrapUnique<gfx::GpuMemoryBuffer>(new GpuMemoryBufferImpl(
      size, format, std::move(shared_memory), 0,
      base::checked_cast<int>(
          gfx::RowSizeForBufferFormat(size.width(), format, 0))));
}

std::unique_ptr<gfx::GpuMemoryBuffer>
BlimpGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  if (handle.type != gfx::SHARED_MEMORY_BUFFER)
    return nullptr;

  return base::WrapUnique<gfx::GpuMemoryBuffer>(new GpuMemoryBufferImpl(
      size, format,
      base::WrapUnique(new base::SharedMemory(handle.handle, false)),
      handle.offset, handle.stride));
}

gfx::GpuMemoryBuffer*
BlimpGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
}

void BlimpGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  NOTIMPLEMENTED();
}

}  // namespace client
}  // namespace blimp
