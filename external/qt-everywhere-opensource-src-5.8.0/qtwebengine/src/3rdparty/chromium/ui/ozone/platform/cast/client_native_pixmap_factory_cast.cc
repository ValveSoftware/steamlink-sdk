// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/client_native_pixmap_factory_cast.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/ozone/public/client_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory.h"

namespace ui {
namespace {

// Dummy ClientNativePixmap implementation for Cast ozone.
// Our NativePixmaps are just used to plumb an overlay frame through,
// so they get instantiated, but not used.
class ClientNativePixmapCast : public ClientNativePixmap {
 public:
  // ClientNativePixmap implementation:
  void* Map() override {
    NOTREACHED();
    return nullptr;
  }
  void Unmap() override { NOTREACHED(); }
  void GetStride(int* stride) const override { NOTREACHED(); }
};

class ClientNativePixmapFactoryCast : public ClientNativePixmapFactory {
 public:
  // ClientNativePixmapFactoryCast implementation:
  bool IsConfigurationSupported(gfx::BufferFormat format,
                                gfx::BufferUsage usage) const override {
    return format == gfx::BufferFormat::RGBA_8888 &&
           usage == gfx::BufferUsage::SCANOUT;
  }

  std::unique_ptr<ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) override {
    return base::WrapUnique(new ClientNativePixmapCast());
  }
};

}  // namespace

ClientNativePixmapFactory* CreateClientNativePixmapFactoryCast() {
  return new ClientNativePixmapFactoryCast();
}

}  // namespace ui
