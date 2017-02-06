// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/draw_property_utils.h"

#include <stddef.h>

#include <vector>

#include "cc/base/math_util.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/property_tree_builder.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

namespace draw_property_utils {

namespace {

static bool IsRootLayer(const Layer* layer) {
  return !layer->parent();
}

static bool IsRootLayer(const LayerImpl* layer) {
  return layer->layer_tree_impl()->IsRootLayer(layer);
}

#if DCHECK_IS_ON()
static void ValidateRenderSurfaceForLayer(LayerImpl* layer) {
  // This test verifies that there are no cases where a LayerImpl needs
  // a render surface, but doesn't have one.
  if (layer->has_render_surface())
    return;

  DCHECK(layer->filters().IsEmpty()) << "layer: " << layer->id();
  DCHECK(!IsRootLayer(layer)) << "layer: " << layer->id();
  EffectNode* effect_node =
      layer->layer_tree_impl()->property_trees()->effect_tree.Node(
          layer->effect_tree_index());
  if (effect_node->owner_id != layer->id())
    return;
  DCHECK_EQ(effect_node->data.mask_layer_id, -1) << "layer: " << layer->id();
  DCHECK_EQ(effect_node->data.replica_layer_id, -1) << "layer: " << layer->id();
  DCHECK(effect_node->data.background_filters.IsEmpty());
}

#endif

template <typename LayerType>
bool ComputeClipRectInTargetSpace(const LayerType* layer,
                                  const ClipNode* clip_node,
                                  const TransformTree& transform_tree,
                                  int target_node_id,
                                  gfx::RectF* clip_rect_in_target_space) {
  DCHECK(layer->clip_tree_index() == clip_node->id);
  DCHECK(clip_node->data.target_id != target_node_id);

  gfx::Transform clip_to_target;
  if (clip_node->data.target_id > target_node_id) {
    // In this case, layer has a scroll parent. We need to keep the scale
    // at the layer's target but remove the scale at the scroll parent's
    // target.
    if (transform_tree.ComputeTransformWithDestinationSublayerScale(
            clip_node->data.target_id, target_node_id, &clip_to_target)) {
      const TransformNode* source_node =
          transform_tree.Node(clip_node->data.target_id);
      if (source_node->data.sublayer_scale.x() != 0.f &&
          source_node->data.sublayer_scale.y() != 0.f)
        clip_to_target.Scale(1.0f / source_node->data.sublayer_scale.x(),
                             1.0f / source_node->data.sublayer_scale.y());
      *clip_rect_in_target_space = MathUtil::MapClippedRect(
          clip_to_target, clip_node->data.clip_in_target_space);
    } else {
      return false;
    }
  } else {
    if (transform_tree.ComputeTransform(clip_node->data.target_id,
                                        target_node_id, &clip_to_target)) {
      *clip_rect_in_target_space = MathUtil::ProjectClippedRect(
          clip_to_target, clip_node->data.clip_in_target_space);
    } else {
      return false;
    }
  }
  return true;
}

struct ConditionalClip {
  bool is_clipped;
  gfx::RectF clip_rect;
};

static ConditionalClip ComputeTargetRectInLocalSpace(
    gfx::RectF rect,
    const TransformTree& transform_tree,
    int current_transform_id,
    int target_transform_id) {
  gfx::Transform current_to_target;
  if (!transform_tree.ComputeTransformWithSourceSublayerScale(
          current_transform_id, target_transform_id, &current_to_target))
    // If transform is not invertible, cannot apply clip.
    return ConditionalClip{false, gfx::RectF()};

  if (current_transform_id > target_transform_id)
    return ConditionalClip{true,  // is_clipped.
                           MathUtil::MapClippedRect(current_to_target, rect)};

  return ConditionalClip{true,  // is_clipped.
                         MathUtil::ProjectClippedRect(current_to_target, rect)};
}

static ConditionalClip ComputeLocalRectInTargetSpace(
    gfx::RectF rect,
    const TransformTree& transform_tree,
    int current_transform_id,
    int target_transform_id) {
  gfx::Transform current_to_target;
  if (!transform_tree.ComputeTransformWithDestinationSublayerScale(
          current_transform_id, target_transform_id, &current_to_target))
    // If transform is not invertible, cannot apply clip.
    return ConditionalClip{false, gfx::RectF()};

  if (current_transform_id > target_transform_id)
    return ConditionalClip{true,  // is_clipped.
                           MathUtil::MapClippedRect(current_to_target, rect)};

  return ConditionalClip{true,  // is_clipped.
                         MathUtil::ProjectClippedRect(current_to_target, rect)};
}

static ConditionalClip ComputeCurrentClip(const ClipNode* clip_node,
                                          const TransformTree& transform_tree,
                                          int target_transform_id) {
  if (clip_node->data.transform_id != target_transform_id)
    return ComputeLocalRectInTargetSpace(clip_node->data.clip, transform_tree,
                                         clip_node->data.transform_id,
                                         target_transform_id);

  gfx::RectF current_clip = clip_node->data.clip;
  gfx::Vector2dF sublayer_scale =
      transform_tree.Node(target_transform_id)->data.sublayer_scale;
  if (sublayer_scale.x() > 0 && sublayer_scale.y() > 0)
    current_clip.Scale(sublayer_scale.x(), sublayer_scale.y());
  return ConditionalClip{true /* is_clipped */, current_clip};
}

static ConditionalClip ComputeAccumulatedClip(
    const ClipTree& clip_tree,
    int local_clip_id,
    const EffectTree& effect_tree,
    int target_id,
    const TransformTree& transform_tree) {
  const ClipNode* clip_node = clip_tree.Node(local_clip_id);
  const EffectNode* target_node = effect_tree.Node(target_id);
  int target_transform_id = target_node->data.transform_id;
  bool is_clipped = false;

  // Collect all the clips that need to be accumulated.
  std::stack<const ClipNode*> parent_chain;

  // If target is not direct ancestor of clip, this will find least common
  // ancestor between the target and the clip.
  while (target_node->id >= 0 && clip_node->id >= 0) {
    while (target_node->data.clip_id > clip_node->id ||
           target_node->data.has_unclipped_descendants) {
      target_node = effect_tree.Node(target_node->data.target_id);
    }
    if (target_node->data.clip_id == clip_node->id)
      break;
    while (target_node->data.clip_id < clip_node->id) {
      parent_chain.push(clip_node);
      clip_node = clip_tree.parent(clip_node);
    }
    if (target_node->data.clip_id == clip_node->id) {
      // Target is responsible for applying this clip_node (id equals to
      // target_node's clip id), no need to accumulate this as part of clip
      // rect.
      clip_node = parent_chain.top();
      parent_chain.pop();
      break;
    }
  }

  // TODO(weiliangc): If we don't create clip for render surface, we don't need
  // to check applies_local_clip.
  while (!clip_node->data.applies_local_clip && parent_chain.size() > 0) {
    clip_node = parent_chain.top();
    parent_chain.pop();
  }

  if (!clip_node->data.applies_local_clip)
    // No clip node applying clip in between.
    return ConditionalClip{false, gfx::RectF()};

  ConditionalClip current_clip =
      ComputeCurrentClip(clip_node, transform_tree, target_transform_id);
  is_clipped = current_clip.is_clipped;
  gfx::RectF accumulated_clip = current_clip.clip_rect;

  while (parent_chain.size() > 0) {
    clip_node = parent_chain.top();
    parent_chain.pop();
    if (!clip_node->data.applies_local_clip) {
      continue;
    }
    ConditionalClip current_clip =
        ComputeCurrentClip(clip_node, transform_tree, target_transform_id);

    // If transform is not invertible, no clip will be applied.
    if (!current_clip.is_clipped)
      return ConditionalClip{false, gfx::RectF()};

    is_clipped = true;
    accumulated_clip =
        gfx::IntersectRects(accumulated_clip, current_clip.clip_rect);
  }

  return ConditionalClip{
      is_clipped, accumulated_clip.IsEmpty() ? gfx::RectF() : accumulated_clip};
}

template <typename LayerType>
void CalculateClipRects(
    const typename LayerType::LayerListType& visible_layer_list,
    const ClipTree& clip_tree,
    const TransformTree& transform_tree,
    const EffectTree& effect_tree,
    bool non_root_surfaces_enabled) {
  for (auto& layer : visible_layer_list) {
    const ClipNode* clip_node = clip_tree.Node(layer->clip_tree_index());
    // The entire layer is visible if it has copy requests.
    const EffectNode* effect_node =
        effect_tree.Node(layer->effect_tree_index());
    if (effect_node->data.has_copy_request &&
        effect_node->owner_id == layer->id())
      continue;

    if (!non_root_surfaces_enabled) {
      layer->set_clip_rect(
          gfx::ToEnclosingRect(clip_node->data.clip_in_target_space));
      continue;
    }

    // When both the layer and the target are unclipped, the entire layer
    // content rect is visible.
    const bool fully_visible = !clip_node->data.layers_are_clipped &&
                               !clip_node->data.target_is_clipped;

    if (!fully_visible) {
      const TransformNode* transform_node =
          transform_tree.Node(layer->transform_tree_index());
      int target_node_id = transform_tree.ContentTargetId(transform_node->id);

      // The clip node stores clip rect in its target space.
      gfx::RectF clip_rect_in_target_space =
          clip_node->data.clip_in_target_space;

      // If required, this clip rect should be mapped to the current layer's
      // target space.
      if (clip_node->data.target_id != target_node_id) {
        // In this case, layer has a clip parent or scroll parent (or shares the
        // target with an ancestor layer that has clip parent) and the clip
        // parent's target is different from the layer's target. As the layer's
        // target has unclippped descendants, it is unclippped.
        if (!clip_node->data.layers_are_clipped)
          continue;

        // Compute the clip rect in target space and store it.
        if (!ComputeClipRectInTargetSpace(layer, clip_node, transform_tree,
                                          target_node_id,
                                          &clip_rect_in_target_space))
          continue;
      }

      if (!clip_rect_in_target_space.IsEmpty()) {
        layer->set_clip_rect(gfx::ToEnclosingRect(clip_rect_in_target_space));
      } else {
        layer->set_clip_rect(gfx::Rect());
      }
    }
  }
}

bool GetLayerClipRect(const scoped_refptr<Layer> layer,
                      const ClipNode* clip_node,
                      const TransformTree& transform_tree,
                      int target_node_id,
                      gfx::RectF* clip_rect_in_target_space) {
  return ComputeClipRectInTargetSpace(layer.get(), clip_node, transform_tree,
                                      target_node_id,
                                      clip_rect_in_target_space);
}

bool GetLayerClipRect(const LayerImpl* layer,
                      const ClipNode* clip_node,
                      const TransformTree& transform_tree,
                      int target_node_id,
                      gfx::RectF* clip_rect_in_target_space) {
  // This is equivalent of calling ComputeClipRectInTargetSpace.
  *clip_rect_in_target_space = gfx::RectF(layer->clip_rect());
  return transform_tree.Node(target_node_id)->data.ancestors_are_invertible;
}

template <typename LayerType>
void CalculateVisibleRects(
    const typename LayerType::LayerListType& visible_layer_list,
    const ClipTree& clip_tree,
    const TransformTree& transform_tree,
    const EffectTree& effect_tree,
    bool non_root_surfaces_enabled) {
  for (auto& layer : visible_layer_list) {
    gfx::Size layer_bounds = layer->bounds();

    int effect_ancestor_with_copy_request =
        effect_tree.ClosestAncestorWithCopyRequest(layer->effect_tree_index());
    if (effect_ancestor_with_copy_request > 1) {
      // Non root copy request.
      ConditionalClip accumulated_clip_rect = ComputeAccumulatedClip(
          clip_tree, layer->clip_tree_index(), effect_tree,
          effect_ancestor_with_copy_request, transform_tree);
      if (!accumulated_clip_rect.is_clipped) {
        layer->set_visible_layer_rect(gfx::Rect(layer_bounds));
        continue;
      }

      gfx::RectF accumulated_clip_in_copy_request_space =
          accumulated_clip_rect.clip_rect;

      const EffectNode* copy_request_effect_node =
          effect_tree.Node(effect_ancestor_with_copy_request);
      ConditionalClip clip_in_layer_space = ComputeTargetRectInLocalSpace(
          accumulated_clip_in_copy_request_space, transform_tree,
          copy_request_effect_node->data.transform_id,
          layer->transform_tree_index());

      if (clip_in_layer_space.is_clipped) {
        gfx::RectF clip_rect = clip_in_layer_space.clip_rect;
        clip_rect.Offset(-layer->offset_to_transform_parent());
        gfx::Rect visible_rect = gfx::ToEnclosingRect(clip_rect);
        visible_rect.Intersect(gfx::Rect(layer_bounds));
        layer->set_visible_layer_rect(visible_rect);
      } else {
        layer->set_visible_layer_rect(gfx::Rect(layer_bounds));
      }
      continue;
    }

    const ClipNode* clip_node = clip_tree.Node(layer->clip_tree_index());
    const TransformNode* transform_node =
        transform_tree.Node(layer->transform_tree_index());
    if (!non_root_surfaces_enabled) {
      // When we only have a root surface, the clip node and the layer must
      // necessarily have the same target (the root).
      if (transform_node->data.ancestors_are_invertible) {
        gfx::RectF combined_clip_rect_in_target_space =
            clip_node->data.combined_clip_in_target_space;
        gfx::Transform target_to_content;
        target_to_content.Translate(-layer->offset_to_transform_parent().x(),
                                    -layer->offset_to_transform_parent().y());
        target_to_content.PreconcatTransform(
            transform_tree.FromScreen(transform_node->id));

        gfx::Rect visible_rect =
            gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
                target_to_content, combined_clip_rect_in_target_space));
        visible_rect.Intersect(gfx::Rect(layer_bounds));
        layer->set_visible_layer_rect(visible_rect);
      } else {
        layer->set_visible_layer_rect(gfx::Rect(layer_bounds));
      }
      continue;
    }

