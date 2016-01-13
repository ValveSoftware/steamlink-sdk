// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_common.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/trees/layer_sorter.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

ScrollAndScaleSet::ScrollAndScaleSet() {}

ScrollAndScaleSet::~ScrollAndScaleSet() {}

static void SortLayers(LayerList::iterator forst,
                       LayerList::iterator end,
                       void* layer_sorter) {
  NOTREACHED();
}

static void SortLayers(LayerImplList::iterator first,
                       LayerImplList::iterator end,
                       LayerSorter* layer_sorter) {
  DCHECK(layer_sorter);
  TRACE_EVENT0("cc", "LayerTreeHostCommon::SortLayers");
  layer_sorter->Sort(first, end);
}

template <typename LayerType>
static gfx::Vector2dF GetEffectiveScrollDelta(LayerType* layer) {
  gfx::Vector2dF scroll_delta = layer->ScrollDelta();
  // The scroll parent's scroll delta is the amount we've scrolled on the
  // compositor thread since the commit for this layer tree's source frame.
  // we last reported to the main thread. I.e., it's the discrepancy between
  // a scroll parent's scroll delta and offset, so we must add it here.
  if (layer->scroll_parent())
    scroll_delta += layer->scroll_parent()->ScrollDelta();
  return scroll_delta;
}

template <typename LayerType>
static gfx::Vector2dF GetEffectiveTotalScrollOffset(LayerType* layer) {
  gfx::Vector2dF offset = layer->TotalScrollOffset();
  // The scroll parent's total scroll offset (scroll offset + scroll delta)
  // can't be used because its scroll offset has already been applied to the
  // scroll children's positions by the main thread layer positioning code.
  if (layer->scroll_parent())
    offset += layer->scroll_parent()->ScrollDelta();
  return offset;
}

inline gfx::Rect CalculateVisibleRectWithCachedLayerRect(
    const gfx::Rect& target_surface_rect,
    const gfx::Rect& layer_bound_rect,
    const gfx::Rect& layer_rect_in_target_space,
    const gfx::Transform& transform) {
  if (layer_rect_in_target_space.IsEmpty())
    return gfx::Rect();

  // Is this layer fully contained within the target surface?
  if (target_surface_rect.Contains(layer_rect_in_target_space))
    return layer_bound_rect;

  // If the layer doesn't fill up the entire surface, then find the part of
  // the surface rect where the layer could be visible. This avoids trying to
  // project surface rect points that are behind the projection point.
  gfx::Rect minimal_surface_rect = target_surface_rect;
  minimal_surface_rect.Intersect(layer_rect_in_target_space);

  if (minimal_surface_rect.IsEmpty())
    return gfx::Rect();

  // Project the corners of the target surface rect into the layer space.
  // This bounding rectangle may be larger than it needs to be (being
  // axis-aligned), but is a reasonable filter on the space to consider.
  // Non-invertible transforms will create an empty rect here.

  gfx::Transform surface_to_layer(gfx::Transform::kSkipInitialization);
  if (!transform.GetInverse(&surface_to_layer)) {
    // Because we cannot use the surface bounds to determine what portion of
    // the layer is visible, we must conservatively assume the full layer is
    // visible.
    return layer_bound_rect;
  }

  gfx::Rect layer_rect = MathUtil::ProjectEnclosingClippedRect(
      surface_to_layer, minimal_surface_rect);
  layer_rect.Intersect(layer_bound_rect);
  return layer_rect;
}

gfx::Rect LayerTreeHostCommon::CalculateVisibleRect(
    const gfx::Rect& target_surface_rect,
    const gfx::Rect& layer_bound_rect,
    const gfx::Transform& transform) {
  gfx::Rect layer_in_surface_space =
      MathUtil::MapEnclosingClippedRect(transform, layer_bound_rect);
  return CalculateVisibleRectWithCachedLayerRect(
      target_surface_rect, layer_bound_rect, layer_in_surface_space, transform);
}

template <typename LayerType>
static LayerType* NextTargetSurface(LayerType* layer) {
  return layer->parent() ? layer->parent()->render_target() : 0;
}

// Given two layers, this function finds their respective render targets and,
// computes a change of basis translation. It does this by accumulating the
// translation components of the draw transforms of each target between the
// ancestor and descendant. These transforms must be 2D translations, and this
// requirement is enforced at every step.
template <typename LayerType>
static gfx::Vector2dF ComputeChangeOfBasisTranslation(
    const LayerType& ancestor_layer,
    const LayerType& descendant_layer) {
  DCHECK(descendant_layer.HasAncestor(&ancestor_layer));
  const LayerType* descendant_target = descendant_layer.render_target();
  DCHECK(descendant_target);
  const LayerType* ancestor_target = ancestor_layer.render_target();
  DCHECK(ancestor_target);

  gfx::Vector2dF translation;
  for (const LayerType* target = descendant_target; target != ancestor_target;
       target = NextTargetSurface(target)) {
    const gfx::Transform& trans = target->render_surface()->draw_transform();
    // Ensure that this translation is truly 2d.
    DCHECK(trans.IsIdentityOrTranslation());
    DCHECK_EQ(0.f, trans.matrix().get(2, 3));
    translation += trans.To2dTranslation();
  }

  return translation;
}

enum TranslateRectDirection {
  TranslateRectDirectionToAncestor,
  TranslateRectDirectionToDescendant
};

template <typename LayerType>
static gfx::Rect TranslateRectToTargetSpace(const LayerType& ancestor_layer,
                                            const LayerType& descendant_layer,
                                            const gfx::Rect& rect,
                                            TranslateRectDirection direction) {
  gfx::Vector2dF translation = ComputeChangeOfBasisTranslation<LayerType>(
      ancestor_layer, descendant_layer);
  if (direction == TranslateRectDirectionToDescendant)
    translation.Scale(-1.f);
  return gfx::ToEnclosingRect(
      gfx::RectF(rect.origin() + translation, rect.size()));
}

// Attempts to update the clip rects for the given layer. If the layer has a
// clip_parent, it may not inherit its immediate ancestor's clip.
template <typename LayerType>
static void UpdateClipRectsForClipChild(
    const LayerType* layer,
    gfx::Rect* clip_rect_in_parent_target_space,
    bool* subtree_should_be_clipped) {
  // If the layer has no clip_parent, or the ancestor is the same as its actual
  // parent, then we don't need special clip rects. Bail now and leave the out
  // parameters untouched.
  const LayerType* clip_parent = layer->scroll_parent();

  if (!clip_parent)
    clip_parent = layer->clip_parent();

  if (!clip_parent || clip_parent == layer->parent())
    return;

  // The root layer is never a clip child.
  DCHECK(layer->parent());

  // Grab the cached values.
  *clip_rect_in_parent_target_space = clip_parent->clip_rect();
  *subtree_should_be_clipped = clip_parent->is_clipped();

  // We may have to project the clip rect into our parent's target space. Note,
  // it must be our parent's target space, not ours. For one, we haven't
  // computed our transforms, so we couldn't put it in our space yet even if we
  // wanted to. But more importantly, this matches the expectations of
  // CalculateDrawPropertiesInternal. If we, say, create a render surface, these
  // clip rects will want to be in its target space, not ours.
  if (clip_parent == layer->clip_parent()) {
    *clip_rect_in_parent_target_space = TranslateRectToTargetSpace<LayerType>(
        *clip_parent,
        *layer->parent(),
        *clip_rect_in_parent_target_space,
        TranslateRectDirectionToDescendant);
  } else {
    // If we're being clipped by our scroll parent, we must translate through
    // our common ancestor. This happens to be our parent, so it is sufficent to
    // translate from our clip parent's space to the space of its ancestor (our
    // parent).
    *clip_rect_in_parent_target_space =
        TranslateRectToTargetSpace<LayerType>(*layer->parent(),
                                              *clip_parent,
                                              *clip_rect_in_parent_target_space,
                                              TranslateRectDirectionToAncestor);
  }
}

// We collect an accumulated drawable content rect per render surface.
// Typically, a layer will contribute to only one surface, the surface
// associated with its render target. Clip children, however, may affect
// several surfaces since there may be several surfaces between the clip child
// and its parent.
//
// NB: we accumulate the layer's *clipped* drawable content rect.
template <typename LayerType>
struct AccumulatedSurfaceState {
  explicit AccumulatedSurfaceState(LayerType* render_target)
      : render_target(render_target) {}

  // The accumulated drawable content rect for the surface associated with the
  // given |render_target|.
  gfx::Rect drawable_content_rect;

  // The target owning the surface. (We hang onto the target rather than the
  // surface so that we can DCHECK that the surface's draw transform is simply
  // a translation when |render_target| reports that it has no unclipped
  // descendants).
  LayerType* render_target;
};

template <typename LayerType>
void UpdateAccumulatedSurfaceState(
    LayerType* layer,
    const gfx::Rect& drawable_content_rect,
    std::vector<AccumulatedSurfaceState<LayerType> >*
        accumulated_surface_state) {
  if (IsRootLayer(layer))
    return;

  // We will apply our drawable content rect to the accumulated rects for all
  // surfaces between us and |render_target| (inclusive). This is either our
  // clip parent's target if we are a clip child, or else simply our parent's
  // target. We use our parent's target because we're either the owner of a
  // render surface and we'll want to add our rect to our *surface's* target, or
  // we're not and our target is the same as our parent's. In both cases, the
  // parent's target gives us what we want.
  LayerType* render_target = layer->clip_parent()
                                 ? layer->clip_parent()->render_target()
                                 : layer->parent()->render_target();

  // If the layer owns a surface, then the content rect is in the wrong space.
  // Instead, we will use the surface's DrawableContentRect which is in target
  // space as required.
  gfx::Rect target_rect = drawable_content_rect;
  if (layer->render_surface()) {
    target_rect =
        gfx::ToEnclosedRect(layer->render_surface()->DrawableContentRect());
  }

  if (render_target->is_clipped()) {
    gfx::Rect clip_rect = render_target->clip_rect();
    // If the layer has a clip parent, the clip rect may be in the wrong space,
    // so we'll need to transform it before it is applied.
    if (layer->clip_parent()) {
      clip_rect = TranslateRectToTargetSpace<LayerType>(
          *layer->clip_parent(),
          *layer,
          clip_rect,
          TranslateRectDirectionToDescendant);
    }
    target_rect.Intersect(clip_rect);
  }

  // We must have at least one entry in the vector for the root.
  DCHECK_LT(0ul, accumulated_surface_state->size());

  typedef typename std::vector<AccumulatedSurfaceState<LayerType> >
      AccumulatedSurfaceStateVector;
  typedef typename AccumulatedSurfaceStateVector::reverse_iterator
      AccumulatedSurfaceStateIterator;
  AccumulatedSurfaceStateIterator current_state =
      accumulated_surface_state->rbegin();

  // Add this rect to the accumulated content rect for all surfaces until we
  // reach the target surface.
  bool found_render_target = false;
  for (; current_state != accumulated_surface_state->rend(); ++current_state) {
    current_state->drawable_content_rect.Union(target_rect);

    // If we've reached |render_target| our work is done and we can bail.
    if (current_state->render_target == render_target) {
      found_render_target = true;
      break;
    }

    // Transform rect from the current target's space to the next.
    LayerType* current_target = current_state->render_target;
    DCHECK(current_target->render_surface());
    const gfx::Transform& current_draw_transform =
         current_target->render_surface()->draw_transform();

    // If we have unclipped descendants, the draw transform is a translation.
    DCHECK(current_target->num_unclipped_descendants() == 0 ||
           current_draw_transform.IsIdentityOrTranslation());

    target_rect = gfx::ToEnclosingRect(
        MathUtil::MapClippedRect(current_draw_transform, target_rect));
  }

  // It is an error to not reach |render_target|. If this happens, it means that
  // either the clip parent is not an ancestor of the clip child or the surface
  // state vector is empty, both of which should be impossible.
  DCHECK(found_render_target);
}

template <typename LayerType> static inline bool IsRootLayer(LayerType* layer) {
  return !layer->parent();
}

template <typename LayerType>
static inline bool LayerIsInExisting3DRenderingContext(LayerType* layer) {
  return layer->Is3dSorted() && layer->parent() &&
         layer->parent()->Is3dSorted();
}

