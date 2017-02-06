// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_H_
#define UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_H_

#include <memory>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/client_native_pixmap.h"

namespace gfx {
struct NativePixmapHandle;
class Size;
}

namespace ui {

// The Ozone interface allows external implementations to hook into Chromium to
// provide a client pixmap for non-GPU processes.
class OZONE_EXPORT ClientNativePixmapFactory {
 public:
  static ClientNativePixmapFactory* GetInstance();
  static void SetInstance(ClientNativePixmapFactory* instance);

  static std::unique_ptr<ClientNativePixmapFactory> Create();

  virtual ~ClientNativePixmapFactory();

  // Returns true if format/usage configuration is supported.
  virtual bool IsConfigurationSupported(gfx::BufferFormat format,
                                        gfx::BufferUsage usage) const = 0;

  // TODO(dshwang): implement it. crbug.com/475633
  // Import the native pixmap from |handle| to be used in non-GPU processes.
  // This function takes ownership of any file descriptors in |handle|.
  virtual std::unique_ptr<ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) = 0;

 protected:
  ClientNativePixmapFactory();

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_FACTORY_H_
