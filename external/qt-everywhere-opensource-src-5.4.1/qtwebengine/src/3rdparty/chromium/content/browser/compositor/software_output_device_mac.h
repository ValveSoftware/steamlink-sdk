// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_

#include "cc/output/software_output_device.h"

namespace gfx {
class Canvas;
}

namespace ui {
class Compositor;
}

namespace content {

class SoftwareOutputDeviceMac : public cc::SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceMac(ui::Compositor* compositor);
  virtual ~SoftwareOutputDeviceMac();

  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE;

 private:
  ui::Compositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_
