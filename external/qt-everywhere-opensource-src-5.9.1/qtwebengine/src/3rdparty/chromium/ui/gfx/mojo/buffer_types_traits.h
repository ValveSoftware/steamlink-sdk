// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_BUFFER_TYPES_TRAITS_H_
#define UI_GFX_MOJO_BUFFER_TYPES_TRAITS_H_

#include "ui/gfx/buffer_types.h"
#include "ui/gfx/mojo/buffer_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::BufferFormat, gfx::BufferFormat> {
  static gfx::mojom::BufferFormat ToMojom(gfx::BufferFormat format) {
    switch (format) {
      case gfx::BufferFormat::ATC:
        return gfx::mojom::BufferFormat::ATC;
      case gfx::BufferFormat::ATCIA:
        return gfx::mojom::BufferFormat::ATCIA;
      case gfx::BufferFormat::DXT1:
        return gfx::mojom::BufferFormat::DXT1;
      case gfx::BufferFormat::DXT5:
        return gfx::mojom::BufferFormat::DXT5;
      case gfx::BufferFormat::ETC1:
        return gfx::mojom::BufferFormat::ETC1;
      case gfx::BufferFormat::R_8:
        return gfx::mojom::BufferFormat::R_8;
      case gfx::BufferFormat::RG_88:
        return gfx::mojom::BufferFormat::RG_88;
      case gfx::BufferFormat::BGR_565:
        return gfx::mojom::BufferFormat::BGR_565;
      case gfx::BufferFormat::RGBA_4444:
        return gfx::mojom::BufferFormat::RGBA_4444;
      case gfx::BufferFormat::RGBX_8888:
        return gfx::mojom::BufferFormat::RGBX_8888;
      case gfx::BufferFormat::RGBA_8888:
        return gfx::mojom::BufferFormat::RGBA_8888;
      case gfx::BufferFormat::BGRX_8888:
        return gfx::mojom::BufferFormat::BGRX_8888;
      case gfx::BufferFormat::BGRA_8888:
        return gfx::mojom::BufferFormat::BGRA_8888;
      case gfx::BufferFormat::YVU_420:
        return gfx::mojom::BufferFormat::YVU_420;
      case gfx::BufferFormat::YUV_420_BIPLANAR:
        return gfx::mojom::BufferFormat::YUV_420_BIPLANAR;
      case gfx::BufferFormat::UYVY_422:
        return gfx::mojom::BufferFormat::UYVY_422;
    }
    NOTREACHED();
    return gfx::mojom::BufferFormat::LAST;
  }

  static bool FromMojom(gfx::mojom::BufferFormat input,
                        gfx::BufferFormat* out) {
    switch (input) {
      case gfx::mojom::BufferFormat::ATC:
        *out = gfx::BufferFormat::ATC;
        return true;
      case gfx::mojom::BufferFormat::ATCIA:
        *out = gfx::BufferFormat::ATCIA;
        return true;
      case gfx::mojom::BufferFormat::DXT1:
        *out = gfx::BufferFormat::DXT1;
        return true;
      case gfx::mojom::BufferFormat::DXT5:
        *out = gfx::BufferFormat::DXT5;
        return true;
      case gfx::mojom::BufferFormat::ETC1:
        *out = gfx::BufferFormat::ETC1;
        return true;
      case gfx::mojom::BufferFormat::R_8:
        *out = gfx::BufferFormat::R_8;
        return true;
      case gfx::mojom::BufferFormat::RG_88:
        *out = gfx::BufferFormat::RG_88;
        return true;
      case gfx::mojom::BufferFormat::BGR_565:
        *out = gfx::BufferFormat::BGR_565;
        return true;
      case gfx::mojom::BufferFormat::RGBA_4444:
        *out = gfx::BufferFormat::RGBA_4444;
        return true;
      case gfx::mojom::BufferFormat::RGBX_8888:
        *out = gfx::BufferFormat::RGBX_8888;
        return true;
      case gfx::mojom::BufferFormat::RGBA_8888:
        *out = gfx::BufferFormat::RGBA_8888;
        return true;
      case gfx::mojom::BufferFormat::BGRX_8888:
        *out = gfx::BufferFormat::BGRX_8888;
        return true;
      case gfx::mojom::BufferFormat::BGRA_8888:
        *out = gfx::BufferFormat::BGRA_8888;
        return true;
      case gfx::mojom::BufferFormat::YVU_420:
        *out = gfx::BufferFormat::YVU_420;
        return true;
      case gfx::mojom::BufferFormat::YUV_420_BIPLANAR:
        *out = gfx::BufferFormat::YUV_420_BIPLANAR;
        return true;
      case gfx::mojom::BufferFormat::UYVY_422:
        *out = gfx::BufferFormat::UYVY_422;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<gfx::mojom::BufferUsage, gfx::BufferUsage> {
  static gfx::mojom::BufferUsage ToMojom(gfx::BufferUsage usage) {
    switch (usage) {
      case gfx::BufferUsage::GPU_READ:
        return gfx::mojom::BufferUsage::GPU_READ;
      case gfx::BufferUsage::SCANOUT:
        return gfx::mojom::BufferUsage::SCANOUT;
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
        return gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE;
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
        return gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT;
    }
    NOTREACHED();
    return gfx::mojom::BufferUsage::LAST;
  }

  static bool FromMojom(gfx::mojom::BufferUsage input, gfx::BufferUsage* out) {
    switch (input) {
      case gfx::mojom::BufferUsage::GPU_READ:
        *out = gfx::BufferUsage::GPU_READ;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT:
        *out = gfx::BufferUsage::SCANOUT;
        return true;
      case gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE:
        *out = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
        return true;
      case gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
        *out = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<gfx::mojom::GpuMemoryBufferType, gfx::GpuMemoryBufferType> {
  static gfx::mojom::GpuMemoryBufferType ToMojom(
      gfx::GpuMemoryBufferType type) {
    switch (type) {
      case gfx::GpuMemoryBufferType::EMPTY_BUFFER:
        return gfx::mojom::GpuMemoryBufferType::EMPTY_BUFFER;
      case gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER:
        return gfx::mojom::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
      case gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER:
        return gfx::mojom::GpuMemoryBufferType::IO_SURFACE_BUFFER;
      case gfx::GpuMemoryBufferType::OZONE_NATIVE_PIXMAP:
        return gfx::mojom::GpuMemoryBufferType::OZONE_NATIVE_PIXMAP;
    }
    NOTREACHED();
    return gfx::mojom::GpuMemoryBufferType::LAST;
  }

  static bool FromMojom(gfx::mojom::GpuMemoryBufferType input,
                        gfx::GpuMemoryBufferType* out) {
    switch (input) {
      case gfx::mojom::GpuMemoryBufferType::EMPTY_BUFFER:
        *out = gfx::GpuMemoryBufferType::EMPTY_BUFFER;
        return true;
      case gfx::mojom::GpuMemoryBufferType::SHARED_MEMORY_BUFFER:
        *out = gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
        return true;
      case gfx::mojom::GpuMemoryBufferType::IO_SURFACE_BUFFER:
        *out = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
        return true;
      case gfx::mojom::GpuMemoryBufferType::OZONE_NATIVE_PIXMAP:
        *out = gfx::GpuMemoryBufferType::OZONE_NATIVE_PIXMAP;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<gfx::mojom::GpuMemoryBufferIdDataView,
                    gfx::GpuMemoryBufferId> {
  static int32_t id(const gfx::GpuMemoryBufferId& buffer_id) {
    return buffer_id.id;
  }
  static bool Read(gfx::mojom::GpuMemoryBufferIdDataView data,
                   gfx::GpuMemoryBufferId* out) {
    out->id = data.id();
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::NativePixmapPlaneDataView,
                    gfx::NativePixmapPlane> {
  static uint32_t stride(const gfx::NativePixmapPlane& plane) {
    return plane.stride;
  }
  static int32_t offset(const gfx::NativePixmapPlane& plane) {
    return plane.offset;
  }
  static uint64_t size(const gfx::NativePixmapPlane& plane) {
    return plane.size;
  }
  static uint64_t modifier(const gfx::NativePixmapPlane& plane) {
    return plane.modifier;
  }
  static bool Read(gfx::mojom::NativePixmapPlaneDataView data,
                   gfx::NativePixmapPlane* out) {
    out->stride = data.stride();
    out->offset = data.offset();
    out->size = data.size();
    out->modifier = data.modifier();
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::NativePixmapHandleDataView,
                    gfx::NativePixmapHandle> {
  using PixmapHandleFdList = std::vector<mojo::ScopedHandle>;

  static void* SetUpContext(const gfx::NativePixmapHandle& handle);
  static void TearDownContext(const gfx::NativePixmapHandle& handle,
                              void* context);

  static bool IsNull(const gfx::NativePixmapHandle& handle) {
#if defined(USE_OZONE)
    return false;
#else
    // NativePixmapHandle are not used on non-ozone platforms.
    return true;
#endif
  }
  static PixmapHandleFdList fds(const gfx::NativePixmapHandle& pixmap_handle,
                                void* context);

  static const std::vector<gfx::NativePixmapPlane>& planes(
      const gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.planes;
  }

  static bool Read(gfx::mojom::NativePixmapHandleDataView data,
                   gfx::NativePixmapHandle* out);
};

template <>
struct StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
                    gfx::GpuMemoryBufferHandle> {
  static gfx::GpuMemoryBufferType type(
      const gfx::GpuMemoryBufferHandle& handle) {
    return handle.type;
  }
  static gfx::GpuMemoryBufferId id(const gfx::GpuMemoryBufferHandle& handle) {
    return handle.id;
  }
  static mojo::ScopedHandle shared_memory_handle(
      const gfx::GpuMemoryBufferHandle& handle);
  static uint32_t offset(const gfx::GpuMemoryBufferHandle& handle) {
    return handle.offset;
  }
  static uint32_t stride(const gfx::GpuMemoryBufferHandle& handle) {
    return handle.stride;
  }
  static const gfx::NativePixmapHandle& native_pixmap_handle(
      const gfx::GpuMemoryBufferHandle& handle);
  static bool Read(gfx::mojom::GpuMemoryBufferHandleDataView data,
                   gfx::GpuMemoryBufferHandle* handle);
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_BUFFER_TYPES_TRAITS_H_
