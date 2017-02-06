// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_HITTEST_DELEGATE_H_
#define CC_SURFACES_SURFACE_HITTEST_DELEGATE_H_

namespace gfx {
class Point;
}  // namespace gfx

namespace cc {

class SurfaceDrawQuad;

// Clients of SurfaceHittest can provide a SurfaceHittestDelegate implementation
// to override the hit target based on metadata outside of the Surfaces system.
class SurfaceHittestDelegate {
 public:
  // Return true if this delegate rejects this |surface_quad| as a candidate hit
  // target.
  virtual bool RejectHitTarget(const SurfaceDrawQuad* surface_quad,
                               const gfx::Point& point_in_quad_space) = 0;

  // Return true if this delegate accepts this |surface_quad| as a candidate hit
  // target.
  virtual bool AcceptHitTarget(const SurfaceDrawQuad* surface_quad,
                               const gfx::Point& point_in_quad_space) = 0;
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_HITTEST_DELEGATE_H_