template <typename LayerType>
static bool IsRootLayerOfNewRenderingContext(LayerType* layer) {
  if (layer->parent())
    return !layer->parent()->Is3dSorted() && layer->Is3dSorted();

  return layer->Is3dSorted();
}

template <typename LayerType>
static bool IsLayerBackFaceVisible(LayerType* layer) {
  // The current W3C spec on CSS transforms says that backface visibility should
  // be determined differently depending on whether the layer is in a "3d
  // rendering context" or not. For Chromium code, we can determine whether we
  // are in a 3d rendering context by checking if the parent preserves 3d.

  if (LayerIsInExisting3DRenderingContext(layer))
    return layer->draw_transform().IsBackFaceVisible();

  // In this case, either the layer establishes a new 3d rendering context, or
  // is not in a 3d rendering context at all.
  return layer->transform().IsBackFaceVisible();
}

template <typename LayerType>
static bool IsSurfaceBackFaceVisible(LayerType* layer,
                                     const gfx::Transform& draw_transform) {
  if (LayerIsInExisting3DRenderingContext(layer))
    return draw_transform.IsBackFaceVisible();

  if (IsRootLayerOfNewRenderingContext(layer))
    return layer->transform().IsBackFaceVisible();

  // If the render_surface is not part of a new or existing rendering context,
  // then the layers that contribute to this surface will decide back-face
  // visibility for themselves.
  return false;
}

template <typename LayerType>
static inline bool LayerClipsSubtree(LayerType* layer) {
  return layer->masks_to_bounds() || layer->mask_layer();
}

template <typename LayerType>
static gfx::Rect CalculateVisibleContentRect(
    LayerType* layer,
    const gfx::Rect& clip_rect_of_target_surface_in_target_space,
    const gfx::Rect& layer_rect_in_target_space) {
  DCHECK(layer->render_target());

  // Nothing is visible if the layer bounds are empty.
  if (!layer->DrawsContent() || layer->content_bounds().IsEmpty() ||
      layer->drawable_content_rect().IsEmpty())
    return gfx::Rect();

  // Compute visible bounds in target surface space.
  gfx::Rect visible_rect_in_target_surface_space =
      layer->drawable_content_rect();

  if (!layer->render_target()->render_surface()->clip_rect().IsEmpty()) {
    // The |layer| L has a target T which owns a surface Ts. The surface Ts
    // has a target TsT.
    //
    // In this case the target surface Ts does clip the layer L that contributes
    // to it. So, we have to convert the clip rect of Ts from the target space
    // of Ts (that is the space of TsT), to the current render target's space
    // (that is the space of T). This conversion is done outside this function
    // so that it can be cached instead of computing it redundantly for every
    // layer.
    visible_rect_in_target_surface_space.Intersect(
        clip_rect_of_target_surface_in_target_space);
  }

  if (visible_rect_in_target_surface_space.IsEmpty())
    return gfx::Rect();

  return CalculateVisibleRectWithCachedLayerRect(
      visible_rect_in_target_surface_space,
      gfx::Rect(layer->content_bounds()),
      layer_rect_in_target_space,
      layer->draw_transform());
}

static inline bool TransformToParentIsKnown(LayerImpl* layer) { return true; }

static inline bool TransformToParentIsKnown(Layer* layer) {
  return !layer->TransformIsAnimating();
}

static inline bool TransformToScreenIsKnown(LayerImpl* layer) { return true; }

static inline bool TransformToScreenIsKnown(Layer* layer) {
  return !layer->screen_space_transform_is_animating();
}

template <typename LayerType>
static bool LayerShouldBeSkipped(LayerType* layer, bool layer_is_drawn) {
  // Layers can be skipped if any of these conditions are met.
  //   - is not drawn due to it or one of its ancestors being hidden (or having
  //     no copy requests).
  //   - does not draw content.
  //   - is transparent.
  //   - has empty bounds
  //   - the layer is not double-sided, but its back face is visible.
  //
  // Some additional conditions need to be computed at a later point after the
  // recursion is finished.
  //   - the intersection of render_surface content and layer clip_rect is empty
  //   - the visible_content_rect is empty
  //
  // Note, if the layer should not have been drawn due to being fully
  // transparent, we would have skipped the entire subtree and never made it
  // into this function, so it is safe to omit this check here.

  if (!layer_is_drawn)
    return true;

  if (!layer->DrawsContent() || layer->bounds().IsEmpty())
    return true;

  LayerType* backface_test_layer = layer;
  if (layer->use_parent_backface_visibility()) {
    DCHECK(layer->parent());
    DCHECK(!layer->parent()->use_parent_backface_visibility());
    backface_test_layer = layer->parent();
  }

  // The layer should not be drawn if (1) it is not double-sided and (2) the
  // back of the layer is known to be facing the screen.
  if (!backface_test_layer->double_sided() &&
      TransformToScreenIsKnown(backface_test_layer) &&
      IsLayerBackFaceVisible(backface_test_layer))
    return true;

  return false;
}

template <typename LayerType>
static bool HasInvertibleOrAnimatedTransform(LayerType* layer) {
  return layer->transform_is_invertible() || layer->TransformIsAnimating();
}

static inline bool SubtreeShouldBeSkipped(LayerImpl* layer,
                                          bool layer_is_drawn) {
  // If the layer transform is not invertible, it should not be drawn.
  // TODO(ajuma): Correctly process subtrees with singular transform for the
  // case where we may animate to a non-singular transform and wish to
  // pre-raster.
  if (!HasInvertibleOrAnimatedTransform(layer))
    return true;

  // When we need to do a readback/copy of a layer's output, we can not skip
  // it or any of its ancestors.
  if (layer->draw_properties().layer_or_descendant_has_copy_request)
    return false;

  // We cannot skip the the subtree if a descendant has a wheel or touch handler
  // or the hit testing code will break (it requires fresh transforms, etc).
  if (layer->draw_properties().layer_or_descendant_has_input_handler)
    return false;

  // If the layer is not drawn, then skip it and its subtree.
  if (!layer_is_drawn)
    return true;

  // If layer is on the pending tree and opacity is being animated then
  // this subtree can't be skipped as we need to create, prioritize and
  // include tiles for this layer when deciding if tree can be activated.
  if (layer->layer_tree_impl()->IsPendingTree() && layer->OpacityIsAnimating())
    return false;

  // The opacity of a layer always applies to its children (either implicitly
  // via a render surface or explicitly if the parent preserves 3D), so the
  // entire subtree can be skipped if this layer is fully transparent.
  return !layer->opacity();
}

static inline bool SubtreeShouldBeSkipped(Layer* layer, bool layer_is_drawn) {
  // If the layer transform is not invertible, it should not be drawn.
  if (!layer->transform_is_invertible() && !layer->TransformIsAnimating())
    return true;

  // When we need to do a readback/copy of a layer's output, we can not skip
  // it or any of its ancestors.
  if (layer->draw_properties().layer_or_descendant_has_copy_request)
    return false;

  // We cannot skip the the subtree if a descendant has a wheel or touch handler
  // or the hit testing code will break (it requires fresh transforms, etc).
  if (layer->draw_properties().layer_or_descendant_has_input_handler)
    return false;

  // If the layer is not drawn, then skip it and its subtree.
  if (!layer_is_drawn)
    return true;

  // If the opacity is being animated then the opacity on the main thread is
  // unreliable (since the impl thread may be using a different opacity), so it
  // should not be trusted.
  // In particular, it should not cause the subtree to be skipped.
  // Similarly, for layers that might animate opacity using an impl-only
  // animation, their subtree should also not be skipped.
  return !layer->opacity() && !layer->OpacityIsAnimating() &&
         !layer->OpacityCanAnimateOnImplThread();
}

static inline void SavePaintPropertiesLayer(LayerImpl* layer) {}

static inline void SavePaintPropertiesLayer(Layer* layer) {
  layer->SavePaintProperties();

  if (layer->mask_layer())
    layer->mask_layer()->SavePaintProperties();
  if (layer->replica_layer() && layer->replica_layer()->mask_layer())
    layer->replica_layer()->mask_layer()->SavePaintProperties();
}

template <typename LayerType>
static bool SubtreeShouldRenderToSeparateSurface(
    LayerType* layer,
    bool axis_aligned_with_respect_to_parent) {
  //
  // A layer and its descendants should render onto a new RenderSurfaceImpl if
  // any of these rules hold:
  //

  // The root layer owns a render surface, but it never acts as a contributing
  // surface to another render target. Compositor features that are applied via
  // a contributing surface can not be applied to the root layer. In order to
  // use these effects, another child of the root would need to be introduced
  // in order to act as a contributing surface to the root layer's surface.
  bool is_root = IsRootLayer(layer);

  // If the layer uses a mask.
  if (layer->mask_layer()) {
    DCHECK(!is_root);
    return true;
  }

  // If the layer has a reflection.
  if (layer->replica_layer()) {
    DCHECK(!is_root);
    return true;
  }

  // If the layer uses a CSS filter.
  if (!layer->filters().IsEmpty() || !layer->background_filters().IsEmpty()) {
    DCHECK(!is_root);
    return true;
  }

  int num_descendants_that_draw_content =
      layer->draw_properties().num_descendants_that_draw_content;

  // If the layer flattens its subtree, but it is treated as a 3D object by its
  // parent (i.e. parent participates in a 3D rendering context).
  if (LayerIsInExisting3DRenderingContext(layer) &&
      layer->should_flatten_transform() &&
      num_descendants_that_draw_content > 0) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostCommon::SubtreeShouldRenderToSeparateSurface flattening",
        TRACE_EVENT_SCOPE_THREAD);
    DCHECK(!is_root);
    return true;
  }

  // If the layer has blending.
  // TODO(rosca): this is temporary, until blending is implemented for other
  // types of quads than RenderPassDrawQuad. Layers having descendants that draw
  // content will still create a separate rendering surface.
  if (!layer->uses_default_blend_mode()) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostCommon::SubtreeShouldRenderToSeparateSurface blending",
        TRACE_EVENT_SCOPE_THREAD);
    DCHECK(!is_root);
    return true;
  }

  // If the layer clips its descendants but it is not axis-aligned with respect
  // to its parent.
  bool layer_clips_external_content =
      LayerClipsSubtree(layer) || layer->HasDelegatedContent();
  if (layer_clips_external_content && !axis_aligned_with_respect_to_parent &&
      num_descendants_that_draw_content > 0) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostCommon::SubtreeShouldRenderToSeparateSurface clipping",
        TRACE_EVENT_SCOPE_THREAD);
    DCHECK(!is_root);
    return true;
  }

  // If the layer has some translucency and does not have a preserves-3d
  // transform style.  This condition only needs a render surface if two or more
  // layers in the subtree overlap. But checking layer overlaps is unnecessarily
  // costly so instead we conservatively create a surface whenever at least two
  // layers draw content for this subtree.
  bool at_least_two_layers_in_subtree_draw_content =
      num_descendants_that_draw_content > 0 &&
      (layer->DrawsContent() || num_descendants_that_draw_content > 1);

  if (layer->opacity() != 1.f && layer->should_flatten_transform() &&
      at_least_two_layers_in_subtree_draw_content) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostCommon::SubtreeShouldRenderToSeparateSurface opacity",
        TRACE_EVENT_SCOPE_THREAD);
    DCHECK(!is_root);
    return true;
  }

  // The root layer should always have a render_surface.
  if (is_root)
    return true;

  //
  // These are allowed on the root surface, as they don't require the surface to
  // be used as a contributing surface in order to apply correctly.
  //

  // If the layer has isolation.
  // TODO(rosca): to be optimized - create separate rendering surface only when
  // the blending descendants might have access to the content behind this layer
  // (layer has transparent background or descendants overflow).
  // https://code.google.com/p/chromium/issues/detail?id=301738
  if (layer->is_root_for_isolated_group()) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "LayerTreeHostCommon::SubtreeShouldRenderToSeparateSurface isolation",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If we force it.
  if (layer->force_render_surface())
    return true;

  // If we'll make a copy of the layer's contents.
  if (layer->HasCopyRequest())
    return true;

  return false;
}

