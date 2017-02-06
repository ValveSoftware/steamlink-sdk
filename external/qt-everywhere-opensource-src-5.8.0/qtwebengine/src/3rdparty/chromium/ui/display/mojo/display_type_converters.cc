// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojo/display_type_converters.h"

#include "ui/display/display.h"

namespace mojo {

// static
display::Display
TypeConverter<display::Display, mus::mojom::DisplayPtr>::Convert(
    const mus::mojom::DisplayPtr& input) {
  if (input.is_null())
    return display::Display();

  display::Display result(input->id);
  gfx::Rect pixel_bounds = input->bounds;
  gfx::Rect pixel_work_area = input->work_area;
  float pixel_ratio = input->device_pixel_ratio;

  gfx::Rect dip_bounds =
      gfx::ScaleToEnclosingRect(pixel_bounds, 1.f / pixel_ratio);
  gfx::Rect dip_work_area =
      gfx::ScaleToEnclosingRect(pixel_work_area, 1.f / pixel_ratio);
  result.set_bounds(dip_bounds);
  result.set_work_area(dip_work_area);
  result.set_device_scale_factor(input->device_pixel_ratio);

  switch (input->rotation) {
    case mus::mojom::Rotation::VALUE_0:
      result.set_rotation(display::Display::ROTATE_0);
      break;
    case mus::mojom::Rotation::VALUE_90:
      result.set_rotation(display::Display::ROTATE_90);
      break;
    case mus::mojom::Rotation::VALUE_180:
      result.set_rotation(display::Display::ROTATE_180);
      break;
    case mus::mojom::Rotation::VALUE_270:
      result.set_rotation(display::Display::ROTATE_270);
      break;
  }
  switch (input->touch_support) {
    case mus::mojom::TouchSupport::UNKNOWN:
      result.set_touch_support(display::Display::TOUCH_SUPPORT_UNKNOWN);
      break;
    case mus::mojom::TouchSupport::AVAILABLE:
      result.set_touch_support(display::Display::TOUCH_SUPPORT_AVAILABLE);
      break;
    case mus::mojom::TouchSupport::UNAVAILABLE:
      result.set_touch_support(display::Display::TOUCH_SUPPORT_UNAVAILABLE);
      break;
  }
  return result;
}

}  // namespace mojo
