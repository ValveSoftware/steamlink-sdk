// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree_builder.h"

#include <stddef.h>

#include <map>
#include <set>

#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

class LayerTreeHost;

namespace {

static const int kInvalidPropertyTreeNodeId = -1;
static const int kRootPropertyTreeNodeId = 0;
static const int kViewportClipTreeNodeId = 1;

template <typename LayerType>
struct DataForRecursion {
  PropertyTrees* property_trees;
  LayerType* transform_tree_parent;
  LayerType* transform_fixed_parent;
  int render_target;
  int clip_tree_parent;
  int effect_tree_parent;
  int scroll_tree_parent;
  const LayerType* page_scale_layer;
  const LayerType* inner_viewport_scroll_layer;
  const LayerType* outer_viewport_scroll_layer;
  const LayerType* overscroll_elasticity_layer;
  gfx::Vector2dF elastic_overscroll;
  float page_scale_factor;
  bool in_subtree_of_page_scale_layer;
  bool affected_by_inner_viewport_bounds_delta;
  bool affected_by_outer_viewport_bounds_delta;
  bool should_flatten;
  bool target_is_clipped;
  bool is_hidden;
  uint32_t main_thread_scrolling_reasons;
  bool scroll_tree_parent_created_by_uninheritable_criteria;
  const gfx::Transform* device_transform;
  gfx::Vector2dF scroll_snap;
  gfx::Transform compound_transform_since_render_target;
  bool axis_align_since_render_target;
  SkColor safe_opaque_background_color;
};

template <typename LayerType>
struct DataForRecursionFromChild {
  int num_copy_requests_in_subtree;

  DataForRecursionFromChild() : num_copy_requests_in_subtree(0) {}

  void Merge(const DataForRecursionFromChild& data) {
    num_copy_requests_in_subtree += data.num_copy_requests_in_subtree;
  }
};

static LayerPositionConstraint PositionConstraint(Layer* layer) {
  return layer->position_constraint();
}

static LayerPositionConstraint PositionConstraint(LayerImpl* layer) {
  return layer->test_properties()->position_constraint;
}

struct PreCalculateMetaInformationRecursiveData {
  size_t num_unclipped_descendants;
  int num_descendants_that_draw_content;

  PreCalculateMetaInformationRecursiveData()
      : num_unclipped_descendants(0),
        num_descendants_that_draw_content(0) {}

  void Merge(const PreCalculateMetaInformationRecursiveData& data) {
    num_unclipped_descendants += data.num_unclipped_descendants;
    num_descendants_that_draw_content += data.num_descendants_that_draw_content;
  }
};

static inline bool IsRootLayer(const Layer* layer) {
  return !layer->parent();
}

static bool IsMetaInformationRecomputationNeeded(Layer* layer) {
  return layer->layer_tree_host()->needs_meta_info_recomputation();
}

// Recursively walks the layer tree(if needed) to compute any information
// that is needed before doing the main recursion.
static void PreCalculateMetaInformationInternal(
    Layer* layer,
    PreCalculateMetaInformationRecursiveData* recursive_data) {
  if (!IsMetaInformationRecomputationNeeded(layer)) {
    DCHECK(IsRootLayer(layer));
    return;
  }

  if (layer->clip_parent())
    recursive_data->num_unclipped_descendants++;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    Layer* child_layer = layer->child_at(i);

    PreCalculateMetaInformationRecursiveData data_for_child;
    PreCalculateMetaInformationInternal(child_layer, &data_for_child);
    recursive_data->Merge(data_for_child);
  }

  if (layer->clip_children()) {
    size_t num_clip_children = layer->clip_children()->size();
    DCHECK_GE(recursive_data->num_unclipped_descendants, num_clip_children);
    recursive_data->num_unclipped_descendants -= num_clip_children;
  }

  layer->set_num_unclipped_descendants(
      recursive_data->num_unclipped_descendants);

  if (IsRootLayer(layer))
    layer->layer_tree_host()->SetNeedsMetaInfoRecomputation(false);
}

static void PreCalculateMetaInformationInternalForTesting(
    LayerImpl* layer,
    PreCalculateMetaInformationRecursiveData* recursive_data) {
  if (layer->test_properties()->clip_parent)
    recursive_data->num_unclipped_descendants++;

  for (size_t i = 0; i < layer->test_properties()->children.size(); ++i) {
    LayerImpl* child_layer = layer->test_properties()->children[i];

    PreCalculateMetaInformationRecursiveData data_for_child;
    PreCalculateMetaInformationInternalForTesting(child_layer, &data_for_child);
    recursive_data->Merge(data_for_child);
  }

  if (layer->test_properties()->clip_children) {
    size_t num_clip_children = layer->test_properties()->clip_children->size();
    DCHECK_GE(recursive_data->num_unclipped_descendants, num_clip_children);
    recursive_data->num_unclipped_descendants -= num_clip_children;
  }

  layer->test_properties()->num_unclipped_descendants =
      recursive_data->num_unclipped_descendants;
  // TODO(enne): this should be synced from the main thread, so is only
  // for tests constructing layers on the compositor thread.
  layer->test_properties()->num_descendants_that_draw_content =
      recursive_data->num_descendants_that_draw_content;

  if (layer->DrawsContent())
    recursive_data->num_descendants_that_draw_content++;
}

static LayerImplList& Children(LayerImpl* layer) {
  return layer->test_properties()->children;
}

static const LayerList& Children(Layer* layer) {
  return layer->children();
}

static LayerImpl* ChildAt(LayerImpl* layer, int index) {
  return layer->test_properties()->children[index];
}

static Layer* ChildAt(Layer* layer, int index) {
  return layer->child_at(index);
}

static Layer* ScrollParent(Layer* layer) {
  return layer->scroll_parent();
}

static LayerImpl* ScrollParent(LayerImpl* layer) {
  return layer->test_properties()->scroll_parent;
}

static std::set<Layer*>* ScrollChildren(Layer* layer) {
  return layer->scroll_children();
}

static std::set<LayerImpl*>* ScrollChildren(LayerImpl* layer) {
  return layer->test_properties()->scroll_children.get();
}

static Layer* ClipParent(Layer* layer) {
  return layer->clip_parent();
}

static LayerImpl* ClipParent(LayerImpl* layer) {
  return layer->test_properties()->clip_parent;
}

static size_t NumUnclippedDescendants(Layer* layer) {
  return layer->num_unclipped_descendants();
}

static size_t NumUnclippedDescendants(LayerImpl* layer) {
  return layer->test_properties()->num_unclipped_descendants;
}

