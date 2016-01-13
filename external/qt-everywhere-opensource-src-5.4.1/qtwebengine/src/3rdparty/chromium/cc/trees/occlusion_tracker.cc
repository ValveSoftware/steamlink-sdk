// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/occlusion_tracker.h"

#include <algorithm>

#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

template <typename LayerType>
OcclusionTracker<LayerType>::OcclusionTracker(
    const gfx::Rect& screen_space_clip_rect)
    : screen_space_clip_rect_(screen_space_clip_rect),
      occluding_screen_space_rects_(NULL),
      non_occluding_screen_space_rects_(NULL) {}

template <typename LayerType>
OcclusionTracker<LayerType>::~OcclusionTracker() {}

template <typename LayerType>
void OcclusionTracker<LayerType>::EnterLayer(
    const LayerIteratorPosition<LayerType>& layer_iterator) {
  LayerType* render_target = layer_iterator.target_render_surface_layer;

  if (layer_iterator.represents_itself)
    EnterRenderTarget(render_target);
  else if (layer_iterator.represents_target_render_surface)
    FinishedRenderTarget(render_target);
}

template <typename LayerType>
void OcclusionTracker<LayerType>::LeaveLayer(
    const LayerIteratorPosition<LayerType>& layer_iterator) {
  LayerType* render_target = layer_iterator.target_render_surface_layer;

  if (layer_iterator.represents_itself)
    MarkOccludedBehindLayer(layer_iterator.current_layer);
  // TODO(danakj): This should be done when entering the contributing surface,
  // but in a way that the surface's own occlusion won't occlude itself.
  else if (layer_iterator.represents_contributing_render_surface)
    LeaveToRenderTarget(render_target);
}

template <typename RenderSurfaceType>
static gfx::Rect ScreenSpaceClipRectInTargetSurface(
    const RenderSurfaceType* target_surface,
    const gfx::Rect& screen_space_clip_rect) {
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (!target_surface->screen_space_transform().GetInverse(
          &inverse_screen_space_transform))
    return target_surface->content_rect();

  return MathUtil::ProjectEnclosingClippedRect(inverse_screen_space_transform,
                                               screen_space_clip_rect);
}

template <typename RenderSurfaceType>
static Region TransformSurfaceOpaqueRegion(
    const Region& region,
    bool have_clip_rect,
    const gfx::Rect& clip_rect_in_new_target,
    const gfx::Transform& transform) {
  if (region.IsEmpty())
    return Region();

  // Verify that rects within the |surface| will remain rects in its target
  // surface after applying |transform|. If this is true, then apply |transform|
  // to each rect within |region| in order to transform the entire Region.

  // TODO(danakj): Find a rect interior to each transformed quad.
  if (!transform.Preserves2dAxisAlignment())
    return Region();

  // TODO(danakj): If the Region is too complex, degrade gracefully here by
  // skipping rects in it.
  Region transformed_region;
  for (Region::Iterator rects(region); rects.has_rect(); rects.next()) {
    bool clipped;
    gfx::QuadF transformed_quad =
        MathUtil::MapQuad(transform, gfx::QuadF(rects.rect()), &clipped);
    gfx::Rect transformed_rect =
        gfx::ToEnclosedRect(transformed_quad.BoundingBox());
    DCHECK(!clipped);  // We only map if the transform preserves axis alignment.
    if (have_clip_rect)
      transformed_rect.Intersect(clip_rect_in_new_target);
    transformed_region.Union(transformed_rect);
  }
  return transformed_region;
}

static inline bool LayerOpacityKnown(const Layer* layer) {
  return !layer->draw_opacity_is_animating();
}
static inline bool LayerOpacityKnown(const LayerImpl* layer) {
  return true;
}
static inline bool LayerTransformsToTargetKnown(const Layer* layer) {
  return !layer->draw_transform_is_animating();
}
static inline bool LayerTransformsToTargetKnown(const LayerImpl* layer) {
  return true;
}

