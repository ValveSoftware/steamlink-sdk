// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_hittest.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_hittest_delegate.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"

namespace cc {

SurfaceHittest::SurfaceHittest(SurfaceHittestDelegate* delegate,
                               SurfaceManager* manager)
    : delegate_(delegate), manager_(manager) {}

SurfaceHittest::~SurfaceHittest() {}

SurfaceId SurfaceHittest::GetTargetSurfaceAtPoint(SurfaceId root_surface_id,
                                                  const gfx::Point& point,
                                                  gfx::Transform* transform) {
  SurfaceId out_surface_id = root_surface_id;

  // Reset the output transform to identity.
  if (transform)
    *transform = gfx::Transform();

  std::set<const RenderPass*> referenced_passes;
  GetTargetSurfaceAtPointInternal(root_surface_id, RenderPassId(), point,
                                  &referenced_passes, &out_surface_id,
                                  transform);

  return out_surface_id;
}

bool SurfaceHittest::GetTransformToTargetSurface(SurfaceId root_surface_id,
                                                 SurfaceId target_surface_id,
                                                 gfx::Transform* transform) {
  // Reset the output transform to identity.
  if (transform)
    *transform = gfx::Transform();

  std::set<const RenderPass*> referenced_passes;
  return GetTransformToTargetSurfaceInternal(root_surface_id, target_surface_id,
                                             RenderPassId(), &referenced_passes,
                                             transform);
}

bool SurfaceHittest::GetTargetSurfaceAtPointInternal(
    SurfaceId surface_id,
    const RenderPassId& render_pass_id,
    const gfx::Point& point_in_root_target,
    std::set<const RenderPass*>* referenced_passes,
    SurfaceId* out_surface_id,
    gfx::Transform* out_transform) {
  const RenderPass* render_pass =
      GetRenderPassForSurfaceById(surface_id, render_pass_id);
  if (!render_pass)
    return false;

  // To avoid an infinite recursion, we need to skip the RenderPass if it's
  // already been referenced.
  if (referenced_passes->find(render_pass) != referenced_passes->end())
    return false;

  referenced_passes->insert(render_pass);

  // The |transform_to_root_target| matrix cannot be inverted if it has a
  // z-scale of 0 or due to floating point errors.
  gfx::Transform transform_from_root_target;
  if (!render_pass->transform_to_root_target.GetInverse(
          &transform_from_root_target)) {
    return false;
  }

  gfx::Point point_in_render_pass_space(point_in_root_target);
  transform_from_root_target.TransformPoint(&point_in_render_pass_space);

  for (const DrawQuad* quad : render_pass->quad_list) {
    gfx::Transform target_to_quad_transform;
    gfx::Point point_in_quad_space;
    if (!PointInQuad(quad, point_in_render_pass_space,
                     &target_to_quad_transform, &point_in_quad_space)) {
      continue;
    }

    if (quad->material == DrawQuad::SURFACE_CONTENT) {
      // We've hit a SurfaceDrawQuad, we need to recurse into this
      // Surface.
      const SurfaceDrawQuad* surface_quad = SurfaceDrawQuad::MaterialCast(quad);

      if (delegate_ &&
          delegate_->RejectHitTarget(surface_quad, point_in_quad_space)) {
        continue;
      }

      gfx::Transform transform_to_child_space;
      if (GetTargetSurfaceAtPointInternal(
              surface_quad->surface_id, RenderPassId(), point_in_quad_space,
              referenced_passes, out_surface_id, &transform_to_child_space)) {
        *out_transform = transform_to_child_space * target_to_quad_transform *
                         transform_from_root_target;
        return true;
      } else if (delegate_ &&
                 delegate_->AcceptHitTarget(surface_quad,
                                            point_in_quad_space)) {
        *out_surface_id = surface_quad->surface_id;
        *out_transform = transform_to_child_space * target_to_quad_transform *
                         transform_from_root_target;
        return true;
      }

      continue;
    }

    if (quad->material == DrawQuad::RENDER_PASS) {
      // We've hit a RenderPassDrawQuad, we need to recurse into this
      // RenderPass.
      const RenderPassDrawQuad* render_quad =
          RenderPassDrawQuad::MaterialCast(quad);

      gfx::Transform transform_to_child_space;
      if (GetTargetSurfaceAtPointInternal(
              surface_id, render_quad->render_pass_id, point_in_root_target,
              referenced_passes, out_surface_id, &transform_to_child_space)) {
        *out_transform = transform_to_child_space;
        return true;
      }

      continue;
    }

    // We've hit a different type of quad in the current Surface. There's no
    // need to iterate anymore, this is the quad that receives the event.
    *out_surface_id = surface_id;
    return true;
  }

  // No quads were found beneath the provided |point|.
  return false;
}

bool SurfaceHittest::GetTransformToTargetSurfaceInternal(
    SurfaceId root_surface_id,
    SurfaceId target_surface_id,
    const RenderPassId& render_pass_id,
    std::set<const RenderPass*>* referenced_passes,
    gfx::Transform* out_transform) {
  if (root_surface_id == target_surface_id) {
    *out_transform = gfx::Transform();
    return true;
  }

  const RenderPass* render_pass =
      GetRenderPassForSurfaceById(root_surface_id, render_pass_id);
  if (!render_pass)
    return false;

  // To avoid an infinite recursion, we need to skip the RenderPass if it's
  // already been referenced.
  if (referenced_passes->find(render_pass) != referenced_passes->end())
    return false;

  referenced_passes->insert(render_pass);

  // The |transform_to_root_target| matrix cannot be inverted if it has a
  // z-scale of 0 or due to floating point errors.
  gfx::Transform transform_from_root_target;
  if (!render_pass->transform_to_root_target.GetInverse(
          &transform_from_root_target)) {
    return false;
  }

  for (const DrawQuad* quad : render_pass->quad_list) {
    if (quad->material == DrawQuad::SURFACE_CONTENT) {
      gfx::Transform target_to_quad_transform;
      if (!quad->shared_quad_state->quad_to_target_transform.GetInverse(
              &target_to_quad_transform)) {
        return false;
      }

      const SurfaceDrawQuad* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      if (surface_quad->surface_id == target_surface_id) {
        *out_transform = target_to_quad_transform * transform_from_root_target;
        return true;
      }

      // This isn't the target surface. Let's recurse deeper to see if we can
      // find the |target_surface_id| there.
      gfx::Transform transform_to_child_space;
      if (GetTransformToTargetSurfaceInternal(
              surface_quad->surface_id, target_surface_id, RenderPassId(),
              referenced_passes, &transform_to_child_space)) {
        *out_transform = transform_to_child_space * target_to_quad_transform *
                         transform_from_root_target;
        return true;
      }
      continue;
    }

    if (quad->material == DrawQuad::RENDER_PASS) {
      // We've hit a RenderPassDrawQuad, we need to recurse into this
      // RenderPass.
      const RenderPassDrawQuad* render_quad =
          RenderPassDrawQuad::MaterialCast(quad);

      gfx::Transform transform_to_child_space;
      if (GetTransformToTargetSurfaceInternal(
              root_surface_id, target_surface_id, render_quad->render_pass_id,
              referenced_passes, &transform_to_child_space)) {
        *out_transform = transform_to_child_space;
        return true;
      }

      continue;
    }
  }

  // The target surface was not found.
  return false;
}

const RenderPass* SurfaceHittest::GetRenderPassForSurfaceById(
    SurfaceId surface_id,
    const RenderPassId& render_pass_id) {
  Surface* surface = manager_->GetSurfaceForId(surface_id);
  if (!surface)
    return nullptr;

  const CompositorFrame& surface_frame = surface->GetEligibleFrame();
  if (!surface_frame.delegated_frame_data)
    return nullptr;

  const DelegatedFrameData* frame_data =
      surface_frame.delegated_frame_data.get();
  if (frame_data->render_pass_list.empty())
    return nullptr;

  if (!render_pass_id.IsValid())
    return frame_data->render_pass_list.back().get();

  for (const auto& render_pass : frame_data->render_pass_list) {
    if (render_pass->id == render_pass_id)
      return render_pass.get();
  }

  return nullptr;
}

bool SurfaceHittest::PointInQuad(const DrawQuad* quad,
                                 const gfx::Point& point_in_render_pass_space,
                                 gfx::Transform* target_to_quad_transform,
                                 gfx::Point* point_in_quad_space) {
  // First we test against the clip_rect. The clip_rect is in target space, so
  // we can test the point directly.
  if (quad->shared_quad_state->is_clipped &&
      !quad->shared_quad_state->clip_rect.Contains(
          point_in_render_pass_space)) {
    return false;
  }

  // We now transform the point to content space and test if it hits the
  // rect.
  if (!quad->shared_quad_state->quad_to_target_transform.GetInverse(
          target_to_quad_transform)) {
    return false;
  }

  *point_in_quad_space = point_in_render_pass_space;
  target_to_quad_transform->TransformPoint(point_in_quad_space);

  return quad->rect.Contains(*point_in_quad_space);
}

}  // namespace cc
