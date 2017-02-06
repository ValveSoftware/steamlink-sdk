// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_OZONE_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_OZONE_H_

#include <memory>

#include "base/macros.h"
#include "cc/output/software_output_device.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class Compositor;
class SurfaceOzoneCanvas;
}

namespace content {

// Ozone implementation which relies on software rendering. Ozone will present
// an accelerated widget as a SkCanvas. SoftwareOutputDevice will then use the
// Ozone provided canvas to draw.
class CONTENT_EXPORT SoftwareOutputDeviceOzone
    : public cc::SoftwareOutputDevice {
 public:
  static std::unique_ptr<SoftwareOutputDeviceOzone> Create(
      ui::Compositor* compositor);
  ~SoftwareOutputDeviceOzone() override;

  void Resize(const gfx::Size& viewport_pixel_size,
                      float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;

 private:
  explicit SoftwareOutputDeviceOzone(ui::Compositor* compositor);
  ui::Compositor* compositor_;

  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceOzone);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_OZONE_H_