static inline bool SurfaceOpacityKnown(const RenderSurface* rs) {
  return !rs->draw_opacity_is_animating();
}
static inline bool SurfaceOpacityKnown(const RenderSurfaceImpl* rs) {
  return true;
}
static inline bool SurfaceTransformsToTargetKnown(const RenderSurface* rs) {
  return !rs->target_surface_transforms_are_animating();
}
static inline bool SurfaceTransformsToTargetKnown(const RenderSurfaceImpl* rs) {
  return true;
}
static inline bool SurfaceTransformsToScreenKnown(const RenderSurface* rs) {
  return !rs->screen_space_transforms_are_animating();
}
static inline bool SurfaceTransformsToScreenKnown(const RenderSurfaceImpl* rs) {
  return true;
}

static inline bool LayerIsInUnsorted3dRenderingContext(const Layer* layer) {
  return layer->Is3dSorted();
}
static inline bool LayerIsInUnsorted3dRenderingContext(const LayerImpl* layer) {
  return false;
}

template <typename LayerType>
static inline bool LayerIsHidden(const LayerType* layer) {
  return layer->hide_layer_and_subtree() ||
         (layer->parent() && LayerIsHidden(layer->parent()));
}

template <typename LayerType>
void OcclusionTracker<LayerType>::EnterRenderTarget(
    const LayerType* new_target) {
  if (!stack_.empty() && stack_.back().target == new_target)
    return;

  const LayerType* old_target = NULL;
  const typename LayerType::RenderSurfaceType* old_occlusion_immune_ancestor =
      NULL;
  if (!stack_.empty()) {
    old_target = stack_.back().target;
    old_occlusion_immune_ancestor =
        old_target->render_surface()->nearest_occlusion_immune_ancestor();
  }
  const typename LayerType::RenderSurfaceType* new_occlusion_immune_ancestor =
      new_target->render_surface()->nearest_occlusion_immune_ancestor();

  stack_.push_back(StackObject(new_target));

  // We copy the screen occlusion into the new RenderSurface subtree, but we
  // never copy in the occlusion from inside the target, since we are looking
  // at a new RenderSurface target.

  // If entering an unoccluded subtree, do not carry forward the outside
  // occlusion calculated so far.
  bool entering_unoccluded_subtree =
      new_occlusion_immune_ancestor &&
      new_occlusion_immune_ancestor != old_occlusion_immune_ancestor;

  bool have_transform_from_screen_to_new_target = false;
  gfx::Transform inverse_new_target_screen_space_transform(
      // Note carefully, not used if screen space transform is uninvertible.
      gfx::Transform::kSkipInitialization);
  if (SurfaceTransformsToScreenKnown(new_target->render_surface())) {
    have_transform_from_screen_to_new_target =
        new_target->render_surface()->screen_space_transform().GetInverse(
            &inverse_new_target_screen_space_transform);
  }

  bool entering_root_target = new_target->parent() == NULL;

  bool copy_outside_occlusion_forward =
      stack_.size() > 1 &&
      !entering_unoccluded_subtree &&
      have_transform_from_screen_to_new_target &&
      !entering_root_target;
  if (!copy_outside_occlusion_forward)
    return;

  int last_index = stack_.size() - 1;
  gfx::Transform old_target_to_new_target_transform(
      inverse_new_target_screen_space_transform,
      old_target->render_surface()->screen_space_transform());
  stack_[last_index].occlusion_from_outside_target =
      TransformSurfaceOpaqueRegion<typename LayerType::RenderSurfaceType>(
          stack_[last_index - 1].occlusion_from_outside_target,
          false,
          gfx::Rect(),
          old_target_to_new_target_transform);
  stack_[last_index].occlusion_from_outside_target.Union(
      TransformSurfaceOpaqueRegion<typename LayerType::RenderSurfaceType>(
          stack_[last_index - 1].occlusion_from_inside_target,
          false,
          gfx::Rect(),
          old_target_to_new_target_transform));
}