    // When both the layer and the target are unclipped, the entire layer
    // content rect is visible.
    const bool fully_visible = !clip_node->data.layers_are_clipped &&
                               !clip_node->data.target_is_clipped;

    if (fully_visible) {
      layer->set_visible_layer_rect(gfx::Rect(layer_bounds));
      continue;
    }

    int target_node_id = transform_tree.ContentTargetId(transform_node->id);

    // The clip node stores clip rect in its target space. If required,
    // this clip rect should be mapped to the current layer's target space.
    gfx::RectF combined_clip_rect_in_target_space;

    if (clip_node->data.target_id != target_node_id) {
      // In this case, layer has a clip parent or scroll parent (or shares the
      // target with an ancestor layer that has clip parent) and the clip
      // parent's target is different from the layer's target. As the layer's
      // target has unclippped descendants, it is unclippped.
      if (!clip_node->data.layers_are_clipped) {
        layer->set_visible_layer_rect(gfx::Rect(layer_bounds));
        continue;
      }

      // We use the clip node's clip_in_target_space (and not
      // combined_clip_in_target_space) here because we want to clip
      // with respect to clip parent's local clip and not its combined clip as
      // the combined clip has even the clip parent's target's clip baked into
      // it and as our target is different, we don't want to use it in our
      // visible rect computation.
      if (!GetLayerClipRect(layer, clip_node, transform_tree, target_node_id,
                            &combined_clip_rect_in_target_space)) {
        layer->set_visible_layer_rect(gfx::Rect(layer_bounds));
        continue;
      }
    } else {
      if (clip_node->data.target_is_clipped) {
        combined_clip_rect_in_target_space =
            clip_node->data.combined_clip_in_target_space;
      } else {
        combined_clip_rect_in_target_space =
            clip_node->data.clip_in_target_space;
      }
    }