// This function returns a translation matrix that can be applied on a vector
// that's in the layer's target surface coordinate, while the position offset is
// specified in some ancestor layer's coordinate.
gfx::Transform ComputeSizeDeltaCompensation(
    LayerImpl* layer,
    LayerImpl* container,
    const gfx::Vector2dF& position_offset) {
  gfx::Transform result_transform;

  // To apply a translate in the container's layer space,
  // the following steps need to be done:
  //     Step 1a. transform from target surface space to the container's target
  //              surface space
  //     Step 1b. transform from container's target surface space to the
  //              container's layer space
  //     Step 2. apply the compensation
  //     Step 3. transform back to target surface space

  gfx::Transform target_surface_space_to_container_layer_space;
  // Calculate step 1a
  LayerImpl* container_target_surface = container->render_target();
  for (LayerImpl* current_target_surface = NextTargetSurface(layer);
      current_target_surface &&
          current_target_surface != container_target_surface;
      current_target_surface = NextTargetSurface(current_target_surface)) {
    // Note: Concat is used here to convert the result coordinate space from
    //       current render surface to the next render surface.
    target_surface_space_to_container_layer_space.ConcatTransform(
        current_target_surface->render_surface()->draw_transform());
  }
  // Calculate step 1b
  gfx::Transform container_layer_space_to_container_target_surface_space =
      container->draw_transform();
  container_layer_space_to_container_target_surface_space.Scale(
      container->contents_scale_x(), container->contents_scale_y());

  gfx::Transform container_target_surface_space_to_container_layer_space;
  if (container_layer_space_to_container_target_surface_space.GetInverse(
      &container_target_surface_space_to_container_layer_space)) {
    // Note: Again, Concat is used to conver the result coordinate space from
    //       the container render surface to the container layer.
    target_surface_space_to_container_layer_space.ConcatTransform(
        container_target_surface_space_to_container_layer_space);
  }

  // Apply step 3
  gfx::Transform container_layer_space_to_target_surface_space;
  if (target_surface_space_to_container_layer_space.GetInverse(
          &container_layer_space_to_target_surface_space)) {
    result_transform.PreconcatTransform(
        container_layer_space_to_target_surface_space);
  } else {
    // TODO(shawnsingh): A non-invertible matrix could still make meaningful
    // projection.  For example ScaleZ(0) is non-invertible but the layer is
    // still visible.
    return gfx::Transform();
  }

  // Apply step 2
  result_transform.Translate(position_offset.x(), position_offset.y());

  // Apply step 1
  result_transform.PreconcatTransform(
      target_surface_space_to_container_layer_space);

  return result_transform;
}

void ApplyPositionAdjustment(
    Layer* layer,
    Layer* container,
    const gfx::Transform& scroll_compensation,
    gfx::Transform* combined_transform) {}
void ApplyPositionAdjustment(
    LayerImpl* layer,
    LayerImpl* container,
    const gfx::Transform& scroll_compensation,
    gfx::Transform* combined_transform) {
  if (!layer->position_constraint().is_fixed_position())
    return;

  // Special case: this layer is a composited fixed-position layer; we need to
  // explicitly compensate for all ancestors' nonzero scroll_deltas to keep
  // this layer fixed correctly.
  // Note carefully: this is Concat, not Preconcat
  // (current_scroll_compensation * combined_transform).
  combined_transform->ConcatTransform(scroll_compensation);

  // For right-edge or bottom-edge anchored fixed position layers,
  // the layer should relocate itself if the container changes its size.
  bool fixed_to_right_edge =
      layer->position_constraint().is_fixed_to_right_edge();
  bool fixed_to_bottom_edge =
      layer->position_constraint().is_fixed_to_bottom_edge();
  gfx::Vector2dF position_offset = container->FixedContainerSizeDelta();
  position_offset.set_x(fixed_to_right_edge ? position_offset.x() : 0);
  position_offset.set_y(fixed_to_bottom_edge ? position_offset.y() : 0);
  if (position_offset.IsZero())
    return;

  // Note: Again, this is Concat. The compensation matrix will be applied on
  //       the vector in target surface space.
  combined_transform->ConcatTransform(
      ComputeSizeDeltaCompensation(layer, container, position_offset));
}

gfx::Transform ComputeScrollCompensationForThisLayer(
    LayerImpl* scrolling_layer,
    const gfx::Transform& parent_matrix,
    const gfx::Vector2dF& scroll_delta) {
  // For every layer that has non-zero scroll_delta, we have to compute a
  // transform that can undo the scroll_delta translation. In particular, we
  // want this matrix to premultiply a fixed-position layer's parent_matrix, so
  // we design this transform in three steps as follows. The steps described
  // here apply from right-to-left, so Step 1 would be the right-most matrix:
  //
  //     Step 1. transform from target surface space to the exact space where
  //           scroll_delta is actually applied.
  //           -- this is inverse of parent_matrix
  //     Step 2. undo the scroll_delta
  //           -- this is just a translation by scroll_delta.
  //     Step 3. transform back to target surface space.
  //           -- this transform is the parent_matrix
  //
  // These steps create a matrix that both start and end in target surface
  // space. So this matrix can pre-multiply any fixed-position layer's
  // draw_transform to undo the scroll_deltas -- as long as that fixed position
  // layer is fixed onto the same render_target as this scrolling_layer.
  //

  gfx::Transform scroll_compensation_for_this_layer = parent_matrix;  // Step 3
  scroll_compensation_for_this_layer.Translate(
      scroll_delta.x(),
      scroll_delta.y());  // Step 2

  gfx::Transform inverse_parent_matrix(gfx::Transform::kSkipInitialization);
  if (!parent_matrix.GetInverse(&inverse_parent_matrix)) {
    // TODO(shawnsingh): Either we need to handle uninvertible transforms
    // here, or DCHECK that the transform is invertible.
  }
  scroll_compensation_for_this_layer.PreconcatTransform(
      inverse_parent_matrix);  // Step 1
  return scroll_compensation_for_this_layer;
}

gfx::Transform ComputeScrollCompensationMatrixForChildren(
    Layer* current_layer,
    const gfx::Transform& current_parent_matrix,
    const gfx::Transform& current_scroll_compensation,
    const gfx::Vector2dF& scroll_delta) {
  // The main thread (i.e. Layer) does not need to worry about scroll
  // compensation.  So we can just return an identity matrix here.
  return gfx::Transform();
}

gfx::Transform ComputeScrollCompensationMatrixForChildren(
    LayerImpl* layer,
    const gfx::Transform& parent_matrix,
    const gfx::Transform& current_scroll_compensation_matrix,
    const gfx::Vector2dF& scroll_delta) {
  // "Total scroll compensation" is the transform needed to cancel out all
  // scroll_delta translations that occurred since the nearest container layer,
  // even if there are render_surfaces in-between.
  //
  // There are some edge cases to be aware of, that are not explicit in the
  // code:
  //  - A layer that is both a fixed-position and container should not be its
  //  own container, instead, that means it is fixed to an ancestor, and is a
  //  container for any fixed-position descendants.
  //  - A layer that is a fixed-position container and has a render_surface
  //  should behave the same as a container without a render_surface, the
  //  render_surface is irrelevant in that case.
  //  - A layer that does not have an explicit container is simply fixed to the
  //  viewport.  (i.e. the root render_surface.)
  //  - If the fixed-position layer has its own render_surface, then the
  //  render_surface is the one who gets fixed.
  //
  // This function needs to be called AFTER layers create their own
  // render_surfaces.
  //

  // Scroll compensation restarts from identity under two possible conditions:
  //  - the current layer is a container for fixed-position descendants
  //  - the current layer is fixed-position itself, so any fixed-position
  //    descendants are positioned with respect to this layer. Thus, any
  //    fixed position descendants only need to compensate for scrollDeltas
  //    that occur below this layer.
  bool current_layer_resets_scroll_compensation_for_descendants =
      layer->IsContainerForFixedPositionLayers() ||
      layer->position_constraint().is_fixed_position();

  // Avoid the overheads (including stack allocation and matrix
  // initialization/copy) if we know that the scroll compensation doesn't need
  // to be reset or adjusted.
  if (!current_layer_resets_scroll_compensation_for_descendants &&
      scroll_delta.IsZero() && !layer->render_surface())
    return current_scroll_compensation_matrix;

  // Start as identity matrix.
  gfx::Transform next_scroll_compensation_matrix;

  // If this layer does not reset scroll compensation, then it inherits the
  // existing scroll compensations.
  if (!current_layer_resets_scroll_compensation_for_descendants)
    next_scroll_compensation_matrix = current_scroll_compensation_matrix;

  // If the current layer has a non-zero scroll_delta, then we should compute
  // its local scroll compensation and accumulate it to the
  // next_scroll_compensation_matrix.
  if (!scroll_delta.IsZero()) {
    gfx::Transform scroll_compensation_for_this_layer =
        ComputeScrollCompensationForThisLayer(
            layer, parent_matrix, scroll_delta);
    next_scroll_compensation_matrix.PreconcatTransform(
        scroll_compensation_for_this_layer);
  }

  // If the layer created its own render_surface, we have to adjust
  // next_scroll_compensation_matrix.  The adjustment allows us to continue
  // using the scroll compensation on the next surface.
  //  Step 1 (right-most in the math): transform from the new surface to the
  //  original ancestor surface
  //  Step 2: apply the scroll compensation
  //  Step 3: transform back to the new surface.
  if (layer->render_surface() &&
      !next_scroll_compensation_matrix.IsIdentity()) {
    gfx::Transform inverse_surface_draw_transform(
        gfx::Transform::kSkipInitialization);
    if (!layer->render_surface()->draw_transform().GetInverse(
            &inverse_surface_draw_transform)) {
      // TODO(shawnsingh): Either we need to handle uninvertible transforms
      // here, or DCHECK that the transform is invertible.
    }
    next_scroll_compensation_matrix =
        inverse_surface_draw_transform * next_scroll_compensation_matrix *
        layer->render_surface()->draw_transform();
  }

  return next_scroll_compensation_matrix;
}

template <typename LayerType>
static inline void UpdateLayerScaleDrawProperties(
    LayerType* layer,
    float ideal_contents_scale,
    float maximum_animation_contents_scale,
    float page_scale_factor,
    float device_scale_factor) {
  layer->draw_properties().ideal_contents_scale = ideal_contents_scale;
  layer->draw_properties().maximum_animation_contents_scale =
      maximum_animation_contents_scale;
  layer->draw_properties().page_scale_factor = page_scale_factor;
  layer->draw_properties().device_scale_factor = device_scale_factor;
}

static inline void CalculateContentsScale(
    LayerImpl* layer,
    float contents_scale,
    float device_scale_factor,
    float page_scale_factor,
    float maximum_animation_contents_scale,
    bool animating_transform_to_screen) {
  // LayerImpl has all of its content scales and bounds pushed from the Main
  // thread during commit and just uses those values as-is.
}

static inline void CalculateContentsScale(
    Layer* layer,
    float contents_scale,
    float device_scale_factor,
    float page_scale_factor,
    float maximum_animation_contents_scale,
    bool animating_transform_to_screen) {
  layer->CalculateContentsScale(contents_scale,
                                device_scale_factor,
                                page_scale_factor,
                                maximum_animation_contents_scale,
                                animating_transform_to_screen,
                                &layer->draw_properties().contents_scale_x,
                                &layer->draw_properties().contents_scale_y,
                                &layer->draw_properties().content_bounds);

  Layer* mask_layer = layer->mask_layer();
  if (mask_layer) {
    mask_layer->CalculateContentsScale(
        contents_scale,
        device_scale_factor,
        page_scale_factor,
        maximum_animation_contents_scale,
        animating_transform_to_screen,
        &mask_layer->draw_properties().contents_scale_x,
        &mask_layer->draw_properties().contents_scale_y,
        &mask_layer->draw_properties().content_bounds);
  }

  Layer* replica_mask_layer =
      layer->replica_layer() ? layer->replica_layer()->mask_layer() : NULL;
  if (replica_mask_layer) {
    replica_mask_layer->CalculateContentsScale(
        contents_scale,
        device_scale_factor,
        page_scale_factor,
        maximum_animation_contents_scale,
        animating_transform_to_screen,
        &replica_mask_layer->draw_properties().contents_scale_x,
        &replica_mask_layer->draw_properties().contents_scale_y,
        &replica_mask_layer->draw_properties().content_bounds);
  }
}

static inline void UpdateLayerContentsScale(
    LayerImpl* layer,
    bool can_adjust_raster_scale,
    float ideal_contents_scale,
    float device_scale_factor,
    float page_scale_factor,
    float maximum_animation_contents_scale,
    bool animating_transform_to_screen) {
  CalculateContentsScale(layer,
                         ideal_contents_scale,
                         device_scale_factor,
                         page_scale_factor,
                         maximum_animation_contents_scale,
                         animating_transform_to_screen);
}

