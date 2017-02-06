// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/client_native_pixmap_factory_gbm.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/native_pixmap_handle_ozone.h"
#include "ui/ozone/platform/drm/common/client_native_pixmap_dmabuf.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"

namespace ui {

namespace {

class ClientNativePixmapGbm : public ClientNativePixmap {
 public:
  ClientNativePixmapGbm() {}
  ~ClientNativePixmapGbm() override {}

  void* Map() override {
    NOTREACHED();
    return nullptr;
  }
  void Unmap() override { NOTREACHED(); }
  void GetStride(int* stride) const override { NOTREACHED(); }
};

}  // namespace

class ClientNativePixmapFactoryGbm : public ClientNativePixmapFactory {
 public:
  ClientNativePixmapFactoryGbm() {}
  ~ClientNativePixmapFactoryGbm() override {}

  // ClientNativePixmapFactory:
  bool IsConfigurationSupported(gfx::BufferFormat format,
                                gfx::BufferUsage usage) const override {
    switch (usage) {
      case gfx::BufferUsage::GPU_READ:
        return format == gfx::BufferFormat::BGR_565 ||
               format == gfx::BufferFormat::RGBA_8888 ||
               format == gfx::BufferFormat::RGBX_8888 ||
               format == gfx::BufferFormat::BGRA_8888 ||
               format == gfx::BufferFormat::BGRX_8888 ||
               format == gfx::BufferFormat::YVU_420;
      case gfx::BufferUsage::SCANOUT:
        return format == gfx::BufferFormat::BGRX_8888;
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT: {
#if defined(OS_CHROMEOS)
        return
#if defined(ARCH_CPU_X86_FAMILY)
            // Currently only Intel driver (i.e. minigbm and Mesa) supports R_8.
            // crbug.com/356871
            format == gfx::BufferFormat::R_8 ||
#endif
            format == gfx::BufferFormat::BGRA_8888;
#else
        return false;
#endif
      }
    }
    NOTREACHED();
    return false;
  }
  std::unique_ptr<ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) override {
    DCHECK(!handle.fds.empty());
    base::ScopedFD scoped_fd(handle.fds[0].fd);
    switch (usage) {
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
#if defined(OS_CHROMEOS)
        // TODO(dcastagna): Add support for pixmaps with multiple FDs for non
        // scanout buffers.
        return ClientNativePixmapDmaBuf::ImportFromDmabuf(
            scoped_fd.release(), size, handle.strides_and_offsets[0].first);
#else
        NOTREACHED();
        return nullptr;
#endif
      case gfx::BufferUsage::GPU_READ:
      case gfx::BufferUsage::SCANOUT:
        return base::WrapUnique(new ClientNativePixmapGbm);
    }
    NOTREACHED();
    return nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactoryGbm);
};

ClientNativePixmapFactory* CreateClientNativePixmapFactoryGbm() {
  return new ClientNativePixmapFactoryGbm();
}

}  // namespace ui