static Layer* MaskLayer(Layer* layer) {
  return layer->mask_layer();
}

static LayerImpl* MaskLayer(LayerImpl* layer) {
  return layer->test_properties()->mask_layer;
}

static Layer* ReplicaLayer(Layer* layer) {
  return layer->replica_layer();
}

static LayerImpl* ReplicaLayer(LayerImpl* layer) {
  return layer->test_properties()->replica_layer;
}

template <typename LayerType>
static LayerType* GetTransformParent(const DataForRecursion<LayerType>& data,
                                     LayerType* layer) {
  return PositionConstraint(layer).is_fixed_position()
             ? data.transform_fixed_parent
             : data.transform_tree_parent;
}

template <typename LayerType>
static ClipNode* GetClipParent(const DataForRecursion<LayerType>& data,
                               LayerType* layer) {
  const bool inherits_clip = !ClipParent(layer);
  const int id = inherits_clip ? data.clip_tree_parent
                               : ClipParent(layer)->clip_tree_index();
  return data.property_trees->clip_tree.Node(id);
}

template <typename LayerType>
static bool LayerClipsSubtree(LayerType* layer) {
  return layer->masks_to_bounds() || MaskLayer(layer);
}

template <typename LayerType>
static int GetScrollParentId(const DataForRecursion<LayerType>& data,
                             LayerType* layer) {
  const bool inherits_scroll = !ScrollParent(layer);
  const int id = inherits_scroll ? data.scroll_tree_parent
                                 : ScrollParent(layer)->scroll_tree_index();
  return id;
}

static Layer* Parent(Layer* layer) {
  return layer->parent();
}

static LayerImpl* Parent(LayerImpl* layer) {
  return layer->test_properties()->parent;
}

template <typename LayerType>
void AddClipNodeIfNeeded(const DataForRecursion<LayerType>& data_from_ancestor,
                         LayerType* layer,
                         bool created_render_surface,
                         bool created_transform_node,
                         DataForRecursion<LayerType>* data_for_children) {
  ClipNode* parent = GetClipParent(data_from_ancestor, layer);
  int parent_id = parent->id;

  bool is_root = !Parent(layer);

  // Whether we have an ancestor clip that we might need to apply.
  bool ancestor_clips_subtree = is_root || parent->data.layers_are_clipped;

  bool layers_are_clipped = false;
  bool has_unclipped_surface = false;

  if (created_render_surface) {
    // Clips can usually be applied to a surface's descendants simply by
    // clipping the surface (or applied implicitly by the surface's bounds).
    // However, if the surface has unclipped descendants (layers that aren't
    // affected by the ancestor clip), we cannot clip the surface itself, and
    // must instead apply clips to the clipped descendants.
    if (ancestor_clips_subtree && NumUnclippedDescendants(layer) > 0) {
      layers_are_clipped = true;
    } else if (!ancestor_clips_subtree) {
      // When there are no ancestor clips that need to be applied to a render
      // surface, we reset clipping state. The surface might contribute a clip
      // of its own, but clips from ancestor nodes don't need to be considered
      // when computing clip rects or visibility.
      has_unclipped_surface = true;
      DCHECK(!parent->data.applies_local_clip);
    }
    // A surface with unclipped descendants cannot be clipped by its ancestor
    // clip at draw time since the unclipped descendants aren't affected by the
    // ancestor clip.
    data_for_children->target_is_clipped =
        ancestor_clips_subtree && !NumUnclippedDescendants(layer);
  } else {
    // Without a new render surface, layer clipping state from ancestors needs
    // to continue to propagate.
    data_for_children->target_is_clipped = data_from_ancestor.target_is_clipped;
    layers_are_clipped = ancestor_clips_subtree;
  }

  bool layer_clips_subtree = LayerClipsSubtree(layer);
  if (layer_clips_subtree)
    layers_are_clipped = true;

  // Without surfaces, all non-viewport clips have to be applied using layer
  // clipping.
  bool layers_are_clipped_when_surfaces_disabled =
      layer_clips_subtree ||
      parent->data.layers_are_clipped_when_surfaces_disabled;

  // Render surface's clip is needed during hit testing. So, we need to create
  // a clip node for every render surface.
  bool requires_node = layer_clips_subtree || created_render_surface;

  if (!requires_node) {
    data_for_children->clip_tree_parent = parent_id;
    DCHECK_EQ(layers_are_clipped, parent->data.layers_are_clipped);
    DCHECK_EQ(layers_are_clipped_when_surfaces_disabled,
              parent->data.layers_are_clipped_when_surfaces_disabled);
  } else {
    LayerType* transform_parent = data_for_children->transform_tree_parent;
    if (PositionConstraint(layer).is_fixed_position() &&
        !created_transform_node) {
      transform_parent = data_for_children->transform_fixed_parent;
    }
    ClipNode node;
    node.data.clip =
        gfx::RectF(gfx::PointF() + layer->offset_to_transform_parent(),
                   gfx::SizeF(layer->bounds()));
    node.data.transform_id = transform_parent->transform_tree_index();
    node.data.target_id = data_for_children->property_trees->effect_tree
                              .Node(data_for_children->render_target)
                              ->data.transform_id;
    node.owner_id = layer->id();

    if (ancestor_clips_subtree || layer_clips_subtree) {
      // Surfaces reset the rect used for layer clipping. At other nodes, layer
      // clipping state from ancestors must continue to get propagated.
      node.data.layer_clipping_uses_only_local_clip =
          (created_render_surface && NumUnclippedDescendants(layer) == 0) ||
          !ancestor_clips_subtree;
    } else {
      // Otherwise, we're either unclipped, or exist only in order to apply our
      // parent's clips in our space.
      node.data.layer_clipping_uses_only_local_clip = false;
    }

    node.data.applies_local_clip = layer_clips_subtree;
    node.data.resets_clip = has_unclipped_surface;
    node.data.target_is_clipped = data_for_children->target_is_clipped;
    node.data.layers_are_clipped = layers_are_clipped;
    node.data.layers_are_clipped_when_surfaces_disabled =
        layers_are_clipped_when_surfaces_disabled;

    data_for_children->clip_tree_parent =
        data_for_children->property_trees->clip_tree.Insert(node, parent_id);
    data_for_children->property_trees->clip_id_to_index_map[layer->id()] =
        data_for_children->clip_tree_parent;
  }

  layer->SetClipTreeIndex(data_for_children->clip_tree_parent);
  // TODO(awoloszyn): Right now when we hit a node with a replica, we reset the
  // clip for all children since we may need to draw. We need to figure out a
  // better way, since we will need both the clipped and unclipped versions.
}