    // The clip rect should be intersected with layer rect in target space.
    gfx::Transform content_to_target =
        transform_tree.ToTarget(transform_node->id);
    content_to_target.Translate(layer->offset_to_transform_parent().x(),
                                layer->offset_to_transform_parent().y());
    gfx::Rect layer_content_rect = gfx::Rect(layer_bounds);
    gfx::RectF layer_content_bounds_in_target_space = MathUtil::MapClippedRect(
        content_to_target, gfx::RectF(layer_content_rect));
    // If the layer is fully contained within the clip, treat it as fully
    // visible.
    if (!layer_content_bounds_in_target_space.IsEmpty() &&
        combined_clip_rect_in_target_space.Contains(
            layer_content_bounds_in_target_space)) {
      layer->set_visible_layer_rect(gfx::Rect(layer_bounds));
      continue;
    }

    combined_clip_rect_in_target_space.Intersect(
        layer_content_bounds_in_target_space);
    if (combined_clip_rect_in_target_space.IsEmpty()) {
      layer->set_visible_layer_rect(gfx::Rect());
      continue;
    }

    gfx::Transform target_to_layer;
    if (transform_node->data.ancestors_are_invertible) {
      target_to_layer = transform_tree.FromTarget(transform_node->id);
    } else {
      if (!transform_tree.ComputeTransformWithSourceSublayerScale(
              target_node_id, transform_node->id, &target_to_layer)) {
        // An animated singular transform may become non-singular during the
        // animation, so we still need to compute a visible rect. In this
        // situation, we treat the entire layer as visible.
        layer->set_visible_layer_rect(gfx::Rect(layer_bounds));
        continue;
      }
    }

    gfx::Transform target_to_content;
    target_to_content.Translate(-layer->offset_to_transform_parent().x(),
                                -layer->offset_to_transform_parent().y());
    target_to_content.PreconcatTransform(target_to_layer);

    gfx::Rect visible_rect = gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
        target_to_content, combined_clip_rect_in_target_space));
    visible_rect.Intersect(gfx::Rect(layer_bounds));
    layer->set_visible_layer_rect(visible_rect);
  }
}

static bool HasSingularTransform(int transform_tree_index,
                                 const TransformTree& tree) {
  const TransformNode* node = tree.Node(transform_tree_index);
  return !node->data.is_invertible || !node->data.ancestors_are_invertible;
}

template <typename LayerType>
static int TransformTreeIndexForBackfaceVisibility(LayerType* layer,
                                                   const TransformTree& tree) {
  if (!layer->use_parent_backface_visibility())
    return layer->transform_tree_index();
  const TransformNode* node = tree.Node(layer->transform_tree_index());
  return layer->id() == node->owner_id ? tree.parent(node)->id : node->id;
}

template <typename LayerType>
static bool IsLayerBackFaceVisible(LayerType* layer,
                                   int transform_tree_index,
                                   const TransformTree& tree) {
  const TransformNode* node = tree.Node(transform_tree_index);
  return layer->use_local_transform_for_backface_visibility()
             ? node->data.local.IsBackFaceVisible()
             : tree.ToTarget(transform_tree_index).IsBackFaceVisible();
}

static inline bool TransformToScreenIsKnown(Layer* layer,
                                            int transform_tree_index,
                                            const TransformTree& tree) {
  const TransformNode* node = tree.Node(transform_tree_index);
  return !node->data.to_screen_is_potentially_animated;
}

static inline bool TransformToScreenIsKnown(LayerImpl* layer,
                                            int transform_tree_index,
                                            const TransformTree& tree) {
  return true;
}

template <typename LayerType>
static bool LayerNeedsUpdateInternal(LayerType* layer,
                                     bool layer_is_drawn,
                                     const TransformTree& tree) {
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
  //   - the visible_layer_rect is empty
  //
  // Note, if the layer should not have been drawn due to being fully
  // transparent, we would have skipped the entire subtree and never made it
  // into this function, so it is safe to omit this check here.
  if (!layer_is_drawn)
    return false;

  if (!layer->DrawsContent() || layer->bounds().IsEmpty())
    return false;

  // The layer should not be drawn if (1) it is not double-sided and (2) the
  // back of the layer is known to be facing the screen.
  if (layer->should_check_backface_visibility()) {
    int backface_transform_id =
        TransformTreeIndexForBackfaceVisibility(layer, tree);
    // A layer with singular transform is not drawn. So, we can assume that its
    // backface is not visible.
    if (TransformToScreenIsKnown(layer, backface_transform_id, tree) &&
        !HasSingularTransform(backface_transform_id, tree) &&
        IsLayerBackFaceVisible(layer, backface_transform_id, tree))
      return false;
  }

  return true;
}

void FindLayersThatNeedUpdates(LayerTreeImpl* layer_tree_impl,
                               const TransformTree& transform_tree,
                               const EffectTree& effect_tree,
                               std::vector<LayerImpl*>* visible_layer_list) {
  for (auto* layer_impl : *layer_tree_impl) {
    bool layer_is_drawn =
        effect_tree.Node(layer_impl->effect_tree_index())->data.is_drawn;

    if (!IsRootLayer(layer_impl) &&
        LayerShouldBeSkipped(layer_impl, layer_is_drawn, transform_tree,
                             effect_tree))
      continue;

    if (LayerNeedsUpdate(layer_impl, layer_is_drawn, transform_tree))
      visible_layer_list->push_back(layer_impl);
  }
}

