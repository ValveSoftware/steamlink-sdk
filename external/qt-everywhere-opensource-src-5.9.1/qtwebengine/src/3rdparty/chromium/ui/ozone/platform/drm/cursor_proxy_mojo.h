// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_CURSOR_PROXY_MOJO_H_
#define UI_OZONE_PLATFORM_DRM_CURSOR_PROXY_MOJO_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/gpu/inter_thread_messaging_proxy.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/public/interfaces/device_cursor.mojom.h"

namespace service_manager {
class Connector;
}

namespace ui {

// Ozone requires a IPC from the browser (or mus-ws) process to the gpu (or
// mus-gpu) process to control the mouse pointer. This class provides mouse
// pointer control via Mojo-style IPC. This code runs only in the mus-ws (i.e.
// it's the client) and sends mouse pointer control messages to a less
// priviledged process.
class CursorProxyMojo : public DrmCursorProxy {
 public:
  explicit CursorProxyMojo(service_manager::Connector* connector);
  ~CursorProxyMojo() override;

 private:
  // DrmCursorProxy.
  void CursorSet(gfx::AcceleratedWidget window,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& point,
                 int frame_delay_ms) override;
  void Move(gfx::AcceleratedWidget window, const gfx::Point& point) override;
  void InitializeOnEvdev() override;

  std::unique_ptr<service_manager::Connector> connector_;

  // Mojo implementation of the DrmCursorProxy.
  ui::ozone::mojom::DeviceCursorPtr main_cursor_ptr_;
  ui::ozone::mojom::DeviceCursorPtr evdev_cursor_ptr_;

  base::PlatformThreadRef evdev_ref_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_CURSOR_PROXY_MOJO_H_