template <typename LayerType>
static inline bool IsAtBoundaryOf3dRenderingContext(LayerType* layer) {
  return Parent(layer)
             ? Parent(layer)->sorting_context_id() !=
                   layer->sorting_context_id()
             : layer->Is3dSorted();
}

static inline gfx::Point3F TransformOrigin(Layer* layer) {
  return layer->transform_origin();
}

static inline gfx::Point3F TransformOrigin(LayerImpl* layer) {
  return layer->test_properties()->transform_origin;
}

static inline bool IsContainerForFixedPositionLayers(Layer* layer) {
  return layer->IsContainerForFixedPositionLayers();
}

static inline bool IsContainerForFixedPositionLayers(LayerImpl* layer) {
  return layer->test_properties()->is_container_for_fixed_position_layers;
}

static inline bool ShouldFlattenTransform(Layer* layer) {
  return layer->should_flatten_transform();
}

static inline bool ShouldFlattenTransform(LayerImpl* layer) {
  return layer->test_properties()->should_flatten_transform;
}

template <typename LayerType>
bool AddTransformNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    bool created_render_surface,
    DataForRecursion<LayerType>* data_for_children) {
  const bool is_root = !Parent(layer);
  const bool is_page_scale_layer = layer == data_from_ancestor.page_scale_layer;
  const bool is_overscroll_elasticity_layer =
      layer == data_from_ancestor.overscroll_elasticity_layer;
  const bool is_scrollable = layer->scrollable();
  const bool is_fixed = PositionConstraint(layer).is_fixed_position();

  const bool has_significant_transform =
      !layer->transform().IsIdentityOr2DTranslation();

  const bool has_potentially_animated_transform =
      layer->HasPotentiallyRunningTransformAnimation();

  // A transform node is needed even for a finished animation, since differences
  // in the timing of animation state updates can mean that an animation that's
  // in the Finished state at tree-building time on the main thread is still in
  // the Running state right after commit on the compositor thread.
  const bool has_any_transform_animation =
      layer->HasAnyAnimationTargetingProperty(TargetProperty::TRANSFORM);

  const bool has_surface = created_render_surface;

  // A transform node is needed to change the render target for subtree when
  // a scroll child's render target is different from the scroll parent's render
  // target.
  const bool scroll_child_has_different_target =
      ScrollParent(layer) &&
      Parent(layer)->effect_tree_index() !=
          ScrollParent(layer)->effect_tree_index();

  const bool is_at_boundary_of_3d_rendering_context =
      IsAtBoundaryOf3dRenderingContext(layer);

  bool requires_node = is_root || is_scrollable || has_significant_transform ||
                       has_any_transform_animation || has_surface || is_fixed ||
                       is_page_scale_layer || is_overscroll_elasticity_layer ||
                       scroll_child_has_different_target ||
                       is_at_boundary_of_3d_rendering_context;

  LayerType* transform_parent = GetTransformParent(data_from_ancestor, layer);
  DCHECK(is_root || transform_parent);

  int parent_index = kRootPropertyTreeNodeId;
  if (transform_parent)
    parent_index = transform_parent->transform_tree_index();

  int source_index = parent_index;

  gfx::Vector2dF source_offset;
  if (transform_parent) {
    if (ScrollParent(layer)) {
      LayerType* source = Parent(layer);
      source_offset += source->offset_to_transform_parent();
      source_index = source->transform_tree_index();
    } else if (!is_fixed) {
      source_offset = transform_parent->offset_to_transform_parent();
    } else {
      source_offset = data_from_ancestor.transform_tree_parent
                          ->offset_to_transform_parent();
      source_index =
          data_from_ancestor.transform_tree_parent->transform_tree_index();
      source_offset -= data_from_ancestor.scroll_snap;
    }
  }

  if (IsContainerForFixedPositionLayers(layer) || is_root) {
    data_for_children->affected_by_inner_viewport_bounds_delta =
        layer == data_from_ancestor.inner_viewport_scroll_layer;
    data_for_children->affected_by_outer_viewport_bounds_delta =
        layer == data_from_ancestor.outer_viewport_scroll_layer;
    if (is_scrollable) {
      DCHECK(!is_root);
      DCHECK(layer->transform().IsIdentity());
      data_for_children->transform_fixed_parent = Parent(layer);
    } else {
      data_for_children->transform_fixed_parent = layer;
    }
  }
  data_for_children->transform_tree_parent = layer;

  if (IsContainerForFixedPositionLayers(layer) || is_fixed)
    data_for_children->scroll_snap = gfx::Vector2dF();

  if (!requires_node) {
    data_for_children->should_flatten |= ShouldFlattenTransform(layer);
    gfx::Vector2dF local_offset = layer->position().OffsetFromOrigin() +
                                  layer->transform().To2dTranslation();
    gfx::Vector2dF source_to_parent;
    if (source_index != parent_index) {
      gfx::Transform to_parent;
      data_from_ancestor.property_trees->transform_tree.ComputeTransform(
          source_index, parent_index, &to_parent);
      source_to_parent = to_parent.To2dTranslation();
    }
    layer->set_offset_to_transform_parent(source_offset + source_to_parent +
                                          local_offset);
    layer->set_should_flatten_transform_from_property_tree(
        data_from_ancestor.should_flatten);
    layer->SetTransformTreeIndex(parent_index);
    return false;
  }

  data_for_children->property_trees->transform_tree.Insert(TransformNode(),
                                                           parent_index);

  TransformNode* node =
      data_for_children->property_trees->transform_tree.back();
  layer->SetTransformTreeIndex(node->id);
  data_for_children->property_trees->transform_id_to_index_map[layer->id()] =
      node->id;

  node->data.scrolls = is_scrollable;
  node->data.flattens_inherited_transform = data_for_children->should_flatten;

  node->data.sorting_context_id = layer->sorting_context_id();

  if (layer == data_from_ancestor.page_scale_layer)
    data_for_children->in_subtree_of_page_scale_layer = true;
  node->data.in_subtree_of_page_scale_layer =
      data_for_children->in_subtree_of_page_scale_layer;

  // Surfaces inherently flatten transforms.
  data_for_children->should_flatten =
      ShouldFlattenTransform(layer) || has_surface;
  DCHECK_GT(data_from_ancestor.property_trees->effect_tree.size(), 0u);

  data_for_children->property_trees->transform_tree.SetTargetId(
      node->id, data_for_children->property_trees->effect_tree
                    .Node(data_from_ancestor.render_target)
                    ->data.transform_id);
  data_for_children->property_trees->transform_tree.SetContentTargetId(
      node->id, data_for_children->property_trees->effect_tree
                    .Node(data_for_children->render_target)
                    ->data.transform_id);
  DCHECK_NE(
      data_for_children->property_trees->transform_tree.TargetId(node->id),
      kInvalidPropertyTreeNodeId);

  node->data.has_potential_animation = has_potentially_animated_transform;
  node->data.is_currently_animating = layer->TransformIsAnimating();
  if (has_potentially_animated_transform) {
    node->data.has_only_translation_animations =
        layer->HasOnlyTranslationTransforms();
  }

  float post_local_scale_factor = 1.0f;
  if (is_root)
    post_local_scale_factor =
        data_for_children->property_trees->transform_tree.device_scale_factor();

  if (is_page_scale_layer) {
    post_local_scale_factor *= data_from_ancestor.page_scale_factor;
    data_for_children->property_trees->transform_tree.set_page_scale_factor(
        data_from_ancestor.page_scale_factor);
  }

  if (has_surface && !is_root)
    node->data.needs_sublayer_scale = true;

  node->data.source_node_id = source_index;
  node->data.post_local_scale_factor = post_local_scale_factor;
  if (is_root) {
    data_for_children->property_trees->transform_tree.SetDeviceTransform(
        *data_from_ancestor.device_transform, layer->position());
    data_for_children->property_trees->transform_tree
        .SetDeviceTransformScaleFactor(*data_from_ancestor.device_transform);
  } else {
    node->data.source_offset = source_offset;
    node->data.update_post_local_transform(layer->position(),
                                           TransformOrigin(layer));
  }

  if (is_overscroll_elasticity_layer) {
    DCHECK(!is_scrollable);
    node->data.scroll_offset =
        gfx::ScrollOffset(data_from_ancestor.elastic_overscroll);
  } else if (!ScrollParent(layer)) {
    node->data.scroll_offset = layer->CurrentScrollOffset();
  }

  if (is_fixed) {
    if (data_from_ancestor.affected_by_inner_viewport_bounds_delta) {
      node->data.affected_by_inner_viewport_bounds_delta_x =
          PositionConstraint(layer).is_fixed_to_right_edge();
      node->data.affected_by_inner_viewport_bounds_delta_y =
          PositionConstraint(layer).is_fixed_to_bottom_edge();
      if (node->data.affected_by_inner_viewport_bounds_delta_x ||
          node->data.affected_by_inner_viewport_bounds_delta_y) {
        data_for_children->property_trees->transform_tree
            .AddNodeAffectedByInnerViewportBoundsDelta(node->id);
      }
    } else if (data_from_ancestor.affected_by_outer_viewport_bounds_delta) {
      node->data.affected_by_outer_viewport_bounds_delta_x =
          PositionConstraint(layer).is_fixed_to_right_edge();
      node->data.affected_by_outer_viewport_bounds_delta_y =
          PositionConstraint(layer).is_fixed_to_bottom_edge();
      if (node->data.affected_by_outer_viewport_bounds_delta_x ||
          node->data.affected_by_outer_viewport_bounds_delta_y) {
        data_for_children->property_trees->transform_tree
            .AddNodeAffectedByOuterViewportBoundsDelta(node->id);
      }
    }
  }

  node->data.local = layer->transform();
  node->data.update_pre_local_transform(TransformOrigin(layer));

  node->data.needs_local_transform_update = true;
  data_from_ancestor.property_trees->transform_tree.UpdateTransforms(node->id);

  layer->set_offset_to_transform_parent(gfx::Vector2dF());

  // Flattening (if needed) will be handled by |node|.
  layer->set_should_flatten_transform_from_property_tree(false);

  data_for_children->scroll_snap += node->data.scroll_snap;

  node->owner_id = layer->id();

  return true;
}