static inline void UpdateLayerContentsScale(
    Layer* layer,
    bool can_adjust_raster_scale,
    float ideal_contents_scale,
    float device_scale_factor,
    float page_scale_factor,
    float maximum_animation_contents_scale,
    bool animating_transform_to_screen) {
  if (can_adjust_raster_scale) {
    float ideal_raster_scale =
        ideal_contents_scale / (device_scale_factor * page_scale_factor);

    bool need_to_set_raster_scale = layer->raster_scale_is_unknown();

    // If we've previously saved a raster_scale but the ideal changes, things
    // are unpredictable and we should just use 1.
    if (!need_to_set_raster_scale && layer->raster_scale() != 1.f &&
        ideal_raster_scale != layer->raster_scale()) {
      ideal_raster_scale = 1.f;
      need_to_set_raster_scale = true;
    }

    if (need_to_set_raster_scale) {
      bool use_and_save_ideal_scale =
          ideal_raster_scale >= 1.f && !animating_transform_to_screen;
      if (use_and_save_ideal_scale)
        layer->set_raster_scale(ideal_raster_scale);
    }
  }

  float raster_scale = 1.f;
  if (!layer->raster_scale_is_unknown())
    raster_scale = layer->raster_scale();

  gfx::Size old_content_bounds = layer->content_bounds();
  float old_contents_scale_x = layer->contents_scale_x();
  float old_contents_scale_y = layer->contents_scale_y();

  float contents_scale = raster_scale * device_scale_factor * page_scale_factor;
  CalculateContentsScale(layer,
                         contents_scale,
                         device_scale_factor,
                         page_scale_factor,
                         maximum_animation_contents_scale,
                         animating_transform_to_screen);

  if (layer->content_bounds() != old_content_bounds ||
      layer->contents_scale_x() != old_contents_scale_x ||
      layer->contents_scale_y() != old_contents_scale_y)
    layer->SetNeedsPushProperties();
}

static inline void CalculateAnimationContentsScale(
    Layer* layer,
    bool ancestor_is_animating_scale,
    float ancestor_maximum_animation_contents_scale,
    const gfx::Transform& parent_transform,
    const gfx::Transform& combined_transform,
    bool* combined_is_animating_scale,
    float* combined_maximum_animation_contents_scale) {
  *combined_is_animating_scale = false;
  *combined_maximum_animation_contents_scale = 0.f;
}

static inline void CalculateAnimationContentsScale(
    LayerImpl* layer,
    bool ancestor_is_animating_scale,
    float ancestor_maximum_animation_contents_scale,
    const gfx::Transform& ancestor_transform,
    const gfx::Transform& combined_transform,
    bool* combined_is_animating_scale,
    float* combined_maximum_animation_contents_scale) {
  if (ancestor_is_animating_scale &&
      ancestor_maximum_animation_contents_scale == 0.f) {
    // We've already failed to compute a maximum animated scale at an
    // ancestor, so we'll continue to fail.
    *combined_maximum_animation_contents_scale = 0.f;
    *combined_is_animating_scale = true;
    return;
  }

  if (!combined_transform.IsScaleOrTranslation()) {
    // Computing maximum animated scale in the presence of
    // non-scale/translation transforms isn't supported.
    *combined_maximum_animation_contents_scale = 0.f;
    *combined_is_animating_scale = true;
    return;
  }

  // We currently only support computing maximum scale for combinations of
  // scales and translations. We treat all non-translations as potentially
  // affecting scale. Animations that include non-translation/scale components
  // will cause the computation of MaximumScale below to fail.
  bool layer_is_animating_scale =
      !layer->layer_animation_controller()->HasOnlyTranslationTransforms();

  if (!layer_is_animating_scale && !ancestor_is_animating_scale) {
    *combined_maximum_animation_contents_scale = 0.f;
    *combined_is_animating_scale = false;
    return;
  }

  // We don't attempt to accumulate animation scale from multiple nodes,
  // because of the risk of significant overestimation. For example, one node
  // may be increasing scale from 1 to 10 at the same time as a descendant is
  // decreasing scale from 10 to 1. Naively combining these scales would produce
  // a scale of 100.
  if (layer_is_animating_scale && ancestor_is_animating_scale) {
    *combined_maximum_animation_contents_scale = 0.f;
    *combined_is_animating_scale = true;
    return;
  }

  // At this point, we know either the layer or an ancestor, but not both,
  // is animating scale.
  *combined_is_animating_scale = true;
  if (!layer_is_animating_scale) {
    gfx::Vector2dF layer_transform_scales =
        MathUtil::ComputeTransform2dScaleComponents(layer->transform(), 0.f);
    *combined_maximum_animation_contents_scale =
        ancestor_maximum_animation_contents_scale *
        std::max(layer_transform_scales.x(), layer_transform_scales.y());
    return;
  }

  float layer_maximum_animated_scale = 0.f;
  if (!layer->layer_animation_controller()->MaximumScale(
          &layer_maximum_animated_scale)) {
    *combined_maximum_animation_contents_scale = 0.f;
    return;
  }
  gfx::Vector2dF ancestor_transform_scales =
      MathUtil::ComputeTransform2dScaleComponents(ancestor_transform, 0.f);
  *combined_maximum_animation_contents_scale =
      layer_maximum_animated_scale *
      std::max(ancestor_transform_scales.x(), ancestor_transform_scales.y());
}

template <typename LayerType>
static inline typename LayerType::RenderSurfaceType* CreateOrReuseRenderSurface(
    LayerType* layer) {
  if (!layer->render_surface()) {
    layer->CreateRenderSurface();
    return layer->render_surface();
  }

  layer->render_surface()->ClearLayerLists();
  return layer->render_surface();
}

template <typename LayerTypePtr>
static inline void MarkLayerWithRenderSurfaceLayerListId(
    LayerTypePtr layer,
    int current_render_surface_layer_list_id) {
  layer->draw_properties().last_drawn_render_surface_layer_list_id =
      current_render_surface_layer_list_id;
}

template <typename LayerTypePtr>
static inline void MarkMasksWithRenderSurfaceLayerListId(
    LayerTypePtr layer,
    int current_render_surface_layer_list_id) {
  if (layer->mask_layer()) {
    MarkLayerWithRenderSurfaceLayerListId(layer->mask_layer(),
                                          current_render_surface_layer_list_id);
  }
  if (layer->replica_layer() && layer->replica_layer()->mask_layer()) {
    MarkLayerWithRenderSurfaceLayerListId(layer->replica_layer()->mask_layer(),
                                          current_render_surface_layer_list_id);
  }
}

template <typename LayerListType>
static inline void MarkLayerListWithRenderSurfaceLayerListId(
    LayerListType* layer_list,
    int current_render_surface_layer_list_id) {
  for (typename LayerListType::iterator it = layer_list->begin();
       it != layer_list->end();
       ++it) {
    MarkLayerWithRenderSurfaceLayerListId(*it,
                                          current_render_surface_layer_list_id);
    MarkMasksWithRenderSurfaceLayerListId(*it,
                                          current_render_surface_layer_list_id);
  }
}

template <typename LayerType>
static inline void RemoveSurfaceForEarlyExit(
    LayerType* layer_to_remove,
    typename LayerType::RenderSurfaceListType* render_surface_layer_list) {
  DCHECK(layer_to_remove->render_surface());
  // Technically, we know that the layer we want to remove should be
  // at the back of the render_surface_layer_list. However, we have had
  // bugs before that added unnecessary layers here
  // (https://bugs.webkit.org/show_bug.cgi?id=74147), but that causes
  // things to crash. So here we proactively remove any additional
  // layers from the end of the list.
  while (render_surface_layer_list->back() != layer_to_remove) {
    MarkLayerListWithRenderSurfaceLayerListId(
        &render_surface_layer_list->back()->render_surface()->layer_list(), 0);
    MarkLayerWithRenderSurfaceLayerListId(render_surface_layer_list->back(), 0);

    render_surface_layer_list->back()->ClearRenderSurfaceLayerList();
    render_surface_layer_list->pop_back();
  }
  DCHECK_EQ(render_surface_layer_list->back(), layer_to_remove);
  MarkLayerListWithRenderSurfaceLayerListId(
      &layer_to_remove->render_surface()->layer_list(), 0);
  MarkLayerWithRenderSurfaceLayerListId(layer_to_remove, 0);
  render_surface_layer_list->pop_back();
  layer_to_remove->ClearRenderSurfaceLayerList();
}

struct PreCalculateMetaInformationRecursiveData {
  bool layer_or_descendant_has_copy_request;
  bool layer_or_descendant_has_input_handler;
  int num_unclipped_descendants;

  PreCalculateMetaInformationRecursiveData()
      : layer_or_descendant_has_copy_request(false),
        layer_or_descendant_has_input_handler(false),
        num_unclipped_descendants(0) {}

  void Merge(const PreCalculateMetaInformationRecursiveData& data) {
    layer_or_descendant_has_copy_request |=
        data.layer_or_descendant_has_copy_request;
    layer_or_descendant_has_input_handler |=
        data.layer_or_descendant_has_input_handler;
    num_unclipped_descendants +=
        data.num_unclipped_descendants;
  }
};

// Recursively walks the layer tree to compute any information that is needed
// before doing the main recursion.
template <typename LayerType>
static void PreCalculateMetaInformation(
    LayerType* layer,
    PreCalculateMetaInformationRecursiveData* recursive_data) {
  bool has_delegated_content = layer->HasDelegatedContent();
  int num_descendants_that_draw_content = 0;

  layer->draw_properties().sorted_for_recursion = false;
  layer->draw_properties().has_child_with_a_scroll_parent = false;

  if (!HasInvertibleOrAnimatedTransform(layer)) {
    // Layers with singular transforms should not be drawn, the whole subtree
    // can be skipped.
    return;
  }

  if (has_delegated_content) {
    // Layers with delegated content need to be treated as if they have as
    // many children as the number of layers they own delegated quads for.
    // Since we don't know this number right now, we choose one that acts like
    // infinity for our purposes.
    num_descendants_that_draw_content = 1000;
  }

  if (layer->clip_parent())
    recursive_data->num_unclipped_descendants++;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    LayerType* child_layer =
        LayerTreeHostCommon::get_layer_as_raw_ptr(layer->children(), i);

    PreCalculateMetaInformationRecursiveData data_for_child;
    PreCalculateMetaInformation(child_layer, &data_for_child);

    num_descendants_that_draw_content += child_layer->DrawsContent() ? 1 : 0;
    num_descendants_that_draw_content +=
        child_layer->draw_properties().num_descendants_that_draw_content;

    if (child_layer->scroll_parent())
      layer->draw_properties().has_child_with_a_scroll_parent = true;
    recursive_data->Merge(data_for_child);
  }

  if (layer->clip_children()) {
    int num_clip_children = layer->clip_children()->size();
    DCHECK_GE(recursive_data->num_unclipped_descendants, num_clip_children);
    recursive_data->num_unclipped_descendants -= num_clip_children;
  }

  if (layer->HasCopyRequest())
    recursive_data->layer_or_descendant_has_copy_request = true;

  if (!layer->touch_event_handler_region().IsEmpty() ||
      layer->have_wheel_event_handlers())
    recursive_data->layer_or_descendant_has_input_handler = true;

  layer->draw_properties().num_descendants_that_draw_content =
      num_descendants_that_draw_content;
  layer->draw_properties().num_unclipped_descendants =
      recursive_data->num_unclipped_descendants;
  layer->draw_properties().layer_or_descendant_has_copy_request =
      recursive_data->layer_or_descendant_has_copy_request;
  layer->draw_properties().layer_or_descendant_has_input_handler =
      recursive_data->layer_or_descendant_has_input_handler;
}

static void RoundTranslationComponents(gfx::Transform* transform) {
  transform->matrix().set(0, 3, MathUtil::Round(transform->matrix().get(0, 3)));
  transform->matrix().set(1, 3, MathUtil::Round(transform->matrix().get(1, 3)));
}

