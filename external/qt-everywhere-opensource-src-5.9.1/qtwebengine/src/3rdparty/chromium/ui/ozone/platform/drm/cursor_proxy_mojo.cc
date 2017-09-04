// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/cursor_proxy_mojo.h"

#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"

namespace ui {

CursorProxyMojo::CursorProxyMojo(service_manager::Connector* connector)
    : connector_(connector->Clone()) {
  connector->ConnectToInterface(ui::mojom::kServiceName, &main_cursor_ptr_);
}

void CursorProxyMojo::InitializeOnEvdev() {
  evdev_ref_ = base::PlatformThread::CurrentRef();
  connector_->ConnectToInterface(ui::mojom::kServiceName, &evdev_cursor_ptr_);
}

CursorProxyMojo::~CursorProxyMojo() {}

void CursorProxyMojo::CursorSet(gfx::AcceleratedWidget widget,
                                const std::vector<SkBitmap>& bitmaps,
                                const gfx::Point& location,
                                int frame_delay_ms) {
  if (evdev_ref_ == base::PlatformThread::CurrentRef()) {
    evdev_cursor_ptr_->SetCursor(widget, bitmaps, location, frame_delay_ms);
  } else {
    main_cursor_ptr_->SetCursor(widget, bitmaps, location, frame_delay_ms);
  }
}

void CursorProxyMojo::Move(gfx::AcceleratedWidget widget,
                           const gfx::Point& location) {
  if (evdev_ref_ == base::PlatformThread::CurrentRef()) {
    evdev_cursor_ptr_->MoveCursor(widget, location);
  } else {
    main_cursor_ptr_->MoveCursor(widget, location);
  }
}

}  // namespace ui
