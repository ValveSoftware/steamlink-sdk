// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/gpu_memory_buffer_impl_ozone_native_pixmap.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {
namespace {

void FreeNativePixmapForTesting(scoped_refptr<ui::NativePixmap> native_pixmap) {
  // Nothing to do here. |native_pixmap| will be freed when this function
  // returns and reference count drops to 0.
}

}  // namespace

GpuMemoryBufferImplOzoneNativePixmap::GpuMemoryBufferImplOzoneNativePixmap(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const DestructionCallback& callback,
    std::unique_ptr<ui::ClientNativePixmap> pixmap,
    const std::vector<std::pair<int, int>>& strides_and_offsets,
    base::ScopedFD fd)
    : GpuMemoryBufferImpl(id, size, format, callback),
      pixmap_(std::move(pixmap)),
      strides_and_offsets_(strides_and_offsets),
      fd_(std::move(fd)),
      data_(nullptr) {}

GpuMemoryBufferImplOzoneNativePixmap::~GpuMemoryBufferImplOzoneNativePixmap() {}

// static
std::unique_ptr<GpuMemoryBufferImplOzoneNativePixmap>
GpuMemoryBufferImplOzoneNativePixmap::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  DCHECK_EQ(handle.native_pixmap_handle.fds.size(), 1u);

  // GpuMemoryBufferImpl needs the FD to implement GetHandle() but
  // ui::ClientNativePixmapFactory::ImportFromHandle is expected to take
  // ownership of the FD passed in the handle so we have to dup it here in
  // order to pass a valid FD to the GpuMemoryBufferImpl ctor.
  base::ScopedFD scoped_fd(
      HANDLE_EINTR(dup(handle.native_pixmap_handle.fds[0].fd)));
  if (!scoped_fd.is_valid()) {
    PLOG(ERROR) << "dup";
    return nullptr;
  }

  gfx::NativePixmapHandle native_pixmap_handle;
  native_pixmap_handle.fds.emplace_back(handle.native_pixmap_handle.fds[0].fd,
                                        true /* auto_close */);
  native_pixmap_handle.strides_and_offsets =
      handle.native_pixmap_handle.strides_and_offsets;
  std::unique_ptr<ui::ClientNativePixmap> native_pixmap =
      ui::ClientNativePixmapFactory::GetInstance()->ImportFromHandle(
          native_pixmap_handle, size, usage);
  DCHECK(native_pixmap);

  return base::WrapUnique(new GpuMemoryBufferImplOzoneNativePixmap(
      handle.id, size, format, callback, std::move(native_pixmap),
      handle.native_pixmap_handle.strides_and_offsets, std::move(scoped_fd)));
}

// static
bool GpuMemoryBufferImplOzoneNativePixmap::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return gpu::IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
}

// static
base::Closure GpuMemoryBufferImplOzoneNativePixmap::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  DCHECK(IsConfigurationSupported(format, usage));
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(gfx::kNullAcceleratedWidget, size, format,
                               usage);
  handle->type = gfx::OZONE_NATIVE_PIXMAP;
  handle->native_pixmap_handle = pixmap->ExportHandle();
  return base::Bind(&FreeNativePixmapForTesting, pixmap);
}

bool GpuMemoryBufferImplOzoneNativePixmap::Map() {
  DCHECK(!mapped_);
  DCHECK(!data_);
  data_ = pixmap_->Map();
  if (!data_)
    return false;
  mapped_ = true;
  return mapped_;
}

void* GpuMemoryBufferImplOzoneNativePixmap::memory(size_t plane) {
  DCHECK(mapped_);
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  return data_;
}

void GpuMemoryBufferImplOzoneNativePixmap::Unmap() {
  DCHECK(mapped_);
  DCHECK(data_);
  pixmap_->Unmap();
  mapped_ = false;
  data_ = nullptr;
}

int GpuMemoryBufferImplOzoneNativePixmap::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  int stride;
  pixmap_->GetStride(&stride);
  return stride;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplOzoneNativePixmap::GetHandle()
    const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  handle.id = id_;
  handle.native_pixmap_handle.fds.emplace_back(fd_.get(),
                                               false /* auto_close */);
  handle.native_pixmap_handle.strides_and_offsets = strides_and_offsets_;
  return handle;
}

}  // namespace gpu