static inline bool HasPotentialOpacityAnimation(Layer* layer) {
  return layer->HasPotentiallyRunningOpacityAnimation() ||
         layer->OpacityCanAnimateOnImplThread();
}

static inline bool HasPotentialOpacityAnimation(LayerImpl* layer) {
  return layer->HasPotentiallyRunningOpacityAnimation() ||
         layer->test_properties()->opacity_can_animate;
}

static inline bool DoubleSided(Layer* layer) {
  return layer->double_sided();
}

static inline bool DoubleSided(LayerImpl* layer) {
  return layer->test_properties()->double_sided;
}

static inline bool ForceRenderSurface(Layer* layer) {
  return layer->force_render_surface_for_testing();
}

static inline bool ForceRenderSurface(LayerImpl* layer) {
  return layer->test_properties()->force_render_surface;
}

template <typename LayerType>
static inline bool LayerIsInExisting3DRenderingContext(LayerType* layer) {
  return layer->Is3dSorted() && Parent(layer) && Parent(layer)->Is3dSorted() &&
         (Parent(layer)->sorting_context_id() == layer->sorting_context_id());
}

static inline bool IsRootForIsolatedGroup(Layer* layer) {
  return layer->is_root_for_isolated_group();
}

static inline bool IsRootForIsolatedGroup(LayerImpl* layer) {
  return false;
}

static inline int NumDescendantsThatDrawContent(Layer* layer) {
  return layer->NumDescendantsThatDrawContent();
}

static inline int NumDescendantsThatDrawContent(LayerImpl* layer) {
  return layer->test_properties()->num_descendants_that_draw_content;
}

static inline float EffectiveOpacity(Layer* layer) {
  return layer->EffectiveOpacity();
}

static inline float EffectiveOpacity(LayerImpl* layer) {
  return layer->test_properties()->hide_layer_and_subtree
             ? 0.f
             : layer->test_properties()->opacity;
}

static inline float Opacity(Layer* layer) {
  return layer->opacity();
}

static inline float Opacity(LayerImpl* layer) {
  return layer->test_properties()->opacity;
}

static inline const FilterOperations& BackgroundFilters(Layer* layer) {
  return layer->background_filters();
}

static inline const FilterOperations& BackgroundFilters(LayerImpl* layer) {
  return layer->test_properties()->background_filters;
}

static inline bool HideLayerAndSubtree(Layer* layer) {
  return layer->hide_layer_and_subtree();
}

static inline bool HideLayerAndSubtree(LayerImpl* layer) {
  return layer->test_properties()->hide_layer_and_subtree;
}