void UpdateRenderSurfaceForLayer(EffectTree* effect_tree,
                                 bool non_root_surfaces_enabled,
                                 LayerImpl* layer) {
  if (!non_root_surfaces_enabled) {
    layer->SetHasRenderSurface(IsRootLayer(layer));
    return;
  }

  EffectNode* node = effect_tree->Node(layer->effect_tree_index());

  if (node->owner_id == layer->id() && node->data.has_render_surface)
    layer->SetHasRenderSurface(true);
  else
    layer->SetHasRenderSurface(false);
}
}  // namespace

template <typename LayerType>
static inline bool LayerShouldBeSkippedInternal(
    LayerType* layer,
    bool layer_is_drawn,
    const TransformTree& transform_tree,
    const EffectTree& effect_tree) {
  const TransformNode* transform_node =
      transform_tree.Node(layer->transform_tree_index());
  const EffectNode* effect_node = effect_tree.Node(layer->effect_tree_index());

  if (effect_node->data.has_render_surface &&
      effect_node->data.num_copy_requests_in_subtree > 0)
    return false;
  // If the layer transform is not invertible, it should be skipped.
  // TODO(ajuma): Correctly process subtrees with singular transform for the
  // case where we may animate to a non-singular transform and wish to
  // pre-raster.
  return !transform_node->data.node_and_ancestors_are_animated_or_invertible ||
         effect_node->data.hidden_by_backface_visibility ||
         !effect_node->data.is_drawn;
}

bool LayerShouldBeSkipped(LayerImpl* layer,
                          bool layer_is_drawn,
                          const TransformTree& transform_tree,
                          const EffectTree& effect_tree) {
  return LayerShouldBeSkippedInternal(layer, layer_is_drawn, transform_tree,
                                      effect_tree);
}

bool LayerShouldBeSkipped(Layer* layer,
                          bool layer_is_drawn,
                          const TransformTree& transform_tree,
                          const EffectTree& effect_tree) {
  return LayerShouldBeSkippedInternal(layer, layer_is_drawn, transform_tree,
                                      effect_tree);
}

void FindLayersThatNeedUpdates(LayerTreeHost* layer_tree_host,
                               const TransformTree& transform_tree,
                               const EffectTree& effect_tree,
                               LayerList* update_layer_list) {
  for (auto* layer : *layer_tree_host) {
    bool layer_is_drawn =
        effect_tree.Node(layer->effect_tree_index())->data.is_drawn;

    if (!IsRootLayer(layer) &&
        LayerShouldBeSkipped(layer, layer_is_drawn, transform_tree,
                             effect_tree))
      continue;

    if (LayerNeedsUpdate(layer, layer_is_drawn, transform_tree)) {
      update_layer_list->push_back(layer);
    }

    // Append mask layers to the update layer list. They don't have valid
    // visible rects, so need to get added after the above calculation.
    // Replica layers don't need to be updated.
    if (Layer* mask_layer = layer->mask_layer())
      update_layer_list->push_back(mask_layer);
    if (Layer* replica_layer = layer->replica_layer()) {
      if (Layer* mask_layer = replica_layer->mask_layer())
        update_layer_list->push_back(mask_layer);
    }
  }
}

static void ResetIfHasNanCoordinate(gfx::RectF* rect) {
  if (std::isnan(rect->x()) || std::isnan(rect->y()) ||
      std::isnan(rect->right()) || std::isnan(rect->bottom()))
    *rect = gfx::RectF();
}