template <typename LayerType>
struct SubtreeGlobals {
  LayerSorter* layer_sorter;
  int max_texture_size;
  float device_scale_factor;
  float page_scale_factor;
  const LayerType* page_scale_application_layer;
  bool can_adjust_raster_scales;
  bool can_render_to_separate_surface;
};

template<typename LayerType>
struct DataForRecursion {
  // The accumulated sequence of transforms a layer will use to determine its
  // own draw transform.
  gfx::Transform parent_matrix;

  // The accumulated sequence of transforms a layer will use to determine its
  // own screen-space transform.
  gfx::Transform full_hierarchy_matrix;

  // The transform that removes all scrolling that may have occurred between a
  // fixed-position layer and its container, so that the layer actually does
  // remain fixed.
  gfx::Transform scroll_compensation_matrix;

  // The ancestor that would be the container for any fixed-position / sticky
  // layers.
  LayerType* fixed_container;

  // This is the normal clip rect that is propagated from parent to child.
  gfx::Rect clip_rect_in_target_space;

  // When the layer's children want to compute their visible content rect, they
  // want to know what their target surface's clip rect will be. BUT - they
  // want to know this clip rect represented in their own target space. This
  // requires inverse-projecting the surface's clip rect from the surface's
  // render target space down to the surface's own space. Instead of computing
  // this value redundantly for each child layer, it is computed only once
  // while dealing with the parent layer, and then this precomputed value is
  // passed down the recursion to the children that actually use it.
  gfx::Rect clip_rect_of_target_surface_in_target_space;

  // The maximum amount by which this layer will be scaled during the lifetime
  // of currently running animations.
  float maximum_animation_contents_scale;

  bool ancestor_is_animating_scale;
  bool ancestor_clips_subtree;
  typename LayerType::RenderSurfaceType*
      nearest_occlusion_immune_ancestor_surface;
  bool in_subtree_of_page_scale_application_layer;
  bool subtree_can_use_lcd_text;
  bool subtree_is_visible_from_ancestor;
};

template <typename LayerType>
static LayerType* GetChildContainingLayer(const LayerType& parent,
                                          LayerType* layer) {
  for (LayerType* ancestor = layer; ancestor; ancestor = ancestor->parent()) {
    if (ancestor->parent() == &parent)
      return ancestor;
  }
  NOTREACHED();
  return 0;
}

template <typename LayerType>
static void AddScrollParentChain(std::vector<LayerType*>* out,
                                 const LayerType& parent,
                                 LayerType* layer) {
  // At a high level, this function walks up the chain of scroll parents
  // recursively, and once we reach the end of the chain, we add the child
  // of |parent| containing each scroll ancestor as we unwind. The result is
  // an ordering of parent's children that ensures that scroll parents are
  // visited before their descendants.
  // Take for example this layer tree:
  //
  // + stacking_context
  //   + scroll_child (1)
  //   + scroll_parent_graphics_layer (*)
  //   | + scroll_parent_scrolling_layer
  //   |   + scroll_parent_scrolling_content_layer (2)
  //   + scroll_grandparent_graphics_layer (**)
  //     + scroll_grandparent_scrolling_layer
  //       + scroll_grandparent_scrolling_content_layer (3)
  //
  // The scroll child is (1), its scroll parent is (2) and its scroll
  // grandparent is (3). Note, this doesn't mean that (2)'s scroll parent is
  // (3), it means that (*)'s scroll parent is (3). We don't want our list to
  // look like [ (3), (2), (1) ], even though that does have the ancestor chain
  // in the right order. Instead, we want [ (**), (*), (1) ]. That is, only want
  // (1)'s siblings in the list, but we want them to appear in such an order
  // that the scroll ancestors get visited in the correct order.
  //
  // So our first task at this step of the recursion is to determine the layer
  // that we will potentionally add to the list. That is, the child of parent
  // containing |layer|.
  LayerType* child = GetChildContainingLayer(parent, layer);
  if (child->draw_properties().sorted_for_recursion)
    return;

  if (LayerType* scroll_parent = child->scroll_parent())
    AddScrollParentChain(out, parent, scroll_parent);

  out->push_back(child);
  child->draw_properties().sorted_for_recursion = true;
}

template <typename LayerType>
static bool SortChildrenForRecursion(std::vector<LayerType*>* out,
                                     const LayerType& parent) {
  out->reserve(parent.children().size());
  bool order_changed = false;
  for (size_t i = 0; i < parent.children().size(); ++i) {
    LayerType* current =
        LayerTreeHostCommon::get_layer_as_raw_ptr(parent.children(), i);

    if (current->draw_properties().sorted_for_recursion) {
      order_changed = true;
      continue;
    }

    AddScrollParentChain(out, parent, current);
  }

  DCHECK_EQ(parent.children().size(), out->size());
  return order_changed;
}

template <typename LayerType>
static void GetNewDescendantsStartIndexAndCount(LayerType* layer,
                                                size_t* start_index,
                                                size_t* count) {
  *start_index = layer->draw_properties().index_of_first_descendants_addition;
  *count = layer->draw_properties().num_descendants_added;
}

template <typename LayerType>
static void GetNewRenderSurfacesStartIndexAndCount(LayerType* layer,
                                                   size_t* start_index,
                                                   size_t* count) {
  *start_index = layer->draw_properties()
                     .index_of_first_render_surface_layer_list_addition;
  *count = layer->draw_properties().num_render_surfaces_added;
}

// We need to extract a list from the the two flavors of RenderSurfaceListType
// for use in the sorting function below.
static LayerList* GetLayerListForSorting(RenderSurfaceLayerList* rsll) {
  return &rsll->AsLayerList();
}

static LayerImplList* GetLayerListForSorting(LayerImplList* layer_list) {
  return layer_list;
}

template <typename LayerType, typename GetIndexAndCountType>
static void SortLayerListContributions(
    const LayerType& parent,
    typename LayerType::LayerListType* unsorted,
    size_t start_index_for_all_contributions,
    GetIndexAndCountType get_index_and_count) {
  typename LayerType::LayerListType buffer;
  for (size_t i = 0; i < parent.children().size(); ++i) {
    LayerType* child =
        LayerTreeHostCommon::get_layer_as_raw_ptr(parent.children(), i);

    size_t start_index = 0;
    size_t count = 0;
    get_index_and_count(child, &start_index, &count);
    for (size_t j = start_index; j < start_index + count; ++j)
      buffer.push_back(unsorted->at(j));
  }

  DCHECK_EQ(buffer.size(),
            unsorted->size() - start_index_for_all_contributions);

  for (size_t i = 0; i < buffer.size(); ++i)
    (*unsorted)[i + start_index_for_all_contributions] = buffer[i];
}