static inline bool AlwaysUseActiveTreeOpacity(Layer* layer) {
  return layer->AlwaysUseActiveTreeOpacity();
}

static inline bool AlwaysUseActiveTreeOpacity(LayerImpl* layer) {
  return false;
}

static inline bool HasCopyRequest(Layer* layer) {
  return layer->HasCopyRequest();
}

static inline bool HasCopyRequest(LayerImpl* layer) {
  return !layer->test_properties()->copy_requests.empty();
}

template <typename LayerType>
bool ShouldCreateRenderSurface(LayerType* layer,
                               gfx::Transform current_transform,
                               bool axis_aligned) {
  const bool preserves_2d_axis_alignment =
      (current_transform * layer->transform()).Preserves2dAxisAlignment() &&
      axis_aligned && layer->AnimationsPreserveAxisAlignment();
  const bool is_root = !Parent(layer);
  if (is_root)
    return true;

  // If the layer uses a mask and the layer is not a replica layer.
  // TODO(weiliangc): After slimming paint there won't be replica layers.
  if (MaskLayer(layer) && ReplicaLayer(Parent(layer)) != layer) {
    return true;
  }

  // If the layer has a reflection.
  if (ReplicaLayer(layer)) {
    return true;
  }

  // If the layer uses a CSS filter.
  if (!layer->filters().IsEmpty() || !BackgroundFilters(layer).IsEmpty()) {
    return true;
  }

  // If the layer will use a CSS filter.  In this case, the animation
  // will start and add a filter to this layer, so it needs a surface.
  if (layer->HasPotentiallyRunningFilterAnimation()) {
    return true;
  }

  int num_descendants_that_draw_content = NumDescendantsThatDrawContent(layer);

  // If the layer flattens its subtree, but it is treated as a 3D object by its
  // parent (i.e. parent participates in a 3D rendering context).
  if (LayerIsInExisting3DRenderingContext(layer) &&
      ShouldFlattenTransform(layer) && num_descendants_that_draw_content > 0) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface flattening",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If the layer has blending.
  // TODO(rosca): this is temporary, until blending is implemented for other
  // types of quads than RenderPassDrawQuad. Layers having descendants that draw
  // content will still create a separate rendering surface.
  if (!layer->uses_default_blend_mode()) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface blending",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }
  // If the layer clips its descendants but it is not axis-aligned with respect
  // to its parent.
  bool layer_clips_external_content = LayerClipsSubtree(layer);
  if (layer_clips_external_content && !preserves_2d_axis_alignment &&
      num_descendants_that_draw_content > 0) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface clipping",
        TRACE_EVENT_SCOPE_THREAD);
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

  if (EffectiveOpacity(layer) != 1.f && ShouldFlattenTransform(layer) &&
      at_least_two_layers_in_subtree_draw_content) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface opacity",
        TRACE_EVENT_SCOPE_THREAD);
    DCHECK(!is_root);
    return true;
  }
  // If the layer has isolation.
  // TODO(rosca): to be optimized - create separate rendering surface only when
  // the blending descendants might have access to the content behind this layer
  // (layer has transparent background or descendants overflow).
  // https://code.google.com/p/chromium/issues/detail?id=301738
  if (IsRootForIsolatedGroup(layer)) {
    TRACE_EVENT_INSTANT0(
        "cc", "PropertyTreeBuilder::ShouldCreateRenderSurface isolation",
        TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // If we force it.
  if (ForceRenderSurface(layer))
    return true;

  // If we'll make a copy of the layer's contents.
  if (HasCopyRequest(layer))
    return true;

  return false;
}

static void TakeCopyRequests(
    Layer* layer,
    std::vector<std::unique_ptr<CopyOutputRequest>>* copy_requests) {
  layer->TakeCopyRequests(copy_requests);
}

static void TakeCopyRequests(
    LayerImpl* layer,
    std::vector<std::unique_ptr<CopyOutputRequest>>* copy_requests) {
  for (auto& request : layer->test_properties()->copy_requests)
    copy_requests->push_back(std::move(request));
  layer->test_properties()->copy_requests.clear();
}

template <typename LayerType>
bool AddEffectNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    DataForRecursion<LayerType>* data_for_children) {
  const bool is_root = !Parent(layer);
  const bool has_transparency = EffectiveOpacity(layer) != 1.f;
  const bool has_potential_opacity_animation =
      HasPotentialOpacityAnimation(layer);
  const bool should_create_render_surface = ShouldCreateRenderSurface(
      layer, data_from_ancestor.compound_transform_since_render_target,
      data_from_ancestor.axis_align_since_render_target);
  data_for_children->axis_align_since_render_target &=
      layer->AnimationsPreserveAxisAlignment();

  bool requires_node = is_root || has_transparency ||
                       has_potential_opacity_animation ||
                       should_create_render_surface;

  int parent_id = data_from_ancestor.effect_tree_parent;

  if (!requires_node) {
    layer->SetEffectTreeIndex(parent_id);
    data_for_children->effect_tree_parent = parent_id;
    data_for_children->compound_transform_since_render_target *=
        layer->transform();
    return false;
  }

  EffectNode node;
  node.owner_id = layer->id();
  if (AlwaysUseActiveTreeOpacity(layer)) {
    data_for_children->property_trees->always_use_active_tree_opacity_effect_ids
        .push_back(node.owner_id);
  }

  node.data.opacity = Opacity(layer);
  node.data.has_render_surface = should_create_render_surface;
  node.data.has_copy_request = HasCopyRequest(layer);
  node.data.background_filters = BackgroundFilters(layer);
  node.data.has_potential_opacity_animation = has_potential_opacity_animation;
  node.data.double_sided = DoubleSided(layer);
  node.data.subtree_hidden = HideLayerAndSubtree(layer);
  node.data.is_currently_animating_opacity = layer->OpacityIsAnimating();

  EffectTree& effect_tree = data_for_children->property_trees->effect_tree;
  if (MaskLayer(layer)) {
    node.data.mask_layer_id = MaskLayer(layer)->id();
    effect_tree.AddMaskOrReplicaLayerId(node.data.mask_layer_id);
  }
  if (ReplicaLayer(layer)) {
    node.data.replica_layer_id = ReplicaLayer(layer)->id();
    effect_tree.AddMaskOrReplicaLayerId(node.data.replica_layer_id);
    if (MaskLayer(ReplicaLayer(layer))) {
      node.data.replica_mask_layer_id = MaskLayer(ReplicaLayer(layer))->id();
      effect_tree.AddMaskOrReplicaLayerId(node.data.replica_mask_layer_id);
    }
  }

  if (!is_root) {
    // The effect node's transform id is used only when we create a render
    // surface. So, we can leave the default value when we don't create a render
    // surface.
    if (should_create_render_surface) {
      // In this case, we will create a transform node, so it's safe to use the
      // next available id from the transform tree as this effect node's
      // transform id.
      node.data.transform_id =
          data_from_ancestor.property_trees->transform_tree.next_available_id();
      node.data.has_unclipped_descendants =
          (NumUnclippedDescendants(layer) != 0);
    }
    node.data.clip_id = data_from_ancestor.clip_tree_parent;
  } else {
    // Root render surface acts the unbounded and untransformed to draw content
    // into. Transform node created from root layer (includes device scale
    // factor) and clip node created from root layer (include viewports) applies
    // to root render surface's content, but not root render surface itself.
    node.data.transform_id = kRootPropertyTreeNodeId;
    node.data.clip_id = kViewportClipTreeNodeId;
  }
  data_for_children->effect_tree_parent = effect_tree.Insert(node, parent_id);
  int node_id = data_for_children->effect_tree_parent;
  layer->SetEffectTreeIndex(node_id);
  data_for_children->property_trees->effect_id_to_index_map[layer->id()] =
      data_for_children->effect_tree_parent;

  std::vector<std::unique_ptr<CopyOutputRequest>> layer_copy_requests;
  TakeCopyRequests(layer, &layer_copy_requests);
  for (auto& it : layer_copy_requests) {
    effect_tree.AddCopyRequest(node_id, std::move(it));
  }
  layer_copy_requests.clear();

  if (should_create_render_surface) {
    data_for_children->compound_transform_since_render_target =
        gfx::Transform();
    data_for_children->axis_align_since_render_target = true;
  }
  return should_create_render_surface;
}

