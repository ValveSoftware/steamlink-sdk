// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_HITTEST_H_
#define CC_SURFACES_SURFACE_HITTEST_H_

#include <set>

#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"

namespace gfx {
class Point;
class Transform;
}

namespace cc {

class DrawQuad;
class QuadList;
class RenderPass;
class RenderPassId;
class SurfaceHittestDelegate;
class SurfaceManager;

// Performs a hittest in surface quads.
class CC_SURFACES_EXPORT SurfaceHittest {
 public:
  SurfaceHittest(SurfaceHittestDelegate* delegate, SurfaceManager* manager);
  ~SurfaceHittest();

  // Returns the target surface that falls underneath the provided |point|.
  // Also returns the |transform| to convert the |point| to the target surface's
  // space.
  SurfaceId GetTargetSurfaceAtPoint(SurfaceId root_surface_id,
                                    const gfx::Point& point,
                                    gfx::Transform* transform);

  // Returns whether the target surface falls inside the provide root surface.
  // Returns the |transform| to convert points from the root surface coordinate
  // space to the target surface coordinate space.
  bool GetTransformToTargetSurface(SurfaceId root_surface_id,
                                   SurfaceId target_surface_id,
                                   gfx::Transform* transform);

 private:
  bool GetTargetSurfaceAtPointInternal(
      SurfaceId surface_id,
      const RenderPassId& render_pass_id,
      const gfx::Point& point_in_root_target,
      std::set<const RenderPass*>* referenced_passes,
      SurfaceId* out_surface_id,
      gfx::Transform* out_transform);

  bool GetTransformToTargetSurfaceInternal(
      SurfaceId root_surface_id,
      SurfaceId target_surface_id,
      const RenderPassId& render_pass_id,
      std::set<const RenderPass*>* referenced_passes,
      gfx::Transform* out_transform);

  const RenderPass* GetRenderPassForSurfaceById(
      SurfaceId surface_id,
      const RenderPassId& render_pass_id);

  bool PointInQuad(const DrawQuad* quad,
                   const gfx::Point& point_in_render_pass_space,
                   gfx::Transform* target_to_quad_transform,
                   gfx::Point* point_in_quad_space);

  SurfaceHittestDelegate* const delegate_;
  SurfaceManager* const manager_;
};
}  // namespace cc

#endif  // CC_SURFACES_SURFACE_HITTEST_H_