template <typename LayerType>
void OcclusionTracker<LayerType>::FinishedRenderTarget(
    const LayerType* finished_target) {
  // Make sure we know about the target surface.
  EnterRenderTarget(finished_target);

  typename LayerType::RenderSurfaceType* surface =
      finished_target->render_surface();

  // Readbacks always happen on render targets so we only need to check
  // for readbacks here.
  bool target_is_only_for_copy_request =
      finished_target->HasCopyRequest() && LayerIsHidden(finished_target);

  // If the occlusion within the surface can not be applied to things outside of
  // the surface's subtree, then clear the occlusion here so it won't be used.
  if (finished_target->mask_layer() || !SurfaceOpacityKnown(surface) ||
      surface->draw_opacity() < 1 ||
      !finished_target->uses_default_blend_mode() ||
      target_is_only_for_copy_request ||
      finished_target->filters().HasFilterThatAffectsOpacity()) {
    stack_.back().occlusion_from_outside_target.Clear();
    stack_.back().occlusion_from_inside_target.Clear();
  } else if (!SurfaceTransformsToTargetKnown(surface)) {
    stack_.back().occlusion_from_inside_target.Clear();
    stack_.back().occlusion_from_outside_target.Clear();
  }
}

template <typename LayerType>
static void ReduceOcclusionBelowSurface(LayerType* contributing_layer,
                                        const gfx::Rect& surface_rect,
                                        const gfx::Transform& surface_transform,
                                        LayerType* render_target,
                                        Region* occlusion_from_inside_target) {
  if (surface_rect.IsEmpty())
    return;

  gfx::Rect affected_area_in_target =
      MathUtil::MapEnclosingClippedRect(surface_transform, surface_rect);
  if (contributing_layer->render_surface()->is_clipped()) {
    affected_area_in_target.Intersect(
        contributing_layer->render_surface()->clip_rect());
  }
  if (affected_area_in_target.IsEmpty())
    return;

  int outset_top, outset_right, outset_bottom, outset_left;
  contributing_layer->background_filters().GetOutsets(
      &outset_top, &outset_right, &outset_bottom, &outset_left);

  // The filter can move pixels from outside of the clip, so allow affected_area
  // to expand outside the clip.
  affected_area_in_target.Inset(
      -outset_left, -outset_top, -outset_right, -outset_bottom);
  Region affected_occlusion = IntersectRegions(*occlusion_from_inside_target,
                                               affected_area_in_target);
  Region::Iterator affected_occlusion_rects(affected_occlusion);

  occlusion_from_inside_target->Subtract(affected_area_in_target);
  for (; affected_occlusion_rects.has_rect(); affected_occlusion_rects.next()) {
    gfx::Rect occlusion_rect = affected_occlusion_rects.rect();

    // Shrink the rect by expanding the non-opaque pixels outside the rect.

    // The left outset of the filters moves pixels on the right side of
    // the occlusion_rect into it, shrinking its right edge.
    int shrink_left =
        occlusion_rect.x() == affected_area_in_target.x() ? 0 : outset_right;
    int shrink_top =
        occlusion_rect.y() == affected_area_in_target.y() ? 0 : outset_bottom;
    int shrink_right =
        occlusion_rect.right() == affected_area_in_target.right() ?
        0 : outset_left;
    int shrink_bottom =
        occlusion_rect.bottom() == affected_area_in_target.bottom() ?
        0 : outset_top;

    occlusion_rect.Inset(shrink_left, shrink_top, shrink_right, shrink_bottom);

    occlusion_from_inside_target->Union(occlusion_rect);
  }
}