template <typename LayerType>
void AddScrollNodeIfNeeded(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    DataForRecursion<LayerType>* data_for_children) {
  int parent_id = GetScrollParentId(data_from_ancestor, layer);

  bool is_root = !Parent(layer);
  bool scrollable = layer->scrollable();
  bool contains_non_fast_scrollable_region =
      !layer->non_fast_scrollable_region().IsEmpty();
  uint32_t main_thread_scrolling_reasons =
      layer->main_thread_scrolling_reasons();

  bool scroll_node_uninheritable_criteria =
      is_root || scrollable || contains_non_fast_scrollable_region;
  bool has_different_main_thread_scrolling_reasons =
      main_thread_scrolling_reasons !=
      data_from_ancestor.main_thread_scrolling_reasons;
  bool requires_node =
      scroll_node_uninheritable_criteria ||
      (main_thread_scrolling_reasons !=
           MainThreadScrollingReason::kNotScrollingOnMain &&
       (has_different_main_thread_scrolling_reasons ||
        data_from_ancestor
            .scroll_tree_parent_created_by_uninheritable_criteria));

  if (!requires_node) {
    data_for_children->scroll_tree_parent = parent_id;
  } else {
    ScrollNode node;
    node.owner_id = layer->id();
    node.data.scrollable = scrollable;
    node.data.main_thread_scrolling_reasons = main_thread_scrolling_reasons;
    node.data.contains_non_fast_scrollable_region =
        contains_non_fast_scrollable_region;
    gfx::Size clip_bounds;
    if (layer->scroll_clip_layer()) {
      clip_bounds = layer->scroll_clip_layer()->bounds();
      DCHECK(layer->scroll_clip_layer()->transform_tree_index() !=
             kInvalidPropertyTreeNodeId);
      node.data.max_scroll_offset_affected_by_page_scale =
          !data_from_ancestor.property_trees->transform_tree
               .Node(layer->scroll_clip_layer()->transform_tree_index())
               ->data.in_subtree_of_page_scale_layer &&
          data_from_ancestor.in_subtree_of_page_scale_layer;
    }

    node.data.scroll_clip_layer_bounds = clip_bounds;
    node.data.is_inner_viewport_scroll_layer =
        layer == data_from_ancestor.inner_viewport_scroll_layer;
    node.data.is_outer_viewport_scroll_layer =
        layer == data_from_ancestor.outer_viewport_scroll_layer;

    node.data.bounds = layer->bounds();
    node.data.offset_to_transform_parent = layer->offset_to_transform_parent();
    node.data.should_flatten =
        layer->should_flatten_transform_from_property_tree();
    node.data.user_scrollable_horizontal = layer->user_scrollable_horizontal();
    node.data.user_scrollable_vertical = layer->user_scrollable_vertical();
    node.data.element_id = layer->element_id();
    node.data.transform_id =
        data_for_children->transform_tree_parent->transform_tree_index();

    data_for_children->scroll_tree_parent =
        data_for_children->property_trees->scroll_tree.Insert(node, parent_id);
    data_for_children->main_thread_scrolling_reasons =
        node.data.main_thread_scrolling_reasons;
    data_for_children->scroll_tree_parent_created_by_uninheritable_criteria =
        scroll_node_uninheritable_criteria;
    data_for_children->property_trees->scroll_id_to_index_map[layer->id()] =
        data_for_children->scroll_tree_parent;

    if (node.data.scrollable) {
      data_for_children->property_trees->scroll_tree.SetBaseScrollOffset(
          layer->id(), layer->CurrentScrollOffset());
    }
  }

  layer->SetScrollTreeIndex(data_for_children->scroll_tree_parent);
}

template <typename LayerType>
void SetBackfaceVisibilityTransform(LayerType* layer,
                                    bool created_transform_node) {
  const bool is_at_boundary_of_3d_rendering_context =
      IsAtBoundaryOf3dRenderingContext(layer);
  if (layer->use_parent_backface_visibility()) {
    DCHECK(!is_at_boundary_of_3d_rendering_context);
    DCHECK(Parent(layer));
    DCHECK(!Parent(layer)->use_parent_backface_visibility());
    layer->SetUseLocalTransformForBackfaceVisibility(
        Parent(layer)->use_local_transform_for_backface_visibility());
    layer->SetShouldCheckBackfaceVisibility(
        Parent(layer)->should_check_backface_visibility());
  } else {
    // The current W3C spec on CSS transforms says that backface visibility
    // should be determined differently depending on whether the layer is in a
    // "3d rendering context" or not. For Chromium code, we can determine
    // whether we are in a 3d rendering context by checking if the parent
    // preserves 3d.
    const bool use_local_transform =
        !layer->Is3dSorted() ||
        (layer->Is3dSorted() && is_at_boundary_of_3d_rendering_context);
    layer->SetUseLocalTransformForBackfaceVisibility(use_local_transform);

    // A double-sided layer's backface can been shown when its visibile.
    if (DoubleSided(layer))
      layer->SetShouldCheckBackfaceVisibility(false);
    // The backface of a layer that uses local transform for backface visibility
    // is not visible when it does not create a transform node as its local
    // transform is identity or 2d translation and is not animating.
    else if (use_local_transform && !created_transform_node)
      layer->SetShouldCheckBackfaceVisibility(false);
    else
      layer->SetShouldCheckBackfaceVisibility(true);
  }
}