void ComputeClips(ClipTree* clip_tree,
                  const TransformTree& transform_tree,
                  bool non_root_surfaces_enabled) {
  if (!clip_tree->needs_update())
    return;
  for (int i = 1; i < static_cast<int>(clip_tree->size()); ++i) {
    ClipNode* clip_node = clip_tree->Node(i);

    if (clip_node->id == 1) {
      ResetIfHasNanCoordinate(&clip_node->data.clip);
      clip_node->data.clip_in_target_space = clip_node->data.clip;
      clip_node->data.combined_clip_in_target_space = clip_node->data.clip;
      continue;
    }
    const TransformNode* transform_node =
        transform_tree.Node(clip_node->data.transform_id);
    ClipNode* parent_clip_node = clip_tree->parent(clip_node);

    gfx::Transform parent_to_current;
    const TransformNode* parent_target_transform_node =
        transform_tree.Node(parent_clip_node->data.target_id);
    bool success = true;

    // Clips must be combined in target space. We cannot, for example, combine
    // clips in the space of the child clip. The reason is non-affine
    // transforms. Say we have the following tree T->A->B->C, and B clips C, but
    // draw into target T. It may be the case that A applies a perspective
    // transform, and B and C are at different z positions. When projected into
    // target space, the relative sizes and positions of B and C can shift.
    // Since it's the relationship in target space that matters, that's where we
    // must combine clips. For each clip node, we save the clip rects in its
    // target space. So, we need to get the ancestor clip rect in the current
    // clip node's target space.
    gfx::RectF parent_combined_clip_in_target_space =
        parent_clip_node->data.combined_clip_in_target_space;
    gfx::RectF parent_clip_in_target_space =
        parent_clip_node->data.clip_in_target_space;
    if (parent_target_transform_node &&
        parent_target_transform_node->id != clip_node->data.target_id &&
        non_root_surfaces_enabled) {
      success &= transform_tree.ComputeTransformWithDestinationSublayerScale(
          parent_target_transform_node->id, clip_node->data.target_id,
          &parent_to_current);
      if (parent_target_transform_node->data.sublayer_scale.x() > 0 &&
          parent_target_transform_node->data.sublayer_scale.y() > 0)
        parent_to_current.Scale(
            1.f / parent_target_transform_node->data.sublayer_scale.x(),
            1.f / parent_target_transform_node->data.sublayer_scale.y());
      // If we can't compute a transform, it's because we had to use the inverse
      // of a singular transform. We won't draw in this case, so there's no need
      // to compute clips.
      if (!success)
        continue;
      parent_combined_clip_in_target_space = MathUtil::ProjectClippedRect(
          parent_to_current,
          parent_clip_node->data.combined_clip_in_target_space);
      parent_clip_in_target_space = MathUtil::ProjectClippedRect(
          parent_to_current, parent_clip_node->data.clip_in_target_space);
    }
    // Only nodes affected by ancestor clips will have their clip adjusted due
    // to intersecting with an ancestor clip. But, we still need to propagate
    // the combined clip to our children because if they are clipped, they may
    // need to clip using our parent clip and if we don't propagate it here,
    // it will be lost.
    if (clip_node->data.resets_clip && non_root_surfaces_enabled) {
      if (clip_node->data.applies_local_clip) {
        clip_node->data.clip_in_target_space = MathUtil::MapClippedRect(
            transform_tree.ToTarget(clip_node->data.transform_id),
            clip_node->data.clip);
        ResetIfHasNanCoordinate(&clip_node->data.clip_in_target_space);
        clip_node->data.combined_clip_in_target_space =
            gfx::IntersectRects(clip_node->data.clip_in_target_space,
                                parent_combined_clip_in_target_space);
      } else {
        DCHECK(!clip_node->data.target_is_clipped);
        DCHECK(!clip_node->data.layers_are_clipped);
        clip_node->data.combined_clip_in_target_space =
            parent_combined_clip_in_target_space;
      }
      ResetIfHasNanCoordinate(&clip_node->data.combined_clip_in_target_space);
      continue;
    }
    bool use_only_parent_clip = !clip_node->data.applies_local_clip;
    if (use_only_parent_clip) {
      clip_node->data.combined_clip_in_target_space =
          parent_combined_clip_in_target_space;
      if (!non_root_surfaces_enabled) {
        clip_node->data.clip_in_target_space =
            parent_clip_node->data.clip_in_target_space;
      } else if (!clip_node->data.target_is_clipped) {
        clip_node->data.clip_in_target_space = parent_clip_in_target_space;
      } else {
        // Render Surface applies clip and the owning layer itself applies
        // no clip. So, clip_in_target_space is not used and hence we can set
        // it to an empty rect.
        clip_node->data.clip_in_target_space = gfx::RectF();
      }
    } else {
      gfx::Transform source_to_target;

      if (!non_root_surfaces_enabled) {
        source_to_target =
            transform_tree.ToScreen(clip_node->data.transform_id);
      } else if (transform_tree.ContentTargetId(transform_node->id) ==
                 clip_node->data.target_id) {
        source_to_target =
            transform_tree.ToTarget(clip_node->data.transform_id);
      } else {
        success = transform_tree.ComputeTransformWithDestinationSublayerScale(
            transform_node->id, clip_node->data.target_id, &source_to_target);
        // source_to_target computation should be successful as target is an
        // ancestor of the transform node.
        DCHECK(success);
      }

      gfx::RectF source_clip_in_target_space =
          MathUtil::MapClippedRect(source_to_target, clip_node->data.clip);

      // With surfaces disabled, the only case where we use only the local clip
      // for layer clipping is the case where no non-viewport ancestor node
      // applies a local clip.
      bool layer_clipping_uses_only_local_clip =
          non_root_surfaces_enabled
              ? clip_node->data.layer_clipping_uses_only_local_clip
              : !parent_clip_node->data
                     .layers_are_clipped_when_surfaces_disabled;
      if (!layer_clipping_uses_only_local_clip) {
        clip_node->data.clip_in_target_space = gfx::IntersectRects(
            parent_clip_in_target_space, source_clip_in_target_space);
      } else {
        clip_node->data.clip_in_target_space = source_clip_in_target_space;
      }

      clip_node->data.combined_clip_in_target_space = gfx::IntersectRects(
          parent_combined_clip_in_target_space, source_clip_in_target_space);
    }
    ResetIfHasNanCoordinate(&clip_node->data.clip_in_target_space);
    ResetIfHasNanCoordinate(&clip_node->data.combined_clip_in_target_space);
  }
  clip_tree->set_needs_update(false);
}

void ComputeTransforms(TransformTree* transform_tree) {
  if (!transform_tree->needs_update())
    return;
  for (int i = 1; i < static_cast<int>(transform_tree->size()); ++i)
    transform_tree->UpdateTransforms(i);
  transform_tree->set_needs_update(false);
}

void UpdateRenderTarget(EffectTree* effect_tree,
                        bool can_render_to_separate_surface) {
  for (int i = 1; i < static_cast<int>(effect_tree->size()); ++i) {
    EffectNode* node = effect_tree->Node(i);
    if (i == 1) {
      // Render target on the first effect node is root.
      node->data.target_id = 0;
    } else if (!can_render_to_separate_surface) {
      node->data.target_id = 1;
    } else if (effect_tree->parent(node)->data.has_render_surface) {
      node->data.target_id = node->parent_id;
    } else {
      node->data.target_id = effect_tree->parent(node)->data.target_id;
    }
  }
}

void ComputeEffects(EffectTree* effect_tree) {
  if (!effect_tree->needs_update())
    return;
  for (int i = 1; i < static_cast<int>(effect_tree->size()); ++i)
    effect_tree->UpdateEffects(i);
  effect_tree->set_needs_update(false);
}

static void ComputeClipsWithEffectTree(PropertyTrees* property_trees) {
  EffectTree* effect_tree = &property_trees->effect_tree;
  const ClipTree* clip_tree = &property_trees->clip_tree;
  const TransformTree* transform_tree = &property_trees->transform_tree;
  EffectNode* root_effect_node = effect_tree->Node(1);
  const RenderSurfaceImpl* root_render_surface =
      root_effect_node->data.render_surface;
  gfx::Rect root_clip = gfx::ToEnclosingRect(
      clip_tree->Node(root_effect_node->data.clip_id)->data.clip);
  if (root_render_surface->is_clipped())
    DCHECK(root_clip == root_render_surface->clip_rect())
        << "clip on root render surface: "
        << root_render_surface->clip_rect().ToString()
        << " v.s. root effect node's clip: " << root_clip.ToString();
  for (int i = 2; i < static_cast<int>(effect_tree->size()); ++i) {
    EffectNode* effect_node = effect_tree->Node(i);
    const EffectNode* target_node =
        effect_tree->Node(effect_node->data.target_id);
    ConditionalClip accumulated_clip_rect =
        ComputeAccumulatedClip(*clip_tree, effect_node->data.clip_id,
                               *effect_tree, target_node->id, *transform_tree);
    gfx::RectF accumulated_clip = accumulated_clip_rect.clip_rect;
    const RenderSurfaceImpl* render_surface = effect_node->data.render_surface;
    if (render_surface && render_surface->is_clipped()) {
      DCHECK(gfx::ToEnclosingRect(accumulated_clip) ==
             render_surface->clip_rect())
          << " render surface's clip rect: "
          << render_surface->clip_rect().ToString()
          << " v.s. accumulated clip: "
          << gfx::ToEnclosingRect(accumulated_clip).ToString();
    }
  }
}

