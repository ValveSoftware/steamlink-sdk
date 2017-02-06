// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MUS_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MUS_H_

#include "base/macros.h"
#include "cc/output/software_output_device.h"
#include "content/common/content_export.h"

namespace ui {
class Compositor;
}

namespace content {

// Mus implementation of software compositing: Chrome will do a software
// composite and ship the resultant bitmap to an instance of the mus
// window server. Remove this upon completion of http://crbug.com/548451
class SoftwareOutputDeviceMus : public cc::SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceMus(ui::Compositor* compositor);

 private:
  // cc::SoftwareOutputDevice
  void EndPaint() override;

  ui::Compositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceMus);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MUS_H_
