// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/ozone_gpu_memory_buffer.h"

#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/public/client_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace mus {

OzoneGpuMemoryBuffer::OzoneGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    std::unique_ptr<ui::ClientNativePixmap> client_pixmap,
    scoped_refptr<ui::NativePixmap> native_pixmap)
    : GpuMemoryBufferImpl(id, size, format),
      client_pixmap_(std::move(client_pixmap)),
      native_pixmap_(native_pixmap) {}

OzoneGpuMemoryBuffer::~OzoneGpuMemoryBuffer() {
  DCHECK(!mapped_);
}

// static
OzoneGpuMemoryBuffer* OzoneGpuMemoryBuffer::FromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<OzoneGpuMemoryBuffer*>(buffer);
}

// static
std::unique_ptr<gfx::GpuMemoryBuffer>
OzoneGpuMemoryBuffer::CreateOzoneGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::AcceleratedWidget widget) {
  scoped_refptr<ui::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(widget, size, format, usage);

  DCHECK(pixmap) << "need pixmap to exist!";

  if (!pixmap.get()) {
    DLOG(ERROR) << "Failed to create pixmap " << size.width() << "x"
                << size.height() << " format " << static_cast<int>(format)
                << ", usage " << static_cast<int>(usage);
    return nullptr;
  }

  // We construct a ui::NativePixmapHandle
  gfx::NativePixmapHandle native_pixmap_handle = pixmap->ExportHandle();
  DCHECK(ui::ClientNativePixmapFactory::GetInstance())
      << "need me a ClientNativePixmapFactory";
  std::unique_ptr<ui::ClientNativePixmap> client_native_pixmap =
      ui::ClientNativePixmapFactory::GetInstance()->ImportFromHandle(
          native_pixmap_handle, size, usage);

  std::unique_ptr<OzoneGpuMemoryBuffer> nb(
      new OzoneGpuMemoryBuffer(gfx::GpuMemoryBufferId(0), size, format,
                               std::move(client_native_pixmap), pixmap));
  return std::move(nb);
}

bool OzoneGpuMemoryBuffer::Map() {
  DCHECK(!mapped_);
  if (!client_pixmap_->Map())
    return false;
  mapped_ = true;
  return mapped_;
}

void* OzoneGpuMemoryBuffer::memory(size_t plane) {
  DCHECK(mapped_);
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  return client_pixmap_->Map();
}

void OzoneGpuMemoryBuffer::Unmap() {
  DCHECK(mapped_);
  client_pixmap_->Unmap();
  mapped_ = false;
}

int OzoneGpuMemoryBuffer::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  int stride;
  client_pixmap_->GetStride(&stride);
  return stride;
}

gfx::GpuMemoryBufferHandle OzoneGpuMemoryBuffer::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  handle.id = id_;
  return handle;
}

gfx::GpuMemoryBufferType OzoneGpuMemoryBuffer::GetBufferType() const {
  return gfx::OZONE_NATIVE_PIXMAP;
}

#if defined(USE_OZONE)
scoped_refptr<ui::NativePixmap> OzoneGpuMemoryBuffer::GetNativePixmap() {
  return native_pixmap_;
}
#endif

}  // namespace mus