static void ComputeLayerClipRect(const PropertyTrees* property_trees,
                                 const LayerImpl* layer) {
  const EffectTree* effect_tree = &property_trees->effect_tree;
  const ClipTree* clip_tree = &property_trees->clip_tree;
  const TransformTree* transform_tree = &property_trees->transform_tree;
  const EffectNode* effect_node = effect_tree->Node(layer->effect_tree_index());
  const EffectNode* target_node =
      effect_node->data.has_render_surface
          ? effect_node
          : effect_tree->Node(effect_node->data.target_id);
  // TODO(weiliangc): When effect node has up to date render surface info on
  // compositor thread, no need to check for resourceless draw mode
  if (!property_trees->non_root_surfaces_enabled) {
    target_node = effect_tree->Node(1);
  }

  ConditionalClip accumulated_clip_rect =
      ComputeAccumulatedClip(*clip_tree, layer->clip_tree_index(), *effect_tree,
                             target_node->id, *transform_tree);

  gfx::RectF accumulated_clip = accumulated_clip_rect.clip_rect;

  if ((!property_trees->non_root_surfaces_enabled &&
       clip_tree->Node(layer->clip_tree_index())
           ->data.layers_are_clipped_when_surfaces_disabled) ||
      clip_tree->Node(layer->clip_tree_index())->data.layers_are_clipped) {
    DCHECK(layer->clip_rect() == gfx::ToEnclosingRect(accumulated_clip))
        << " layer: " << layer->id() << " clip id: " << layer->clip_tree_index()
        << " layer clip: " << layer->clip_rect().ToString() << " v.s. "
        << gfx::ToEnclosingRect(accumulated_clip).ToString()
        << " and clip node clip: "
        << gfx::ToEnclosingRect(clip_tree->Node(layer->clip_tree_index())
                                    ->data.clip_in_target_space)
               .ToString();
  }
}

static void ComputeVisibleRectsInternal(
    LayerImpl* root_layer,
    PropertyTrees* property_trees,
    bool can_render_to_separate_surface,
    std::vector<LayerImpl*>* visible_layer_list) {
  if (property_trees->non_root_surfaces_enabled !=
      can_render_to_separate_surface) {
    property_trees->non_root_surfaces_enabled = can_render_to_separate_surface;
    property_trees->transform_tree.set_needs_update(true);
  }
  if (property_trees->transform_tree.needs_update()) {
    property_trees->clip_tree.set_needs_update(true);
    property_trees->effect_tree.set_needs_update(true);
  }
  UpdateRenderTarget(&property_trees->effect_tree,
                     property_trees->non_root_surfaces_enabled);
  ComputeTransforms(&property_trees->transform_tree);
  ComputeClips(&property_trees->clip_tree, property_trees->transform_tree,
               can_render_to_separate_surface);
  ComputeEffects(&property_trees->effect_tree);

  FindLayersThatNeedUpdates(root_layer->layer_tree_impl(),
                            property_trees->transform_tree,
                            property_trees->effect_tree, visible_layer_list);
  CalculateClipRects<LayerImpl>(*visible_layer_list, property_trees->clip_tree,
                                property_trees->transform_tree,
                                property_trees->effect_tree,
                                can_render_to_separate_surface);
  CalculateVisibleRects<LayerImpl>(
      *visible_layer_list, property_trees->clip_tree,
      property_trees->transform_tree, property_trees->effect_tree,
      can_render_to_separate_surface);
}

void UpdatePropertyTrees(PropertyTrees* property_trees,
                         bool can_render_to_separate_surface) {
  if (property_trees->non_root_surfaces_enabled !=
      can_render_to_separate_surface) {
    property_trees->non_root_surfaces_enabled = can_render_to_separate_surface;
    property_trees->transform_tree.set_needs_update(true);
  }
  if (property_trees->transform_tree.needs_update()) {
    property_trees->clip_tree.set_needs_update(true);
    property_trees->effect_tree.set_needs_update(true);
  }
  ComputeTransforms(&property_trees->transform_tree);
  ComputeClips(&property_trees->clip_tree, property_trees->transform_tree,
               can_render_to_separate_surface);
  ComputeEffects(&property_trees->effect_tree);
}

void ComputeVisibleRectsForTesting(PropertyTrees* property_trees,
                                   bool can_render_to_separate_surface,
                                   LayerList* update_layer_list) {
  CalculateVisibleRects<Layer>(*update_layer_list, property_trees->clip_tree,
                               property_trees->transform_tree,
                               property_trees->effect_tree,
                               can_render_to_separate_surface);
}

void BuildPropertyTreesAndComputeVisibleRects(
    LayerImpl* root_layer,
    const LayerImpl* page_scale_layer,
    const LayerImpl* inner_viewport_scroll_layer,
    const LayerImpl* outer_viewport_scroll_layer,
    const LayerImpl* overscroll_elasticity_layer,
    const gfx::Vector2dF& elastic_overscroll,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    bool can_render_to_separate_surface,
    PropertyTrees* property_trees,
    LayerImplList* visible_layer_list) {
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_layer,
      elastic_overscroll, page_scale_factor, device_scale_factor, viewport,
      device_transform, property_trees);
  ComputeVisibleRects(root_layer, property_trees,
                      can_render_to_separate_surface, visible_layer_list);
}

void VerifyClipTreeCalculations(const LayerImplList& layer_list,
                                PropertyTrees* property_trees) {
  if (property_trees->non_root_surfaces_enabled) {
    ComputeClipsWithEffectTree(property_trees);
  }
  for (auto layer : layer_list)
    ComputeLayerClipRect(property_trees, layer);
}

void ComputeVisibleRects(LayerImpl* root_layer,
                         PropertyTrees* property_trees,
                         bool can_render_to_separate_surface,
                         LayerImplList* visible_layer_list) {
  for (auto* layer : *root_layer->layer_tree_impl()) {
    UpdateRenderSurfaceForLayer(&property_trees->effect_tree,
                                can_render_to_separate_surface, layer);
    EffectNode* node =
        property_trees->effect_tree.Node(layer->effect_tree_index());
    if (node->owner_id == layer->id())
      node->data.render_surface = layer->render_surface();
#if DCHECK_IS_ON()
    if (can_render_to_separate_surface)
      ValidateRenderSurfaceForLayer(layer);
#endif
  }
  ComputeVisibleRectsInternal(root_layer, property_trees,
                              can_render_to_separate_surface,
                              visible_layer_list);
}

bool LayerNeedsUpdate(Layer* layer,
                      bool layer_is_drawn,
                      const TransformTree& tree) {
  return LayerNeedsUpdateInternal(layer, layer_is_drawn, tree);
}

bool LayerNeedsUpdate(LayerImpl* layer,
                      bool layer_is_drawn,
                      const TransformTree& tree) {
  return LayerNeedsUpdateInternal(layer, layer_is_drawn, tree);
}

gfx::Transform DrawTransform(const LayerImpl* layer,
                             const TransformTree& tree) {
  const TransformNode* node = tree.Node(layer->transform_tree_index());
  gfx::Transform xform;
  const bool owns_non_root_surface =
      !IsRootLayer(layer) && layer->has_render_surface();
  if (!owns_non_root_surface) {
    // If you're not the root, or you don't own a surface, you need to apply
    // your local offset.
    xform = tree.ToTarget(layer->transform_tree_index());
    if (layer->should_flatten_transform_from_property_tree())
      xform.FlattenTo2d();
    xform.Translate(layer->offset_to_transform_parent().x(),
                    layer->offset_to_transform_parent().y());
  } else {
    // Surfaces need to apply their sublayer scale.
    xform.Scale(node->data.sublayer_scale.x(), node->data.sublayer_scale.y());
  }
  return xform;
}

