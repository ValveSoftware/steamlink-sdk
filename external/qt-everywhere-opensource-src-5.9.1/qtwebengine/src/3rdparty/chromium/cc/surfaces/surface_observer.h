// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_OBSERVER_H_
#define CC_SURFACES_SURFACE_OBSERVER_H_

namespace gfx {
class Size;
}

namespace cc {

class SurfaceId;

class SurfaceObserver {
 public:
  // Runs when a CompositorFrame is received for the given |surface_id| for the
  // first time.
  virtual void OnSurfaceCreated(const SurfaceId& surface_id,
                                const gfx::Size& frame_size,
                                float device_scale_factor) = 0;

  // Runs when a Surface is damaged. *changed should be set to true if this
  // causes a Display to be damaged.
  virtual void OnSurfaceDamaged(const SurfaceId& surface_id, bool* changed) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_OBSERVER_H_