template <typename LayerType>
void OcclusionTracker<LayerType>::LeaveToRenderTarget(
    const LayerType* new_target) {
  int last_index = stack_.size() - 1;
  bool surface_will_be_at_top_after_pop =
      stack_.size() > 1 && stack_[last_index - 1].target == new_target;

  // We merge the screen occlusion from the current RenderSurfaceImpl subtree
  // out to its parent target RenderSurfaceImpl. The target occlusion can be
  // merged out as well but needs to be transformed to the new target.

  const LayerType* old_target = stack_[last_index].target;
  const typename LayerType::RenderSurfaceType* old_surface =
      old_target->render_surface();

  Region old_occlusion_from_inside_target_in_new_target =
      TransformSurfaceOpaqueRegion<typename LayerType::RenderSurfaceType>(
          stack_[last_index].occlusion_from_inside_target,
          old_surface->is_clipped(),
          old_surface->clip_rect(),
          old_surface->draw_transform());
  if (old_target->has_replica() && !old_target->replica_has_mask()) {
    old_occlusion_from_inside_target_in_new_target.Union(
        TransformSurfaceOpaqueRegion<typename LayerType::RenderSurfaceType>(
            stack_[last_index].occlusion_from_inside_target,
            old_surface->is_clipped(),
            old_surface->clip_rect(),
            old_surface->replica_draw_transform()));
  }

  Region old_occlusion_from_outside_target_in_new_target =
      TransformSurfaceOpaqueRegion<typename LayerType::RenderSurfaceType>(
          stack_[last_index].occlusion_from_outside_target,
          false,
          gfx::Rect(),
          old_surface->draw_transform());

  gfx::Rect unoccluded_surface_rect;
  gfx::Rect unoccluded_replica_rect;
  if (old_target->background_filters().HasFilterThatMovesPixels()) {
    unoccluded_surface_rect = UnoccludedContributingSurfaceContentRect(
        old_surface->content_rect(), old_surface->draw_transform());
    if (old_target->has_replica()) {
      unoccluded_replica_rect = UnoccludedContributingSurfaceContentRect(
          old_surface->content_rect(),
          old_surface->replica_draw_transform());
    }
  }

  if (surface_will_be_at_top_after_pop) {
    // Merge the top of the stack down.
    stack_[last_index - 1].occlusion_from_inside_target.Union(
        old_occlusion_from_inside_target_in_new_target);
    // TODO(danakj): Strictly this should subtract the inside target occlusion
    // before union.
    if (new_target->parent()) {
      stack_[last_index - 1].occlusion_from_outside_target.Union(
          old_occlusion_from_outside_target_in_new_target);
    }
    stack_.pop_back();
  } else {
    // Replace the top of the stack with the new pushed surface.
    stack_.back().target = new_target;
    stack_.back().occlusion_from_inside_target =
        old_occlusion_from_inside_target_in_new_target;
    if (new_target->parent()) {
      stack_.back().occlusion_from_outside_target =
          old_occlusion_from_outside_target_in_new_target;
    } else {
      stack_.back().occlusion_from_outside_target.Clear();
    }
  }

  if (!old_target->background_filters().HasFilterThatMovesPixels())
    return;

  ReduceOcclusionBelowSurface(old_target,
                              unoccluded_surface_rect,
                              old_surface->draw_transform(),
                              new_target,
                              &stack_.back().occlusion_from_inside_target);
  ReduceOcclusionBelowSurface(old_target,
                              unoccluded_surface_rect,
                              old_surface->draw_transform(),
                              new_target,
                              &stack_.back().occlusion_from_outside_target);

  if (!old_target->has_replica())
    return;
  ReduceOcclusionBelowSurface(old_target,
                              unoccluded_replica_rect,
                              old_surface->replica_draw_transform(),
                              new_target,
                              &stack_.back().occlusion_from_inside_target);
  ReduceOcclusionBelowSurface(old_target,
                              unoccluded_replica_rect,
                              old_surface->replica_draw_transform(),
                              new_target,
                              &stack_.back().occlusion_from_outside_target);
}

