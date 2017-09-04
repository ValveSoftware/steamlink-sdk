// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/resource_manager.h"

#include "base/trace_event/memory_usage_estimator.h"
#include "ui/gfx/geometry/insets_f.h"

namespace ui {

ResourceManager::Resource::Resource() {
}

ResourceManager::Resource::~Resource() {
}

gfx::Rect ResourceManager::Resource::Border(const gfx::Size& bounds) const {
  return Border(bounds, gfx::InsetsF(1.f, 1.f, 1.f, 1.f));
}

gfx::Rect ResourceManager::Resource::Border(const gfx::Size& bounds,
                                            const gfx::InsetsF& scale) const {
  // Calculate whether or not we need to scale down the border if the bounds of
  // the layer are going to be smaller than the aperture padding.
  float x_scale = std::min((float)bounds.width() / size.width(), 1.f);
  float y_scale = std::min((float)bounds.height() / size.height(), 1.f);

  float left_scale = std::min(x_scale * scale.left(), 1.f);
  float right_scale = std::min(x_scale * scale.right(), 1.f);
  float top_scale = std::min(y_scale * scale.top(), 1.f);
  float bottom_scale = std::min(y_scale * scale.bottom(), 1.f);

  return gfx::Rect(aperture.x() * left_scale, aperture.y() * top_scale,
                   (size.width() - aperture.width()) * right_scale,
                   (size.height() - aperture.height()) * bottom_scale);
}

size_t ResourceManager::Resource::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(ui_resource);
}

}  // namespace ui