static void SetSurfaceDrawTransform(const TransformTree& tree,
                                    RenderSurfaceImpl* render_surface) {
  const TransformNode* node = tree.Node(render_surface->TransformTreeIndex());
  // The draw transform of root render surface is identity tranform.
  if (node->id == 1) {
    render_surface->SetDrawTransform(gfx::Transform());
    return;
  }

  gfx::Transform render_surface_transform;
  const TransformNode* target_node = tree.Node(tree.TargetId(node->id));
  tree.ComputeTransformWithDestinationSublayerScale(node->id, target_node->id,
                                                    &render_surface_transform);
  if (node->data.sublayer_scale.x() != 0.0 &&
      node->data.sublayer_scale.y() != 0.0)
    render_surface_transform.Scale(1.0 / node->data.sublayer_scale.x(),
                                   1.0 / node->data.sublayer_scale.y());
  render_surface->SetDrawTransform(render_surface_transform);
}

static void SetSurfaceIsClipped(const ClipNode* clip_node,
                                RenderSurfaceImpl* render_surface) {
  DCHECK(render_surface->OwningLayerId() == clip_node->owner_id)
      << "we now create clip node for every render surface";

  render_surface->SetIsClipped(clip_node->data.target_is_clipped);
}

static void SetSurfaceClipRect(const ClipNode* parent_clip_node,
                               const TransformTree& transform_tree,
                               RenderSurfaceImpl* render_surface) {
  if (!render_surface->is_clipped()) {
    render_surface->SetClipRect(gfx::Rect());
    return;
  }

  const TransformNode* transform_node =
      transform_tree.Node(render_surface->TransformTreeIndex());
  if (transform_tree.TargetId(transform_node->id) ==
      parent_clip_node->data.target_id) {
    render_surface->SetClipRect(
        gfx::ToEnclosingRect(parent_clip_node->data.clip_in_target_space));
    return;
  }

  // In this case, the clip child has reset the clip node for subtree and hence
  // the parent clip node's clip rect is in clip parent's target space and not
  // our target space. We need to transform it to our target space.
  gfx::Transform clip_parent_target_to_target;
  const bool success =
      transform_tree.ComputeTransformWithDestinationSublayerScale(
          parent_clip_node->data.target_id,
          transform_tree.TargetId(transform_node->id),
          &clip_parent_target_to_target);

  if (!success) {
    render_surface->SetClipRect(gfx::Rect());
    return;
  }

  DCHECK_LT(parent_clip_node->data.target_id,
            transform_tree.TargetId(transform_node->id));
  render_surface->SetClipRect(gfx::ToEnclosingRect(MathUtil::ProjectClippedRect(
      clip_parent_target_to_target,
      parent_clip_node->data.clip_in_target_space)));
}

template <typename LayerType>
static gfx::Transform ScreenSpaceTransformInternal(LayerType* layer,
                                                   const TransformTree& tree) {
  gfx::Transform xform(1, 0, 0, 1, layer->offset_to_transform_parent().x(),
                       layer->offset_to_transform_parent().y());
  gfx::Transform ssxform = tree.ToScreen(layer->transform_tree_index());
  xform.ConcatTransform(ssxform);
  if (layer->should_flatten_transform_from_property_tree())
    xform.FlattenTo2d();
  return xform;
}

gfx::Transform ScreenSpaceTransform(const Layer* layer,
                                    const TransformTree& tree) {
  return ScreenSpaceTransformInternal(layer, tree);
}

gfx::Transform ScreenSpaceTransform(const LayerImpl* layer,
                                    const TransformTree& tree) {
  return ScreenSpaceTransformInternal(layer, tree);
}

static float LayerDrawOpacity(const LayerImpl* layer, const EffectTree& tree) {
  if (!layer->render_target())
    return 0.f;

  const EffectNode* target_node =
      tree.Node(layer->render_target()->EffectTreeIndex());
  const EffectNode* node = tree.Node(layer->effect_tree_index());
  if (node == target_node)
    return 1.f;

  float draw_opacity = 1.f;
  while (node != target_node) {
    draw_opacity *= tree.EffectiveOpacity(node);
    node = tree.parent(node);
  }
  return draw_opacity;
}

static void SetSurfaceDrawOpacity(const EffectTree& tree,
                                  RenderSurfaceImpl* render_surface) {
  // Draw opacity of a surface is the product of opacities between the surface
  // (included) and its target surface (excluded).
  const EffectNode* node = tree.Node(render_surface->EffectTreeIndex());
  float draw_opacity = tree.EffectiveOpacity(node);
  for (node = tree.parent(node); node && !node->data.has_render_surface;
       node = tree.parent(node)) {
    draw_opacity *= tree.EffectiveOpacity(node);
  }
  render_surface->SetDrawOpacity(draw_opacity);
}

static gfx::Rect LayerDrawableContentRect(
    const LayerImpl* layer,
    const gfx::Rect& layer_bounds_in_target_space,
    const gfx::Rect& clip_rect) {
  if (layer->is_clipped())
    return IntersectRects(layer_bounds_in_target_space, clip_rect);

  return layer_bounds_in_target_space;
}

static gfx::Transform ReplicaToSurfaceTransform(
    const RenderSurfaceImpl* render_surface,
    const TransformTree& tree) {
  gfx::Transform replica_to_surface;
  if (!render_surface->HasReplica())
    return replica_to_surface;
  const LayerImpl* replica_layer = render_surface->ReplicaLayer();
  const TransformNode* surface_transform_node =
      tree.Node(render_surface->TransformTreeIndex());
  replica_to_surface.Scale(surface_transform_node->data.sublayer_scale.x(),
                           surface_transform_node->data.sublayer_scale.y());
  replica_to_surface.Translate(replica_layer->offset_to_transform_parent().x(),
                               replica_layer->offset_to_transform_parent().y());
  gfx::Transform replica_transform_node_to_surface;
  tree.ComputeTransform(replica_layer->transform_tree_index(),
                        render_surface->TransformTreeIndex(),
                        &replica_transform_node_to_surface);
  replica_to_surface.PreconcatTransform(replica_transform_node_to_surface);
  if (surface_transform_node->data.sublayer_scale.x() != 0 &&
      surface_transform_node->data.sublayer_scale.y() != 0) {
    replica_to_surface.Scale(
        1.0 / surface_transform_node->data.sublayer_scale.x(),
        1.0 / surface_transform_node->data.sublayer_scale.y());
  }
  return replica_to_surface;
}