// Recursively walks the layer tree starting at the given node and computes all
// the necessary transformations, clip rects, render surfaces, etc.
template <typename LayerType>
static void CalculateDrawPropertiesInternal(
    LayerType* layer,
    const SubtreeGlobals<LayerType>& globals,
    const DataForRecursion<LayerType>& data_from_ancestor,
    typename LayerType::RenderSurfaceListType* render_surface_layer_list,
    typename LayerType::LayerListType* layer_list,
    std::vector<AccumulatedSurfaceState<LayerType> >* accumulated_surface_state,
    int current_render_surface_layer_list_id) {
  // This function computes the new matrix transformations recursively for this
  // layer and all its descendants. It also computes the appropriate render
  // surfaces.
  // Some important points to remember:
  //
  // 0. Here, transforms are notated in Matrix x Vector order, and in words we
  // describe what the transform does from left to right.
  //
  // 1. In our terminology, the "layer origin" refers to the top-left corner of
  // a layer, and the positive Y-axis points downwards. This interpretation is
  // valid because the orthographic projection applied at draw time flips the Y
  // axis appropriately.
  //
  // 2. The anchor point, when given as a PointF object, is specified in "unit
  // layer space", where the bounds of the layer map to [0, 1]. However, as a
  // Transform object, the transform to the anchor point is specified in "layer
  // space", where the bounds of the layer map to [bounds.width(),
  // bounds.height()].
  //
  // 3. Definition of various transforms used:
  //        M[parent] is the parent matrix, with respect to the nearest render
  //        surface, passed down recursively.
  //
  //        M[root] is the full hierarchy, with respect to the root, passed down
  //        recursively.
  //
  //        Tr[origin] is the translation matrix from the parent's origin to
  //        this layer's origin.
  //
  //        Tr[origin2anchor] is the translation from the layer's origin to its
  //        anchor point
  //
  //        Tr[origin2center] is the translation from the layer's origin to its
  //        center
  //
  //        M[layer] is the layer's matrix (applied at the anchor point)
  //
  //        S[layer2content] is the ratio of a layer's content_bounds() to its
  //        Bounds().
  //
  //    Some composite transforms can help in understanding the sequence of
  //    transforms:
  //        composite_layer_transform = Tr[origin2anchor] * M[layer] *
  //        Tr[origin2anchor].inverse()
  //
  // 4. When a layer (or render surface) is drawn, it is drawn into a "target
  // render surface". Therefore the draw transform does not necessarily
  // transform from screen space to local layer space. Instead, the draw
  // transform is the transform between the "target render surface space" and
  // local layer space. Note that render surfaces, except for the root, also
  // draw themselves into a different target render surface, and so their draw
  // transform and origin transforms are also described with respect to the
  // target.
  //
  // Using these definitions, then:
  //
  // The draw transform for the layer is:
  //        M[draw] = M[parent] * Tr[origin] * composite_layer_transform *
  //            S[layer2content] = M[parent] * Tr[layer->position() + anchor] *
  //            M[layer] * Tr[anchor2origin] * S[layer2content]
  //
  //        Interpreting the math left-to-right, this transforms from the
  //        layer's render surface to the origin of the layer in content space.
  //
  // The screen space transform is:
  //        M[screenspace] = M[root] * Tr[origin] * composite_layer_transform *
  //            S[layer2content]
  //                       = M[root] * Tr[layer->position() + anchor] * M[layer]
  //                           * Tr[anchor2origin] * S[layer2content]
  //
  //        Interpreting the math left-to-right, this transforms from the root
  //        render surface's content space to the origin of the layer in content
  //        space.
  //
  // The transform hierarchy that is passed on to children (i.e. the child's
  // parent_matrix) is:
  //        M[parent]_for_child = M[parent] * Tr[origin] *
  //            composite_layer_transform
  //                            = M[parent] * Tr[layer->position() + anchor] *
  //                              M[layer] * Tr[anchor2origin]
  //
  //        and a similar matrix for the full hierarchy with respect to the
  //        root.
  //
  // Finally, note that the final matrix used by the shader for the layer is P *
  // M[draw] * S . This final product is computed in drawTexturedQuad(), where:
  //        P is the projection matrix
  //        S is the scale adjustment (to scale up a canonical quad to the
  //            layer's size)
  //
  // When a render surface has a replica layer, that layer's transform is used
  // to draw a second copy of the surface.  gfx::Transforms named here are
  // relative to the surface, unless they specify they are relative to the
  // replica layer.
  //
  // We will denote a scale by device scale S[deviceScale]
  //
  // The render surface draw transform to its target surface origin is:
  //        M[surfaceDraw] = M[owningLayer->Draw]
  //
  // The render surface origin transform to its the root (screen space) origin
  // is:
  //        M[surface2root] =  M[owningLayer->screenspace] *
  //            S[deviceScale].inverse()
  //
  // The replica draw transform to its target surface origin is:
  //        M[replicaDraw] = S[deviceScale] * M[surfaceDraw] *
  //            Tr[replica->position() + replica->anchor()] * Tr[replica] *
  //            Tr[origin2anchor].inverse() * S[contents_scale].inverse()
  //
  // The replica draw transform to the root (screen space) origin is:
  //        M[replica2root] = M[surface2root] * Tr[replica->position()] *
  //            Tr[replica] * Tr[origin2anchor].inverse()
  //

  // It makes no sense to have a non-unit page_scale_factor without specifying
  // which layer roots the subtree the scale is applied to.
  DCHECK(globals.page_scale_application_layer ||
         (globals.page_scale_factor == 1.f));

  DataForRecursion<LayerType> data_for_children;
  typename LayerType::RenderSurfaceType*
      nearest_occlusion_immune_ancestor_surface =
          data_from_ancestor.nearest_occlusion_immune_ancestor_surface;
  data_for_children.in_subtree_of_page_scale_application_layer =
      data_from_ancestor.in_subtree_of_page_scale_application_layer;
  data_for_children.subtree_can_use_lcd_text =
      data_from_ancestor.subtree_can_use_lcd_text;

  // Layers that are marked as hidden will hide themselves and their subtree.
  // Exception: Layers with copy requests, whether hidden or not, must be drawn
  // anyway.  In this case, we will inform their subtree they are visible to get
  // the right results.
  const bool layer_is_visible =
      data_from_ancestor.subtree_is_visible_from_ancestor &&
      !layer->hide_layer_and_subtree();
  const bool layer_is_drawn = layer_is_visible || layer->HasCopyRequest();

  // The root layer cannot skip CalcDrawProperties.
  if (!IsRootLayer(layer) && SubtreeShouldBeSkipped(layer, layer_is_drawn)) {
    if (layer->render_surface())
      layer->ClearRenderSurfaceLayerList();
    return;
  }

  // We need to circumvent the normal recursive flow of information for clip
  // children (they don't inherit their direct ancestor's clip information).
  // This is unfortunate, and would be unnecessary if we were to formally
  // separate the clipping hierarchy from the layer hierarchy.
  bool ancestor_clips_subtree = data_from_ancestor.ancestor_clips_subtree;
  gfx::Rect ancestor_clip_rect_in_target_space =
      data_from_ancestor.clip_rect_in_target_space;

  // Update our clipping state. If we have a clip parent we will need to pull
  // from the clip state cache rather than using the clip state passed from our
  // immediate ancestor.
  UpdateClipRectsForClipChild<LayerType>(
      layer, &ancestor_clip_rect_in_target_space, &ancestor_clips_subtree);

  // As this function proceeds, these are the properties for the current
  // layer that actually get computed. To avoid unnecessary copies
  // (particularly for matrices), we do computations directly on these values
  // when possible.
  DrawProperties<LayerType>& layer_draw_properties = layer->draw_properties();

  gfx::Rect clip_rect_in_target_space;
  bool layer_or_ancestor_clips_descendants = false;

  // This value is cached on the stack so that we don't have to inverse-project
  // the surface's clip rect redundantly for every layer. This value is the
  // same as the target surface's clip rect, except that instead of being
  // described in the target surface's target's space, it is described in the
  // current render target's space.
  gfx::Rect clip_rect_of_target_surface_in_target_space;

  float accumulated_draw_opacity = layer->opacity();
  bool animating_opacity_to_target = layer->OpacityIsAnimating();
  bool animating_opacity_to_screen = animating_opacity_to_target;
  if (layer->parent()) {
    accumulated_draw_opacity *= layer->parent()->draw_opacity();
    animating_opacity_to_target |= layer->parent()->draw_opacity_is_animating();
    animating_opacity_to_screen |=
        layer->parent()->screen_space_opacity_is_animating();
  }

  bool animating_transform_to_target = layer->TransformIsAnimating();
  bool animating_transform_to_screen = animating_transform_to_target;
  if (layer->parent()) {
    animating_transform_to_target |=
        layer->parent()->draw_transform_is_animating();
    animating_transform_to_screen |=
        layer->parent()->screen_space_transform_is_animating();
  }
  gfx::Point3F transform_origin = layer->transform_origin();
  gfx::Vector2dF scroll_offset = GetEffectiveTotalScrollOffset(layer);
  gfx::PointF position = layer->position() - scroll_offset;
  gfx::Transform combined_transform = data_from_ancestor.parent_matrix;
  if (!layer->transform().IsIdentity()) {
    // LT = Tr[origin] * Tr[origin2transformOrigin]
    combined_transform.Translate3d(position.x() + transform_origin.x(),
                                   position.y() + transform_origin.y(),
                                   transform_origin.z());
    // LT = Tr[origin] * Tr[origin2origin] * M[layer]
    combined_transform.PreconcatTransform(layer->transform());
    // LT = Tr[origin] * Tr[origin2origin] * M[layer] *
    // Tr[transformOrigin2origin]
    combined_transform.Translate3d(
        -transform_origin.x(), -transform_origin.y(), -transform_origin.z());
  } else {
    combined_transform.Translate(position.x(), position.y());
  }

  gfx::Vector2dF effective_scroll_delta = GetEffectiveScrollDelta(layer);
  if (!animating_transform_to_target && layer->scrollable() &&
      combined_transform.IsScaleOrTranslation()) {
    // Align the scrollable layer's position to screen space pixels to avoid
    // blurriness.  To avoid side-effects, do this only if the transform is
    // simple.
    gfx::Vector2dF previous_translation = combined_transform.To2dTranslation();
    RoundTranslationComponents(&combined_transform);
    gfx::Vector2dF current_translation = combined_transform.To2dTranslation();

    // This rounding changes the scroll delta, and so must be included
    // in the scroll compensation matrix.  The scaling converts from physical
    // coordinates to the scroll delta's CSS coordinates (using the parent
    // matrix instead of combined transform since scrolling is applied before
    // the layer's transform).  For example, if we have a total scale factor of
    // 3.0, then 1 physical pixel is only 1/3 of a CSS pixel.
    gfx::Vector2dF parent_scales = MathUtil::ComputeTransform2dScaleComponents(
        data_from_ancestor.parent_matrix, 1.f);
    effective_scroll_delta -=
        gfx::ScaleVector2d(current_translation - previous_translation,
                           1.f / parent_scales.x(),
                           1.f / parent_scales.y());
  }

  // Apply adjustment from position constraints.
  ApplyPositionAdjustment(layer, data_from_ancestor.fixed_container,
      data_from_ancestor.scroll_compensation_matrix, &combined_transform);

  bool combined_is_animating_scale = false;
  float combined_maximum_animation_contents_scale = 0.f;
  if (globals.can_adjust_raster_scales) {
    CalculateAnimationContentsScale(
        layer,
        data_from_ancestor.ancestor_is_animating_scale,
        data_from_ancestor.maximum_animation_contents_scale,
        data_from_ancestor.parent_matrix,
        combined_transform,
        &combined_is_animating_scale,
        &combined_maximum_animation_contents_scale);
  }
  data_for_children.ancestor_is_animating_scale = combined_is_animating_scale;
  data_for_children.maximum_animation_contents_scale =
      combined_maximum_animation_contents_scale;

  // Compute the 2d scale components of the transform hierarchy up to the target
  // surface. From there, we can decide on a contents scale for the layer.
  float layer_scale_factors = globals.device_scale_factor;
  if (data_from_ancestor.in_subtree_of_page_scale_application_layer)
    layer_scale_factors *= globals.page_scale_factor;
  gfx::Vector2dF combined_transform_scales =
      MathUtil::ComputeTransform2dScaleComponents(
          combined_transform,
          layer_scale_factors);

  float ideal_contents_scale =
      globals.can_adjust_raster_scales
      ? std::max(combined_transform_scales.x(),
                 combined_transform_scales.y())
      : layer_scale_factors;
  UpdateLayerContentsScale(
      layer,
      globals.can_adjust_raster_scales,
      ideal_contents_scale,
      globals.device_scale_factor,
      data_from_ancestor.in_subtree_of_page_scale_application_layer
          ? globals.page_scale_factor
          : 1.f,
      combined_maximum_animation_contents_scale,
      animating_transform_to_screen);

  UpdateLayerScaleDrawProperties(
      layer,
      ideal_contents_scale,
      combined_maximum_animation_contents_scale,
      data_from_ancestor.in_subtree_of_page_scale_application_layer
          ? globals.page_scale_factor
          : 1.f,
      globals.device_scale_factor);

  LayerType* mask_layer = layer->mask_layer();
  if (mask_layer) {
    UpdateLayerScaleDrawProperties(
        mask_layer,
        ideal_contents_scale,
        combined_maximum_animation_contents_scale,
        data_from_ancestor.in_subtree_of_page_scale_application_layer
            ? globals.page_scale_factor
            : 1.f,
        globals.device_scale_factor);
  }

  LayerType* replica_mask_layer =
      layer->replica_layer() ? layer->replica_layer()->mask_layer() : NULL;
  if (replica_mask_layer) {
    UpdateLayerScaleDrawProperties(
        replica_mask_layer,
        ideal_contents_scale,
        combined_maximum_animation_contents_scale,
        data_from_ancestor.in_subtree_of_page_scale_application_layer
            ? globals.page_scale_factor
            : 1.f,
        globals.device_scale_factor);
  }

  // The draw_transform that gets computed below is effectively the layer's
  // draw_transform, unless the layer itself creates a render_surface. In that
  // case, the render_surface re-parents the transforms.
  layer_draw_properties.target_space_transform = combined_transform;
  // M[draw] = M[parent] * LT * S[layer2content]
  layer_draw_properties.target_space_transform.Scale(
      SK_MScalar1 / layer->contents_scale_x(),
      SK_MScalar1 / layer->contents_scale_y());

  // The layer's screen_space_transform represents the transform between root
  // layer's "screen space" and local content space.
  layer_draw_properties.screen_space_transform =
      data_from_ancestor.full_hierarchy_matrix;
  if (layer->should_flatten_transform())
    layer_draw_properties.screen_space_transform.FlattenTo2d();
  layer_draw_properties.screen_space_transform.PreconcatTransform
      (layer_draw_properties.target_space_transform);

  // Adjusting text AA method during animation may cause repaints, which in-turn
  // causes jank.
  bool adjust_text_aa =
      !animating_opacity_to_screen && !animating_transform_to_screen;
  // To avoid color fringing, LCD text should only be used on opaque layers with
  // just integral translation.
  bool layer_can_use_lcd_text =
      data_from_ancestor.subtree_can_use_lcd_text &&
      accumulated_draw_opacity == 1.f &&
      layer_draw_properties.target_space_transform.
          IsIdentityOrIntegerTranslation();

  gfx::Rect content_rect(layer->content_bounds());

  // full_hierarchy_matrix is the matrix that transforms objects between screen
  // space (except projection matrix) and the most recent RenderSurfaceImpl's
  // space.  next_hierarchy_matrix will only change if this layer uses a new
  // RenderSurfaceImpl, otherwise remains the same.
  data_for_children.full_hierarchy_matrix =
      data_from_ancestor.full_hierarchy_matrix;

  // If the subtree will scale layer contents by the transform hierarchy, then
  // we should scale things into the render surface by the transform hierarchy
  // to take advantage of that.
  gfx::Vector2dF render_surface_sublayer_scale =
      globals.can_adjust_raster_scales
      ? combined_transform_scales
      : gfx::Vector2dF(layer_scale_factors, layer_scale_factors);

  bool render_to_separate_surface;
  if (globals.can_render_to_separate_surface) {
    render_to_separate_surface = SubtreeShouldRenderToSeparateSurface(
          layer, combined_transform.Preserves2dAxisAlignment());
  } else {
    render_to_separate_surface = IsRootLayer(layer);
  }
  if (render_to_separate_surface) {
    // Check back-face visibility before continuing with this surface and its
    // subtree
    if (!layer->double_sided() && TransformToParentIsKnown(layer) &&
        IsSurfaceBackFaceVisible(layer, combined_transform)) {
      layer->ClearRenderSurfaceLayerList();
      return;
    }

    typename LayerType::RenderSurfaceType* render_surface =
        CreateOrReuseRenderSurface(layer);

    if (IsRootLayer(layer)) {
      // The root layer's render surface size is predetermined and so the root
      // layer can't directly support non-identity transforms.  It should just
      // forward top-level transforms to the rest of the tree.
      data_for_children.parent_matrix = combined_transform;

      // The root surface does not contribute to any other surface, it has no
      // target.
      layer->render_surface()->set_contributes_to_drawn_surface(false);
    } else {
      // The owning layer's draw transform has a scale from content to layer
      // space which we do not want; so here we use the combined_transform
      // instead of the draw_transform. However, we do need to add a different
      // scale factor that accounts for the surface's pixel dimensions.
      combined_transform.Scale(1.0 / render_surface_sublayer_scale.x(),
                               1.0 / render_surface_sublayer_scale.y());
      render_surface->SetDrawTransform(combined_transform);

      // The owning layer's transform was re-parented by the surface, so the
      // layer's new draw_transform only needs to scale the layer to surface
      // space.
      layer_draw_properties.target_space_transform.MakeIdentity();
      layer_draw_properties.target_space_transform.
          Scale(render_surface_sublayer_scale.x() / layer->contents_scale_x(),
                render_surface_sublayer_scale.y() / layer->contents_scale_y());

      // Inside the surface's subtree, we scale everything to the owning layer's
      // scale.  The sublayer matrix transforms layer rects into target surface
      // content space.  Conceptually, all layers in the subtree inherit the
      // scale at the point of the render surface in the transform hierarchy,
      // but we apply it explicitly to the owning layer and the remainder of the
      // subtree independently.
      DCHECK(data_for_children.parent_matrix.IsIdentity());
      data_for_children.parent_matrix.Scale(render_surface_sublayer_scale.x(),
                            render_surface_sublayer_scale.y());

      // Even if the |layer_is_drawn|, it only contributes to a drawn surface
      // when the |layer_is_visible|.
      layer->render_surface()->set_contributes_to_drawn_surface(
          layer_is_visible);
    }

    // The opacity value is moved from the layer to its surface, so that the
    // entire subtree properly inherits opacity.
    render_surface->SetDrawOpacity(accumulated_draw_opacity);
    render_surface->SetDrawOpacityIsAnimating(animating_opacity_to_target);
    animating_opacity_to_target = false;
    layer_draw_properties.opacity = 1.f;
    layer_draw_properties.opacity_is_animating = animating_opacity_to_target;
    layer_draw_properties.screen_space_opacity_is_animating =
        animating_opacity_to_screen;

    render_surface->SetTargetSurfaceTransformsAreAnimating(
        animating_transform_to_target);
    render_surface->SetScreenSpaceTransformsAreAnimating(
        animating_transform_to_screen);
    animating_transform_to_target = false;
    layer_draw_properties.target_space_transform_is_animating =
        animating_transform_to_target;
    layer_draw_properties.screen_space_transform_is_animating =
        animating_transform_to_screen;

    // Update the aggregate hierarchy matrix to include the transform of the
    // newly created RenderSurfaceImpl.
    data_for_children.full_hierarchy_matrix.PreconcatTransform(
        render_surface->draw_transform());

    if (layer->mask_layer()) {
      DrawProperties<LayerType>& mask_layer_draw_properties =
          layer->mask_layer()->draw_properties();
      mask_layer_draw_properties.render_target = layer;
      mask_layer_draw_properties.visible_content_rect =
          gfx::Rect(layer->content_bounds());
    }

    if (layer->replica_layer() && layer->replica_layer()->mask_layer()) {
      DrawProperties<LayerType>& replica_mask_draw_properties =
          layer->replica_layer()->mask_layer()->draw_properties();
      replica_mask_draw_properties.render_target = layer;
      replica_mask_draw_properties.visible_content_rect =
          gfx::Rect(layer->content_bounds());
    }

    // Ignore occlusion from outside the surface when surface contents need to
    // be fully drawn. Layers with copy-request need to be complete.
    // We could be smarter about layers with replica and exclude regions
    // where both layer and the replica are occluded, but this seems like an
    // overkill. The same is true for layers with filters that move pixels.
    // TODO(senorblanco): make this smarter for the SkImageFilter case (check
    // for pixel-moving filters)
    if (layer->HasCopyRequest() ||
        layer->has_replica() ||
        layer->filters().HasReferenceFilter() ||
        layer->filters().HasFilterThatMovesPixels()) {
      nearest_occlusion_immune_ancestor_surface = render_surface;
    }
    render_surface->SetNearestOcclusionImmuneAncestor(
        nearest_occlusion_immune_ancestor_surface);

    layer_or_ancestor_clips_descendants = false;
    bool subtree_is_clipped_by_surface_bounds = false;
    if (ancestor_clips_subtree) {
      // It may be the layer or the surface doing the clipping of the subtree,
      // but in either case, we'll be clipping to the projected clip rect of our
      // ancestor.
      gfx::Transform inverse_surface_draw_transform(
          gfx::Transform::kSkipInitialization);
      if (!render_surface->draw_transform().GetInverse(
              &inverse_surface_draw_transform)) {
        // TODO(shawnsingh): Either we need to handle uninvertible transforms
        // here, or DCHECK that the transform is invertible.
      }

      gfx::Rect projected_surface_rect = MathUtil::ProjectEnclosingClippedRect(
          inverse_surface_draw_transform, ancestor_clip_rect_in_target_space);

      if (layer_draw_properties.num_unclipped_descendants > 0) {
        // If we have unclipped descendants, we cannot count on the render
        // surface's bounds clipping our subtree: the unclipped descendants
        // could cause us to expand our bounds. In this case, we must rely on
        // layer clipping for correctess. NB: since we can only encounter
        // translations between a clip child and its clip parent, clipping is
        // guaranteed to be exact in this case.
        layer_or_ancestor_clips_descendants = true;
        clip_rect_in_target_space = projected_surface_rect;
      } else {
        // The new render_surface here will correctly clip the entire subtree.
        // So, we do not need to continue propagating the clipping state further
        // down the tree. This way, we can avoid transforming clip rects from
        // ancestor target surface space to current target surface space that
        // could cause more w < 0 headaches. The render surface clip rect is
        // expressed in the space where this surface draws, i.e. the same space
        // as clip_rect_from_ancestor_in_ancestor_target_space.
        render_surface->SetClipRect(ancestor_clip_rect_in_target_space);
        clip_rect_of_target_surface_in_target_space = projected_surface_rect;
        subtree_is_clipped_by_surface_bounds = true;
      }
    }

    DCHECK(layer->render_surface());
    DCHECK(!layer->parent() || layer->parent()->render_target() ==
           accumulated_surface_state->back().render_target);

    accumulated_surface_state->push_back(
        AccumulatedSurfaceState<LayerType>(layer));

    render_surface->SetIsClipped(subtree_is_clipped_by_surface_bounds);
    if (!subtree_is_clipped_by_surface_bounds) {
      render_surface->SetClipRect(gfx::Rect());
      clip_rect_of_target_surface_in_target_space =
          data_from_ancestor.clip_rect_of_target_surface_in_target_space;
    }

    // If the new render surface is drawn translucent or with a non-integral
    // translation then the subtree that gets drawn on this render surface
    // cannot use LCD text.
    data_for_children.subtree_can_use_lcd_text = layer_can_use_lcd_text;

    render_surface_layer_list->push_back(layer);
  } else {
    DCHECK(layer->parent());

    // Note: layer_draw_properties.target_space_transform is computed above,
    // before this if-else statement.
    layer_draw_properties.target_space_transform_is_animating =
        animating_transform_to_target;
    layer_draw_properties.screen_space_transform_is_animating =
        animating_transform_to_screen;
    layer_draw_properties.opacity = accumulated_draw_opacity;
    layer_draw_properties.opacity_is_animating = animating_opacity_to_target;
    layer_draw_properties.screen_space_opacity_is_animating =
        animating_opacity_to_screen;
    data_for_children.parent_matrix = combined_transform;

    layer->ClearRenderSurface();

    // Layers without render_surfaces directly inherit the ancestor's clip
    // status.
    layer_or_ancestor_clips_descendants = ancestor_clips_subtree;
    if (ancestor_clips_subtree) {
      clip_rect_in_target_space =
          ancestor_clip_rect_in_target_space;
    }

    // The surface's cached clip rect value propagates regardless of what
    // clipping goes on between layers here.
    clip_rect_of_target_surface_in_target_space =
        data_from_ancestor.clip_rect_of_target_surface_in_target_space;

    // Layers that are not their own render_target will render into the target
    // of their nearest ancestor.
    layer_draw_properties.render_target = layer->parent()->render_target();
  }

  if (adjust_text_aa)
    layer_draw_properties.can_use_lcd_text = layer_can_use_lcd_text;

  gfx::Rect rect_in_target_space =
      MathUtil::MapEnclosingClippedRect(layer->draw_transform(), content_rect);

  if (LayerClipsSubtree(layer)) {
    layer_or_ancestor_clips_descendants = true;
    if (ancestor_clips_subtree && !layer->render_surface()) {
      // A layer without render surface shares the same target as its ancestor.
      clip_rect_in_target_space =
          ancestor_clip_rect_in_target_space;
      clip_rect_in_target_space.Intersect(rect_in_target_space);
    } else {
      clip_rect_in_target_space = rect_in_target_space;
    }
  }

  // Tell the layer the rect that it's clipped by. In theory we could use a
  // tighter clip rect here (drawable_content_rect), but that actually does not
  // reduce how much would be drawn, and instead it would create unnecessary
  // changes to scissor state affecting GPU performance. Our clip information
  // is used in the recursion below, so we must set it beforehand.
  layer_draw_properties.is_clipped = layer_or_ancestor_clips_descendants;
  if (layer_or_ancestor_clips_descendants) {
    layer_draw_properties.clip_rect = clip_rect_in_target_space;
  } else {
    // Initialize the clip rect to a safe value that will not clip the
    // layer, just in case clipping is still accidentally used.
    layer_draw_properties.clip_rect = rect_in_target_space;
  }

  typename LayerType::LayerListType& descendants =
      (layer->render_surface() ? layer->render_surface()->layer_list()
                               : *layer_list);

  // Any layers that are appended after this point are in the layer's subtree
  // and should be included in the sorting process.
  size_t sorting_start_index = descendants.size();

  if (!LayerShouldBeSkipped(layer, layer_is_drawn)) {
    MarkLayerWithRenderSurfaceLayerListId(layer,
                                          current_render_surface_layer_list_id);
    descendants.push_back(layer);
  }

  // Any layers that are appended after this point may need to be sorted if we
  // visit the children out of order.
  size_t render_surface_layer_list_child_sorting_start_index =
      render_surface_layer_list->size();
  size_t layer_list_child_sorting_start_index = descendants.size();

  if (!layer->children().empty()) {
    if (layer == globals.page_scale_application_layer) {
      data_for_children.parent_matrix.Scale(
          globals.page_scale_factor,
          globals.page_scale_factor);
      data_for_children.in_subtree_of_page_scale_application_layer = true;
    }

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (layer->should_flatten_transform())
      data_for_children.parent_matrix.FlattenTo2d();

    data_for_children.scroll_compensation_matrix =
        ComputeScrollCompensationMatrixForChildren(
            layer,
            data_from_ancestor.parent_matrix,
            data_from_ancestor.scroll_compensation_matrix,
            effective_scroll_delta);
    data_for_children.fixed_container =
        layer->IsContainerForFixedPositionLayers() ?
            layer : data_from_ancestor.fixed_container;

    data_for_children.clip_rect_in_target_space = clip_rect_in_target_space;
    data_for_children.clip_rect_of_target_surface_in_target_space =
        clip_rect_of_target_surface_in_target_space;
    data_for_children.ancestor_clips_subtree =
        layer_or_ancestor_clips_descendants;
    data_for_children.nearest_occlusion_immune_ancestor_surface =
        nearest_occlusion_immune_ancestor_surface;
    data_for_children.subtree_is_visible_from_ancestor = layer_is_drawn;
  }

  std::vector<LayerType*> sorted_children;
  bool child_order_changed = false;
  if (layer_draw_properties.has_child_with_a_scroll_parent)
    child_order_changed = SortChildrenForRecursion(&sorted_children, *layer);

  for (size_t i = 0; i < layer->children().size(); ++i) {
    // If one of layer's children has a scroll parent, then we may have to
    // visit the children out of order. The new order is stored in
    // sorted_children. Otherwise, we'll grab the child directly from the
    // layer's list of children.
    LayerType* child =
        layer_draw_properties.has_child_with_a_scroll_parent
            ? sorted_children[i]
            : LayerTreeHostCommon::get_layer_as_raw_ptr(layer->children(), i);

    child->draw_properties().index_of_first_descendants_addition =
        descendants.size();
    child->draw_properties().index_of_first_render_surface_layer_list_addition =
        render_surface_layer_list->size();

    CalculateDrawPropertiesInternal<LayerType>(
        child,
        globals,
        data_for_children,
        render_surface_layer_list,
        &descendants,
        accumulated_surface_state,
        current_render_surface_layer_list_id);
    if (child->render_surface() &&
        !child->render_surface()->layer_list().empty() &&
        !child->render_surface()->content_rect().IsEmpty()) {
      // This child will contribute its render surface, which means
      // we need to mark just the mask layer (and replica mask layer)
      // with the id.
      MarkMasksWithRenderSurfaceLayerListId(
          child, current_render_surface_layer_list_id);
      descendants.push_back(child);
    }

    child->draw_properties().num_descendants_added =
        descendants.size() -
        child->draw_properties().index_of_first_descendants_addition;
    child->draw_properties().num_render_surfaces_added =
        render_surface_layer_list->size() -
        child->draw_properties()
            .index_of_first_render_surface_layer_list_addition;
  }

  // Add the unsorted layer list contributions, if necessary.
  if (child_order_changed) {
    SortLayerListContributions(
        *layer,
        GetLayerListForSorting(render_surface_layer_list),
        render_surface_layer_list_child_sorting_start_index,
        &GetNewRenderSurfacesStartIndexAndCount<LayerType>);

    SortLayerListContributions(
        *layer,
        &descendants,
        layer_list_child_sorting_start_index,
        &GetNewDescendantsStartIndexAndCount<LayerType>);
  }

  // Compute the total drawable_content_rect for this subtree (the rect is in
  // target surface space).
  gfx::Rect local_drawable_content_rect_of_subtree =
      accumulated_surface_state->back().drawable_content_rect;
  if (layer->render_surface()) {
    DCHECK(accumulated_surface_state->back().render_target == layer);
    accumulated_surface_state->pop_back();
  }

  if (layer->render_surface() && !IsRootLayer(layer) &&
      layer->render_surface()->layer_list().empty()) {
    RemoveSurfaceForEarlyExit(layer, render_surface_layer_list);
    return;
  }

  // Compute the layer's drawable content rect (the rect is in target surface
  // space).
  layer_draw_properties.drawable_content_rect = rect_in_target_space;
  if (layer_or_ancestor_clips_descendants) {
    layer_draw_properties.drawable_content_rect.Intersect(
        clip_rect_in_target_space);
  }
  if (layer->DrawsContent()) {
    local_drawable_content_rect_of_subtree.Union(
        layer_draw_properties.drawable_content_rect);
  }

  // Compute the layer's visible content rect (the rect is in content space).
  layer_draw_properties.visible_content_rect = CalculateVisibleContentRect(
      layer, clip_rect_of_target_surface_in_target_space, rect_in_target_space);

  // Compute the remaining properties for the render surface, if the layer has
  // one.
  if (IsRootLayer(layer)) {
    // The root layer's surface's content_rect is always the entire viewport.
    DCHECK(layer->render_surface());
    layer->render_surface()->SetContentRect(
        ancestor_clip_rect_in_target_space);
  } else if (layer->render_surface()) {
    typename LayerType::RenderSurfaceType* render_surface =
        layer->render_surface();
    gfx::Rect clipped_content_rect = local_drawable_content_rect_of_subtree;

    // Don't clip if the layer is reflected as the reflection shouldn't be
    // clipped. If the layer is animating, then the surface's transform to
    // its target is not known on the main thread, and we should not use it
    // to clip.
    if (!layer->replica_layer() && TransformToParentIsKnown(layer)) {
      // Note, it is correct to use data_from_ancestor.ancestor_clips_subtree
      // here, because we are looking at this layer's render_surface, not the
      // layer itself.
      if (render_surface->is_clipped() && !clipped_content_rect.IsEmpty()) {
        gfx::Rect surface_clip_rect = LayerTreeHostCommon::CalculateVisibleRect(
            render_surface->clip_rect(),
            clipped_content_rect,
            render_surface->draw_transform());
        clipped_content_rect.Intersect(surface_clip_rect);
      }
    }

    // The RenderSurfaceImpl backing texture cannot exceed the maximum supported
    // texture size.
    clipped_content_rect.set_width(
        std::min(clipped_content_rect.width(), globals.max_texture_size));
    clipped_content_rect.set_height(
        std::min(clipped_content_rect.height(), globals.max_texture_size));

    if (clipped_content_rect.IsEmpty()) {
      RemoveSurfaceForEarlyExit(layer, render_surface_layer_list);
      return;
    }

    // Layers having a non-default blend mode will blend with the content
    // inside its parent's render target. This render target should be
    // either root_for_isolated_group, or the root of the layer tree.
    // Otherwise, this layer will use an incomplete backdrop, limited to its
    // render target and the blending result will be incorrect.
    DCHECK(layer->uses_default_blend_mode() || IsRootLayer(layer) ||
           !layer->parent()->render_target() ||
           IsRootLayer(layer->parent()->render_target()) ||
           layer->parent()->render_target()->is_root_for_isolated_group());

    render_surface->SetContentRect(clipped_content_rect);

    // The owning layer's screen_space_transform has a scale from content to
    // layer space which we need to undo and replace with a scale from the
    // surface's subtree into layer space.
    gfx::Transform screen_space_transform = layer->screen_space_transform();
    screen_space_transform.Scale(
        layer->contents_scale_x() / render_surface_sublayer_scale.x(),
        layer->contents_scale_y() / render_surface_sublayer_scale.y());
    render_surface->SetScreenSpaceTransform(screen_space_transform);

    if (layer->replica_layer()) {
      gfx::Transform surface_origin_to_replica_origin_transform;
      surface_origin_to_replica_origin_transform.Scale(
          render_surface_sublayer_scale.x(), render_surface_sublayer_scale.y());
      surface_origin_to_replica_origin_transform.Translate(
          layer->replica_layer()->position().x() +
              layer->replica_layer()->transform_origin().x(),
          layer->replica_layer()->position().y() +
              layer->replica_layer()->transform_origin().y());
      surface_origin_to_replica_origin_transform.PreconcatTransform(
          layer->replica_layer()->transform());
      surface_origin_to_replica_origin_transform.Translate(
          -layer->replica_layer()->transform_origin().x(),
          -layer->replica_layer()->transform_origin().y());
      surface_origin_to_replica_origin_transform.Scale(
          1.0 / render_surface_sublayer_scale.x(),
          1.0 / render_surface_sublayer_scale.y());

      // Compute the replica's "originTransform" that maps from the replica's
      // origin space to the target surface origin space.
      gfx::Transform replica_origin_transform =
          layer->render_surface()->draw_transform() *
          surface_origin_to_replica_origin_transform;
      render_surface->SetReplicaDrawTransform(replica_origin_transform);

      // Compute the replica's "screen_space_transform" that maps from the
      // replica's origin space to the screen's origin space.
      gfx::Transform replica_screen_space_transform =
          layer->render_surface()->screen_space_transform() *
          surface_origin_to_replica_origin_transform;
      render_surface->SetReplicaScreenSpaceTransform(
          replica_screen_space_transform);
    }
  }

  SavePaintPropertiesLayer(layer);

  // If neither this layer nor any of its children were added, early out.
  if (sorting_start_index == descendants.size()) {
    DCHECK(!layer->render_surface() || IsRootLayer(layer));
    return;
  }

  // If preserves-3d then sort all the descendants in 3D so that they can be
  // drawn from back to front. If the preserves-3d property is also set on the
  // parent then skip the sorting as the parent will sort all the descendants
  // anyway.
  if (globals.layer_sorter && descendants.size() && layer->Is3dSorted() &&
      !LayerIsInExisting3DRenderingContext(layer)) {
    SortLayers(descendants.begin() + sorting_start_index,
               descendants.end(),
               globals.layer_sorter);
  }

  UpdateAccumulatedSurfaceState<LayerType>(
      layer, local_drawable_content_rect_of_subtree, accumulated_surface_state);

  if (layer->HasContributingDelegatedRenderPasses()) {
    layer->render_target()->render_surface()->
        AddContributingDelegatedRenderPassLayer(layer);
  }
}

