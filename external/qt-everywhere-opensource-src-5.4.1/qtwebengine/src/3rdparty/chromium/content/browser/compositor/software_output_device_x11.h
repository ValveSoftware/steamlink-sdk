// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_X11_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_X11_H_

#include <X11/Xlib.h>

#include "cc/output/software_output_device.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {
class Compositor;
}

namespace content {

class SoftwareOutputDeviceX11 : public cc::SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceX11(ui::Compositor* compositor);

  virtual ~SoftwareOutputDeviceX11();

  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE;

 private:
  ui::Compositor* compositor_;
  XDisplay* display_;
  GC gc_;
  XWindowAttributes attributes_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceX11);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_X11_H_