void ComputeLayerDrawProperties(LayerImpl* layer,
                                const PropertyTrees* property_trees) {
  const TransformNode* transform_node =
      property_trees->transform_tree.Node(layer->transform_tree_index());
  const ClipNode* clip_node =
      property_trees->clip_tree.Node(layer->clip_tree_index());

  layer->draw_properties().screen_space_transform =
      ScreenSpaceTransformInternal(layer, property_trees->transform_tree);
  if (property_trees->non_root_surfaces_enabled) {
    layer->draw_properties().target_space_transform =
        DrawTransform(layer, property_trees->transform_tree);
  } else {
    layer->draw_properties().target_space_transform =
        layer->draw_properties().screen_space_transform;
  }
  layer->draw_properties().screen_space_transform_is_animating =
      transform_node->data.to_screen_is_potentially_animated;

  layer->draw_properties().opacity =
      LayerDrawOpacity(layer, property_trees->effect_tree);
  if (property_trees->non_root_surfaces_enabled) {
    layer->draw_properties().is_clipped = clip_node->data.layers_are_clipped;
  } else {
    layer->draw_properties().is_clipped =
        clip_node->data.layers_are_clipped_when_surfaces_disabled;
  }

  gfx::Rect bounds_in_target_space = MathUtil::MapEnclosingClippedRect(
      layer->draw_properties().target_space_transform,
      gfx::Rect(layer->bounds()));
  layer->draw_properties().drawable_content_rect = LayerDrawableContentRect(
      layer, bounds_in_target_space, layer->draw_properties().clip_rect);
}

void ComputeMaskDrawProperties(LayerImpl* mask_layer,
                               const PropertyTrees* property_trees) {
  // Mask draw properties are used only for rastering, so most of the draw
  // properties computed for other layers are not needed.
  mask_layer->draw_properties().screen_space_transform =
      ScreenSpaceTransformInternal(mask_layer,
                                   property_trees->transform_tree);
  mask_layer->draw_properties().visible_layer_rect =
      gfx::Rect(mask_layer->bounds());
}

void ComputeSurfaceDrawProperties(const PropertyTrees* property_trees,
                                  RenderSurfaceImpl* render_surface) {
  const ClipNode* clip_node =
      property_trees->clip_tree.Node(render_surface->ClipTreeIndex());

  SetSurfaceIsClipped(clip_node, render_surface);
  SetSurfaceDrawOpacity(property_trees->effect_tree, render_surface);
  SetSurfaceDrawTransform(property_trees->transform_tree, render_surface);
  render_surface->SetScreenSpaceTransform(
      property_trees->transform_tree.ToScreenSpaceTransformWithoutSublayerScale(
          render_surface->TransformTreeIndex()));

  if (render_surface->HasReplica()) {
    gfx::Transform replica_to_surface = ReplicaToSurfaceTransform(
        render_surface, property_trees->transform_tree);
    render_surface->SetReplicaDrawTransform(render_surface->draw_transform() *
                                            replica_to_surface);
    render_surface->SetReplicaScreenSpaceTransform(
        render_surface->screen_space_transform() * replica_to_surface);
  } else {
    render_surface->SetReplicaDrawTransform(gfx::Transform());
    render_surface->SetReplicaScreenSpaceTransform(gfx::Transform());
  }

  SetSurfaceClipRect(property_trees->clip_tree.parent(clip_node),
                     property_trees->transform_tree, render_surface);
}

#if DCHECK_IS_ON()
static void ValidatePageScaleLayer(const Layer* page_scale_layer) {
  DCHECK_EQ(page_scale_layer->position().ToString(), gfx::PointF().ToString());
  DCHECK_EQ(page_scale_layer->transform_origin().ToString(),
            gfx::Point3F().ToString());
}

static void ValidatePageScaleLayer(const LayerImpl* page_scale_layer) {}
#endif

template <typename LayerType>
static void UpdatePageScaleFactorInternal(PropertyTrees* property_trees,
                                          const LayerType* page_scale_layer,
                                          float page_scale_factor,
                                          float device_scale_factor,
                                          gfx::Transform device_transform) {
  if (property_trees->transform_tree.page_scale_factor() == page_scale_factor)
    return;

  property_trees->transform_tree.set_page_scale_factor(page_scale_factor);
  DCHECK(page_scale_layer);
  DCHECK_GE(page_scale_layer->transform_tree_index(), 0);
  TransformNode* node = property_trees->transform_tree.Node(
      page_scale_layer->transform_tree_index());
  // TODO(enne): property trees can't ask the layer these things, but
  // the page scale layer should *just* be the page scale.
#if DCHECK_IS_ON()
  ValidatePageScaleLayer(page_scale_layer);
#endif

  if (IsRootLayer(page_scale_layer)) {
    // When the page scale layer is also the root layer, the node should also
    // store the combined scale factor and not just the page scale factor.
    float post_local_scale_factor = page_scale_factor * device_scale_factor;
    node->data.post_local_scale_factor = post_local_scale_factor;
    node->data.post_local = device_transform;
    node->data.post_local.Scale(post_local_scale_factor,
                                post_local_scale_factor);
  } else {
    node->data.post_local_scale_factor = page_scale_factor;
    node->data.update_post_local_transform(gfx::PointF(), gfx::Point3F());
  }
  node->data.needs_local_transform_update = true;
  property_trees->transform_tree.set_needs_update(true);
}

void UpdatePageScaleFactor(PropertyTrees* property_trees,
                           const LayerImpl* page_scale_layer,
                           float page_scale_factor,
                           float device_scale_factor,
                           const gfx::Transform device_transform) {
  UpdatePageScaleFactorInternal(property_trees, page_scale_layer,
                                page_scale_factor, device_scale_factor,
                                device_transform);
}

void UpdatePageScaleFactor(PropertyTrees* property_trees,
                           const Layer* page_scale_layer,
                           float page_scale_factor,
                           float device_scale_factor,
                           const gfx::Transform device_transform) {
  UpdatePageScaleFactorInternal(property_trees, page_scale_layer,
                                page_scale_factor, device_scale_factor,
                                device_transform);
}

template <typename LayerType>
static void UpdateElasticOverscrollInternal(
    PropertyTrees* property_trees,
    const LayerType* overscroll_elasticity_layer,
    const gfx::Vector2dF& elastic_overscroll) {
  if (!overscroll_elasticity_layer) {
    DCHECK(elastic_overscroll.IsZero());
    return;
  }

  TransformNode* node = property_trees->transform_tree.Node(
      overscroll_elasticity_layer->transform_tree_index());
  if (node->data.scroll_offset == gfx::ScrollOffset(elastic_overscroll))
    return;

  node->data.scroll_offset = gfx::ScrollOffset(elastic_overscroll);
  node->data.needs_local_transform_update = true;
  property_trees->transform_tree.set_needs_update(true);
}

void UpdateElasticOverscroll(PropertyTrees* property_trees,
                             const LayerImpl* overscroll_elasticity_layer,
                             const gfx::Vector2dF& elastic_overscroll) {
  UpdateElasticOverscrollInternal(property_trees, overscroll_elasticity_layer,
                                  elastic_overscroll);
}

void UpdateElasticOverscroll(PropertyTrees* property_trees,
                             const Layer* overscroll_elasticity_layer,
                             const gfx::Vector2dF& elastic_overscroll) {
  UpdateElasticOverscrollInternal(property_trees, overscroll_elasticity_layer,
                                  elastic_overscroll);
}

}  // namespace draw_property_utils

}  // namespace cc
