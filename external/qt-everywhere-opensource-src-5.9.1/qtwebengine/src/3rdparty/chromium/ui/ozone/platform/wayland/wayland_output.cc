// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_output.h"

#include <wayland-client.h>

#include "ui/ozone/platform/wayland/wayland_connection.h"

namespace ui {

WaylandOutput::WaylandOutput(wl_output* output)
    : output_(output), rect_(0, 0, 0, 0), observer_(nullptr) {
  static const wl_output_listener output_listener = {
      &WaylandOutput::OutputHandleGeometry, &WaylandOutput::OutputHandleMode,
  };
  wl_output_add_listener(output, &output_listener, this);
}

WaylandOutput::~WaylandOutput() {}

// static
void WaylandOutput::OutputHandleGeometry(void* data,
                                         wl_output* output,
                                         int32_t x,
                                         int32_t y,
                                         int32_t physical_width,
                                         int32_t physical_height,
                                         int32_t subpixel,
                                         const char* make,
                                         const char* model,
                                         int32_t output_transform) {
  WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data);
  wayland_output->rect_.set_origin(gfx::Point(x, y));
}

// static
void WaylandOutput::OutputHandleMode(void* data,
                                     wl_output* wl_output,
                                     uint32_t flags,
                                     int32_t width,
                                     int32_t height,
                                     int32_t refresh) {
  WaylandOutput* output = static_cast<WaylandOutput*>(data);

  if (flags & WL_OUTPUT_MODE_CURRENT) {
    output->rect_.set_width(width);
    output->rect_.set_height(height);

    if (output->observer())
      output->observer()->OnOutputReadyForUse();
  }
}

}  // namespace ui