template <typename LayerType>
void SetSafeOpaqueBackgroundColor(
    const DataForRecursion<LayerType>& data_from_ancestor,
    LayerType* layer,
    DataForRecursion<LayerType>* data_for_children) {
  SkColor background_color = layer->background_color();
  data_for_children->safe_opaque_background_color =
      SkColorGetA(background_color) == 255
          ? background_color
          : data_from_ancestor.safe_opaque_background_color;
  layer->SetSafeOpaqueBackgroundColor(
      data_for_children->safe_opaque_background_color);
}

static void SetLayerPropertyChangedForChild(Layer* parent, Layer* child) {
  if (parent->subtree_property_changed())
    child->SetSubtreePropertyChanged();
}

static void SetLayerPropertyChangedForChild(LayerImpl* parent,
                                            LayerImpl* child) {}

template <typename LayerType>
void BuildPropertyTreesInternal(
    LayerType* layer,
    const DataForRecursion<LayerType>& data_from_parent,
    DataForRecursionFromChild<LayerType>* data_to_parent) {
  layer->set_property_tree_sequence_number(
      data_from_parent.property_trees->sequence_number);

  DataForRecursion<LayerType> data_for_children(data_from_parent);

  bool created_render_surface =
      AddEffectNodeIfNeeded(data_from_parent, layer, &data_for_children);

  if (created_render_surface) {
    data_for_children.render_target = data_for_children.effect_tree_parent;
    layer->set_draw_blend_mode(SkXfermode::kSrcOver_Mode);
  } else {
    layer->set_draw_blend_mode(layer->blend_mode());
  }

  bool created_transform_node = AddTransformNodeIfNeeded(
      data_from_parent, layer, created_render_surface, &data_for_children);
  AddClipNodeIfNeeded(data_from_parent, layer, created_render_surface,
                      created_transform_node, &data_for_children);

  AddScrollNodeIfNeeded(data_from_parent, layer, &data_for_children);

  SetBackfaceVisibilityTransform(layer, created_transform_node);
  SetSafeOpaqueBackgroundColor(data_from_parent, layer, &data_for_children);

  for (size_t i = 0; i < Children(layer).size(); ++i) {
    LayerType* current_child = ChildAt(layer, i);
    SetLayerPropertyChangedForChild(layer, current_child);
    if (!ScrollParent(current_child)) {
      DataForRecursionFromChild<LayerType> data_from_child;
      BuildPropertyTreesInternal(current_child, data_for_children,
                                 &data_from_child);
      data_to_parent->Merge(data_from_child);
    } else {
      // The child should be included in its scroll parent's list of scroll
      // children.
      DCHECK(ScrollChildren(ScrollParent(current_child))->count(current_child));
    }
  }

  if (ScrollChildren(layer)) {
    for (LayerType* scroll_child : *ScrollChildren(layer)) {
      DCHECK_EQ(ScrollParent(scroll_child), layer);
      DataForRecursionFromChild<LayerType> data_from_child;
      DCHECK(Parent(scroll_child));
      data_for_children.effect_tree_parent =
          Parent(scroll_child)->effect_tree_index();
      data_for_children.render_target =
          Parent(scroll_child)->effect_tree_index();
      BuildPropertyTreesInternal(scroll_child, data_for_children,
                                 &data_from_child);
      data_to_parent->Merge(data_from_child);
    }
  }

  if (ReplicaLayer(layer)) {
    DataForRecursionFromChild<LayerType> data_from_child;
    BuildPropertyTreesInternal(ReplicaLayer(layer), data_for_children,
                               &data_from_child);
    data_to_parent->Merge(data_from_child);
  }

  if (MaskLayer(layer)) {
    MaskLayer(layer)->set_property_tree_sequence_number(
        data_from_parent.property_trees->sequence_number);
    MaskLayer(layer)->set_offset_to_transform_parent(
        layer->offset_to_transform_parent());
    MaskLayer(layer)->SetTransformTreeIndex(layer->transform_tree_index());
    MaskLayer(layer)->SetClipTreeIndex(layer->clip_tree_index());
    MaskLayer(layer)->SetEffectTreeIndex(layer->effect_tree_index());
    MaskLayer(layer)->SetScrollTreeIndex(layer->scroll_tree_index());
  }

  EffectNode* effect_node = data_for_children.property_trees->effect_tree.Node(
      data_for_children.effect_tree_parent);

  if (effect_node->owner_id == layer->id()) {
    if (effect_node->data.has_copy_request)
      data_to_parent->num_copy_requests_in_subtree++;
    effect_node->data.num_copy_requests_in_subtree =
        data_to_parent->num_copy_requests_in_subtree;
  }
}

}  // namespace

void CC_EXPORT
PropertyTreeBuilder::PreCalculateMetaInformation(Layer* root_layer) {
  PreCalculateMetaInformationRecursiveData recursive_data;
  PreCalculateMetaInformationInternal(root_layer, &recursive_data);
}

void CC_EXPORT PropertyTreeBuilder::PreCalculateMetaInformationForTesting(
    LayerImpl* root_layer) {
  PreCalculateMetaInformationRecursiveData recursive_data;
  PreCalculateMetaInformationInternalForTesting(root_layer, &recursive_data);
}

Layer* PropertyTreeBuilder::FindFirstScrollableLayer(Layer* layer) {
  if (!layer)
    return nullptr;

  if (layer->scrollable())
    return layer;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    Layer* found = FindFirstScrollableLayer(layer->children()[i].get());
    if (found)
      return found;
  }

  return nullptr;
}