template <typename LayerType, typename RenderSurfaceLayerListType>
static void ProcessCalcDrawPropsInputs(
    const LayerTreeHostCommon::CalcDrawPropsInputs<LayerType,
                                                   RenderSurfaceLayerListType>&
        inputs,
    SubtreeGlobals<LayerType>* globals,
    DataForRecursion<LayerType>* data_for_recursion) {
  DCHECK(inputs.root_layer);
  DCHECK(IsRootLayer(inputs.root_layer));
  DCHECK(inputs.render_surface_layer_list);

  gfx::Transform identity_matrix;

  // The root layer's render_surface should receive the device viewport as the
  // initial clip rect.
  gfx::Rect device_viewport_rect(inputs.device_viewport_size);

  gfx::Vector2dF device_transform_scale_components =
      MathUtil::ComputeTransform2dScaleComponents(inputs.device_transform, 1.f);
  // Not handling the rare case of different x and y device scale.
  float device_transform_scale =
      std::max(device_transform_scale_components.x(),
               device_transform_scale_components.y());

  gfx::Transform scaled_device_transform = inputs.device_transform;
  scaled_device_transform.Scale(inputs.device_scale_factor,
                                inputs.device_scale_factor);

  globals->layer_sorter = NULL;
  globals->max_texture_size = inputs.max_texture_size;
  globals->device_scale_factor =
      inputs.device_scale_factor * device_transform_scale;
  globals->page_scale_factor = inputs.page_scale_factor;
  globals->page_scale_application_layer = inputs.page_scale_application_layer;
  globals->can_render_to_separate_surface =
      inputs.can_render_to_separate_surface;
  globals->can_adjust_raster_scales = inputs.can_adjust_raster_scales;

  data_for_recursion->parent_matrix = scaled_device_transform;
  data_for_recursion->full_hierarchy_matrix = identity_matrix;
  data_for_recursion->scroll_compensation_matrix = identity_matrix;
  data_for_recursion->fixed_container = inputs.root_layer;
  data_for_recursion->clip_rect_in_target_space = device_viewport_rect;
  data_for_recursion->clip_rect_of_target_surface_in_target_space =
      device_viewport_rect;
  data_for_recursion->maximum_animation_contents_scale = 0.f;
  data_for_recursion->ancestor_is_animating_scale = false;
  data_for_recursion->ancestor_clips_subtree = true;
  data_for_recursion->nearest_occlusion_immune_ancestor_surface = NULL;
  data_for_recursion->in_subtree_of_page_scale_application_layer = false;
  data_for_recursion->subtree_can_use_lcd_text = inputs.can_use_lcd_text;
  data_for_recursion->subtree_is_visible_from_ancestor = true;
}