template <typename LayerType>
void OcclusionTracker<LayerType>::MarkOccludedBehindLayer(
    const LayerType* layer) {
  DCHECK(!stack_.empty());
  DCHECK_EQ(layer->render_target(), stack_.back().target);
  if (stack_.empty())
    return;

  if (!LayerOpacityKnown(layer) || layer->draw_opacity() < 1)
    return;

  if (!layer->uses_default_blend_mode())
    return;

  if (LayerIsInUnsorted3dRenderingContext(layer))
    return;

  if (!LayerTransformsToTargetKnown(layer))
    return;

  Region opaque_contents = layer->VisibleContentOpaqueRegion();
  if (opaque_contents.IsEmpty())
    return;

  DCHECK(layer->visible_content_rect().Contains(opaque_contents.bounds()));

  // TODO(danakj): Find a rect interior to each transformed quad.
  if (!layer->draw_transform().Preserves2dAxisAlignment())
    return;

  gfx::Rect clip_rect_in_target = ScreenSpaceClipRectInTargetSurface(
      layer->render_target()->render_surface(), screen_space_clip_rect_);
  if (layer->is_clipped()) {
    clip_rect_in_target.Intersect(layer->clip_rect());
  } else {
    clip_rect_in_target.Intersect(
        layer->render_target()->render_surface()->content_rect());
  }

  for (Region::Iterator opaque_content_rects(opaque_contents);
       opaque_content_rects.has_rect();
       opaque_content_rects.next()) {
    bool clipped;
    gfx::QuadF transformed_quad = MathUtil::MapQuad(
        layer->draw_transform(),
        gfx::QuadF(opaque_content_rects.rect()),
        &clipped);
    gfx::Rect transformed_rect =
        gfx::ToEnclosedRect(transformed_quad.BoundingBox());
    DCHECK(!clipped);  // We only map if the transform preserves axis alignment.
    transformed_rect.Intersect(clip_rect_in_target);
    if (transformed_rect.width() < minimum_tracking_size_.width() &&
        transformed_rect.height() < minimum_tracking_size_.height())
      continue;
    stack_.back().occlusion_from_inside_target.Union(transformed_rect);

    if (!occluding_screen_space_rects_)
      continue;

    // Save the occluding area in screen space for debug visualization.
    gfx::QuadF screen_space_quad = MathUtil::MapQuad(
        layer->render_target()->render_surface()->screen_space_transform(),
        gfx::QuadF(transformed_rect), &clipped);
    // TODO(danakj): Store the quad in the debug info instead of the bounding
    // box.
    gfx::Rect screen_space_rect =
        gfx::ToEnclosedRect(screen_space_quad.BoundingBox());
    occluding_screen_space_rects_->push_back(screen_space_rect);
  }

  if (!non_occluding_screen_space_rects_)
    return;

  Region non_opaque_contents =
      SubtractRegions(gfx::Rect(layer->content_bounds()), opaque_contents);
  for (Region::Iterator non_opaque_content_rects(non_opaque_contents);
       non_opaque_content_rects.has_rect();
       non_opaque_content_rects.next()) {
    // We've already checked for clipping in the MapQuad call above, these calls
    // should not clip anything further.
    gfx::Rect transformed_rect = gfx::ToEnclosedRect(
        MathUtil::MapClippedRect(layer->draw_transform(),
                                 gfx::RectF(non_opaque_content_rects.rect())));
    transformed_rect.Intersect(clip_rect_in_target);
    if (transformed_rect.IsEmpty())
      continue;

    bool clipped;
    gfx::QuadF screen_space_quad = MathUtil::MapQuad(
        layer->render_target()->render_surface()->screen_space_transform(),
        gfx::QuadF(transformed_rect),
        &clipped);
    // TODO(danakj): Store the quad in the debug info instead of the bounding
    // box.
    gfx::Rect screen_space_rect =
        gfx::ToEnclosedRect(screen_space_quad.BoundingBox());
    non_occluding_screen_space_rects_->push_back(screen_space_rect);
  }
}

template <typename LayerType>
bool OcclusionTracker<LayerType>::Occluded(
    const LayerType* render_target,
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) const {
  DCHECK(!stack_.empty());
  if (stack_.empty())
    return false;
  if (content_rect.IsEmpty())
    return true;

  // For tests with no render target.
  if (!render_target)
    return false;

  DCHECK_EQ(render_target->render_target(), render_target);
  DCHECK(render_target->render_surface());
  DCHECK_EQ(render_target, stack_.back().target);

  if (stack_.back().occlusion_from_inside_target.IsEmpty() &&
      stack_.back().occlusion_from_outside_target.IsEmpty()) {
    return false;
  }

  gfx::Transform inverse_draw_transform(gfx::Transform::kSkipInitialization);
  if (!draw_transform.GetInverse(&inverse_draw_transform))
    return false;

  // Take the ToEnclosingRect at each step, as we want to contain any unoccluded
  // partial pixels in the resulting Rect.
  Region unoccluded_region_in_target_surface =
      MathUtil::MapEnclosingClippedRect(draw_transform, content_rect);
  unoccluded_region_in_target_surface.Subtract(
      stack_.back().occlusion_from_inside_target);
  gfx::RectF unoccluded_rect_in_target_surface_without_outside_occlusion =
      unoccluded_region_in_target_surface.bounds();
  unoccluded_region_in_target_surface.Subtract(
      stack_.back().occlusion_from_outside_target);

  gfx::RectF unoccluded_rect_in_target_surface =
      unoccluded_region_in_target_surface.bounds();

  return unoccluded_rect_in_target_surface.IsEmpty();
}