template <typename LayerType>
void BuildPropertyTreesTopLevelInternal(
    LayerType* root_layer,
    const LayerType* page_scale_layer,
    const LayerType* inner_viewport_scroll_layer,
    const LayerType* outer_viewport_scroll_layer,
    const LayerType* overscroll_elasticity_layer,
    const gfx::Vector2dF& elastic_overscroll,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees,
    SkColor color) {
  if (!property_trees->needs_rebuild) {
    draw_property_utils::UpdatePageScaleFactor(
        property_trees, page_scale_layer, page_scale_factor,
        device_scale_factor, device_transform);
    draw_property_utils::UpdateElasticOverscroll(
        property_trees, overscroll_elasticity_layer, elastic_overscroll);
    property_trees->clip_tree.SetViewportClip(gfx::RectF(viewport));
    property_trees->transform_tree.SetDeviceTransform(device_transform,
                                                      root_layer->position());
    return;
  }

  property_trees->sequence_number++;

  DataForRecursion<LayerType> data_for_recursion;
  data_for_recursion.property_trees = property_trees;
  data_for_recursion.transform_tree_parent = nullptr;
  data_for_recursion.transform_fixed_parent = nullptr;
  data_for_recursion.render_target = kRootPropertyTreeNodeId;
  data_for_recursion.clip_tree_parent = kRootPropertyTreeNodeId;
  data_for_recursion.effect_tree_parent = kInvalidPropertyTreeNodeId;
  data_for_recursion.scroll_tree_parent = kRootPropertyTreeNodeId;
  data_for_recursion.page_scale_layer = page_scale_layer;
  data_for_recursion.inner_viewport_scroll_layer = inner_viewport_scroll_layer;
  data_for_recursion.outer_viewport_scroll_layer = outer_viewport_scroll_layer;
  data_for_recursion.overscroll_elasticity_layer = overscroll_elasticity_layer;
  data_for_recursion.elastic_overscroll = elastic_overscroll;
  data_for_recursion.page_scale_factor = page_scale_factor;
  data_for_recursion.in_subtree_of_page_scale_layer = false;
  data_for_recursion.affected_by_inner_viewport_bounds_delta = false;
  data_for_recursion.affected_by_outer_viewport_bounds_delta = false;
  data_for_recursion.should_flatten = false;
  data_for_recursion.target_is_clipped = false;
  data_for_recursion.is_hidden = false;
  data_for_recursion.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  data_for_recursion.scroll_tree_parent_created_by_uninheritable_criteria =
      true;
  data_for_recursion.device_transform = &device_transform;

  data_for_recursion.property_trees->transform_tree.clear();
  data_for_recursion.property_trees->clip_tree.clear();
  data_for_recursion.property_trees->effect_tree.clear();
  data_for_recursion.property_trees->scroll_tree.clear();
  data_for_recursion.compound_transform_since_render_target = gfx::Transform();
  data_for_recursion.axis_align_since_render_target = true;
  data_for_recursion.property_trees->transform_tree.set_device_scale_factor(
      device_scale_factor);
  data_for_recursion.safe_opaque_background_color = color;
  data_for_recursion.property_trees->transform_id_to_index_map.clear();
  data_for_recursion.property_trees->effect_id_to_index_map.clear();
  data_for_recursion.property_trees->clip_id_to_index_map.clear();
  data_for_recursion.property_trees->scroll_id_to_index_map.clear();
  data_for_recursion.property_trees->always_use_active_tree_opacity_effect_ids
      .clear();

  ClipNode root_clip;
  root_clip.data.resets_clip = true;
  root_clip.data.applies_local_clip = true;
  root_clip.data.clip = gfx::RectF(viewport);
  root_clip.data.transform_id = kRootPropertyTreeNodeId;
  data_for_recursion.clip_tree_parent =
      data_for_recursion.property_trees->clip_tree.Insert(
          root_clip, kRootPropertyTreeNodeId);

  DataForRecursionFromChild<LayerType> data_from_child;
  BuildPropertyTreesInternal(root_layer, data_for_recursion, &data_from_child);
  property_trees->needs_rebuild = false;

  // The transform tree is kept up to date as it is built, but the
  // combined_clips stored in the clip tree and the screen_space_opacity and
  // is_drawn in the effect tree aren't computed during tree building.
  property_trees->transform_tree.set_needs_update(false);
  property_trees->clip_tree.set_needs_update(true);
  property_trees->effect_tree.set_needs_update(true);
  property_trees->scroll_tree.set_needs_update(false);
}

#if DCHECK_IS_ON()
static void CheckScrollAndClipPointersForLayer(Layer* layer) {
  if (!layer)
    return;

  if (layer->scroll_children()) {
    for (std::set<Layer*>::iterator it = layer->scroll_children()->begin();
         it != layer->scroll_children()->end(); ++it) {
      DCHECK_EQ((*it)->scroll_parent(), layer);
    }
  }

  if (layer->clip_children()) {
    for (std::set<Layer*>::iterator it = layer->clip_children()->begin();
         it != layer->clip_children()->end(); ++it) {
      DCHECK_EQ((*it)->clip_parent(), layer);
    }
  }
}
#endif

void PropertyTreeBuilder::BuildPropertyTrees(
    Layer* root_layer,
    const Layer* page_scale_layer,
    const Layer* inner_viewport_scroll_layer,
    const Layer* outer_viewport_scroll_layer,
    const Layer* overscroll_elasticity_layer,
    const gfx::Vector2dF& elastic_overscroll,
    float page_scale_factor,
    float device_scale_factor,
    const gfx::Rect& viewport,
    const gfx::Transform& device_transform,
    PropertyTrees* property_trees) {
  property_trees->is_main_thread = true;
  property_trees->is_active = false;
  SkColor color = root_layer->layer_tree_host()->background_color();
  if (SkColorGetA(color) != 255)
    color = SkColorSetA(color, 255);
  BuildPropertyTreesTopLevelInternal(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_layer,
      elastic_overscroll, page_scale_factor, device_scale_factor, viewport,
      device_transform, property_trees, color);
#if DCHECK_IS_ON()
  for (auto* layer : *root_layer->layer_tree_host())
    CheckScrollAndClipPointersForLayer(layer);
#endif
}

void PropertyTreeBuilder::BuildPropertyTrees(
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
    PropertyTrees* property_trees) {
  property_trees->is_main_thread = false;
  property_trees->is_active = root_layer->IsActive();
  SkColor color = root_layer->layer_tree_impl()->background_color();
  if (SkColorGetA(color) != 255)
    color = SkColorSetA(color, 255);
  BuildPropertyTreesTopLevelInternal(
      root_layer, page_scale_layer, inner_viewport_scroll_layer,
      outer_viewport_scroll_layer, overscroll_elasticity_layer,
      elastic_overscroll, page_scale_factor, device_scale_factor, viewport,
      device_transform, property_trees, color);
  property_trees->ResetCachedData();
}

}  // namespace cc