void LayerTreeHostCommon::CalculateDrawProperties(
    CalcDrawPropsMainInputs* inputs) {
  LayerList dummy_layer_list;
  SubtreeGlobals<Layer> globals;
  DataForRecursion<Layer> data_for_recursion;
  ProcessCalcDrawPropsInputs(*inputs, &globals, &data_for_recursion);

  PreCalculateMetaInformationRecursiveData recursive_data;
  PreCalculateMetaInformation(inputs->root_layer, &recursive_data);
  std::vector<AccumulatedSurfaceState<Layer> > accumulated_surface_state;
  CalculateDrawPropertiesInternal<Layer>(
      inputs->root_layer,
      globals,
      data_for_recursion,
      inputs->render_surface_layer_list,
      &dummy_layer_list,
      &accumulated_surface_state,
      inputs->current_render_surface_layer_list_id);

  // The dummy layer list should not have been used.
  DCHECK_EQ(0u, dummy_layer_list.size());
  // A root layer render_surface should always exist after
  // CalculateDrawProperties.
  DCHECK(inputs->root_layer->render_surface());
}

void LayerTreeHostCommon::CalculateDrawProperties(
    CalcDrawPropsImplInputs* inputs) {
  LayerImplList dummy_layer_list;
  SubtreeGlobals<LayerImpl> globals;
  DataForRecursion<LayerImpl> data_for_recursion;
  ProcessCalcDrawPropsInputs(*inputs, &globals, &data_for_recursion);

  LayerSorter layer_sorter;
  globals.layer_sorter = &layer_sorter;

  PreCalculateMetaInformationRecursiveData recursive_data;
  PreCalculateMetaInformation(inputs->root_layer, &recursive_data);
  std::vector<AccumulatedSurfaceState<LayerImpl> >
      accumulated_surface_state;
  CalculateDrawPropertiesInternal<LayerImpl>(
      inputs->root_layer,
      globals,
      data_for_recursion,
      inputs->render_surface_layer_list,
      &dummy_layer_list,
      &accumulated_surface_state,
      inputs->current_render_surface_layer_list_id);

  // The dummy layer list should not have been used.
  DCHECK_EQ(0u, dummy_layer_list.size());
  // A root layer render_surface should always exist after
  // CalculateDrawProperties.
  DCHECK(inputs->root_layer->render_surface());
}

}  // namespace cc
