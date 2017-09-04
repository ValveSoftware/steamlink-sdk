// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_SURFACE_ID_HANDLER_H_
#define UI_AURA_MUS_SURFACE_ID_HANDLER_H_

#include "cc/surfaces/surface_id.h"
#include "ui/gfx/geometry/size.h"

namespace aura {

class Window;

// Holds information about the current surface held by a Window.
// |surface_id| uniquely identifies the surface in the display
// compositor.
// |frame_size| is the size of the frame held by the surface.
// |device_scale_factor| is the scale factor that the frame was
// renderered for.
struct SurfaceInfo {
  cc::SurfaceId surface_id;
  gfx::Size frame_size;
  float device_scale_factor;
};

class SurfaceIdHandler {
 public:
  // Called when a child window allocates a new surface ID.
  // If the handler wishes to retain ownership of the |surface_info|,
  // it can move it. If a child's surface has been cleared then
  // |surface_info| will refer to a null pointer.
  virtual void OnChildWindowSurfaceChanged(
      Window* window,
      std::unique_ptr<SurfaceInfo>* surface_info) = 0;
};

}  // namespace aura

#endif  // UI_AURA_MUS_SURFACE_ID_HANDLER_H_