template <typename LayerType>
gfx::Rect OcclusionTracker<LayerType>::UnoccludedContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) const {
  if (stack_.empty())
    return content_rect;
  if (content_rect.IsEmpty())
    return content_rect;

  if (stack_.back().occlusion_from_inside_target.IsEmpty() &&
      stack_.back().occlusion_from_outside_target.IsEmpty()) {
    return content_rect;
  }

  gfx::Transform inverse_draw_transform(gfx::Transform::kSkipInitialization);
  if (!draw_transform.GetInverse(&inverse_draw_transform))
    return content_rect;

  // Take the ToEnclosingRect at each step, as we want to contain any unoccluded
  // partial pixels in the resulting Rect.
  Region unoccluded_region_in_target_surface =
      MathUtil::MapEnclosingClippedRect(draw_transform, content_rect);
  unoccluded_region_in_target_surface.Subtract(
      stack_.back().occlusion_from_inside_target);
  unoccluded_region_in_target_surface.Subtract(
      stack_.back().occlusion_from_outside_target);

  gfx::Rect unoccluded_rect_in_target_surface =
      unoccluded_region_in_target_surface.bounds();
  gfx::Rect unoccluded_rect = MathUtil::ProjectEnclosingClippedRect(
      inverse_draw_transform, unoccluded_rect_in_target_surface);
  unoccluded_rect.Intersect(content_rect);

  return unoccluded_rect;
}

template <typename LayerType>
gfx::Rect OcclusionTracker<LayerType>::UnoccludedContributingSurfaceContentRect(
    const gfx::Rect& content_rect,
    const gfx::Transform& draw_transform) const {
  if (content_rect.IsEmpty())
    return content_rect;

  // A contributing surface doesn't get occluded by things inside its own
  // surface, so only things outside the surface can occlude it. That occlusion
  // is found just below the top of the stack (if it exists).
  bool has_occlusion = stack_.size() > 1;
  if (!has_occlusion)
    return content_rect;

  const StackObject& second_last = stack_[stack_.size() - 2];

  if (second_last.occlusion_from_inside_target.IsEmpty() &&
      second_last.occlusion_from_outside_target.IsEmpty())
    return content_rect;

  gfx::Transform inverse_draw_transform(gfx::Transform::kSkipInitialization);
  if (!draw_transform.GetInverse(&inverse_draw_transform))
    return content_rect;

  // Take the ToEnclosingRect at each step, as we want to contain any unoccluded
  // partial pixels in the resulting Rect.
  Region unoccluded_region_in_target_surface =
      MathUtil::MapEnclosingClippedRect(draw_transform, content_rect);
  unoccluded_region_in_target_surface.Subtract(
      second_last.occlusion_from_inside_target);
  unoccluded_region_in_target_surface.Subtract(
      second_last.occlusion_from_outside_target);

  gfx::Rect unoccluded_rect_in_target_surface =
      unoccluded_region_in_target_surface.bounds();
  gfx::Rect unoccluded_rect = MathUtil::ProjectEnclosingClippedRect(
      inverse_draw_transform, unoccluded_rect_in_target_surface);
  unoccluded_rect.Intersect(content_rect);

  return unoccluded_rect;
}

// Instantiate (and export) templates here for the linker.
template class OcclusionTracker<Layer>;
template class OcclusionTracker<LayerImpl>;

}  // namespace cc
