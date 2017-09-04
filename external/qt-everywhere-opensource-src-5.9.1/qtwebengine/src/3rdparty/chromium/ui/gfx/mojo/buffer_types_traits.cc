// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojo/buffer_types_traits.h"

#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

void* StructTraits<gfx::mojom::NativePixmapHandleDataView,
                   gfx::NativePixmapHandle>::
    SetUpContext(const gfx::NativePixmapHandle& pixmap_handle) {
  return new PixmapHandleFdList();
}

void StructTraits<gfx::mojom::NativePixmapHandleDataView,
                  gfx::NativePixmapHandle>::
    TearDownContext(const gfx::NativePixmapHandle& handle, void* context) {
  delete static_cast<PixmapHandleFdList*>(context);
}

std::vector<mojo::ScopedHandle> StructTraits<
    gfx::mojom::NativePixmapHandleDataView,
    gfx::NativePixmapHandle>::fds(const gfx::NativePixmapHandle& pixmap_handle,
                                  void* context) {
  PixmapHandleFdList* handles = static_cast<PixmapHandleFdList*>(context);
#if defined(USE_OZONE)
  if (handles->empty()) {
    // Generate the handles here, but do not send them yet.
    for (const base::FileDescriptor& fd : pixmap_handle.fds) {
      base::PlatformFile platform_file = fd.fd;
      handles->push_back(mojo::WrapPlatformFile(platform_file));
    }
    return PixmapHandleFdList(handles->size());
  }
#endif  // defined(USE_OZONE)
  return std::move(*handles);
}

bool StructTraits<
    gfx::mojom::NativePixmapHandleDataView,
    gfx::NativePixmapHandle>::Read(gfx::mojom::NativePixmapHandleDataView data,
                                   gfx::NativePixmapHandle* out) {
#if defined(USE_OZONE)
  mojo::ArrayDataView<mojo::ScopedHandle> handles_data_view;
  data.GetFdsDataView(&handles_data_view);
  for (size_t i = 0; i < handles_data_view.size(); ++i) {
    mojo::ScopedHandle handle = handles_data_view.Take(i);
    base::PlatformFile platform_file;
    if (mojo::UnwrapPlatformFile(std::move(handle), &platform_file) !=
        MOJO_RESULT_OK)
      return false;
    constexpr bool auto_close = true;
    out->fds.push_back(base::FileDescriptor(platform_file, auto_close));
  }
  return data.ReadPlanes(&out->planes);
#else
  return false;
#endif
}

mojo::ScopedHandle StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
                                gfx::GpuMemoryBufferHandle>::
    shared_memory_handle(const gfx::GpuMemoryBufferHandle& handle) {
  if (handle.is_null())
    return mojo::ScopedHandle();
  base::PlatformFile platform_file = base::kInvalidPlatformFile;
#if defined(OS_WIN)
  platform_file = handle.handle.GetHandle();
#elif defined(OS_MACOSX) || defined(OS_IOS)
  NOTIMPLEMENTED();
#else
  platform_file = handle.handle.fd;
#endif
  return mojo::WrapPlatformFile(platform_file);
}

const gfx::NativePixmapHandle&
StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
             gfx::GpuMemoryBufferHandle>::
    native_pixmap_handle(const gfx::GpuMemoryBufferHandle& handle) {
#if defined(USE_OZONE)
  return handle.native_pixmap_handle;
#else
  static gfx::NativePixmapHandle pixmap_handle;
  return pixmap_handle;
#endif
}

bool StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
                  gfx::GpuMemoryBufferHandle>::
    Read(gfx::mojom::GpuMemoryBufferHandleDataView data,
         gfx::GpuMemoryBufferHandle* out) {
  if (!data.ReadType(&out->type) || !data.ReadId(&out->id))
    return false;

  mojo::ScopedHandle handle = data.TakeSharedMemoryHandle();
  if (handle.is_valid()) {
    base::PlatformFile platform_file;
    MojoResult unwrap_result = mojo::UnwrapPlatformFile(
        std::move(handle), &platform_file);
    if (unwrap_result != MOJO_RESULT_OK)
      return false;
#if defined(OS_WIN)
    out->handle =
        base::SharedMemoryHandle(platform_file, base::GetCurrentProcId());
#elif defined(OS_MACOSX) || defined(OS_IOS)
    // TODO: Add support for mach_port on mac.
    out->handle = base::SharedMemoryHandle();
#else
    out->handle = base::SharedMemoryHandle(platform_file, true);
#endif
  } else {
    out->handle = base::SharedMemoryHandle();
  }

  out->offset = data.offset();
  out->stride = data.stride();
#if defined(USE_OZONE)
  if (!data.ReadNativePixmapHandle(&out->native_pixmap_handle))
    return false;
#endif
  return true;
}

}  // namespace mojo
