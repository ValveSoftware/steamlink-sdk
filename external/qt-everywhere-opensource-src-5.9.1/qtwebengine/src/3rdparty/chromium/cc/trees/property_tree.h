// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_H_
#define CC_TREES_PROPERTY_TREE_H_

#include <stddef.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/base/synced_property.h"
#include "cc/layers/layer_sticky_position_constraint.h"
#include "cc/output/filter_operations.h"
#include "cc/trees/element_id.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {

class CopyOutputRequest;
class LayerTreeImpl;
class ScrollState;
struct ClipNode;
struct EffectNode;
struct ScrollAndScaleSet;
struct ScrollNode;
struct TransformNode;
struct TransformCachedNodeData;

typedef SyncedProperty<AdditionGroup<gfx::ScrollOffset>> SyncedScrollOffset;

class PropertyTrees;

template <typename T>
class CC_EXPORT PropertyTree {
 public:
  PropertyTree();
  PropertyTree(const PropertyTree& other) = delete;

  // These C++ special member functions cannot be implicit inline because
  // they are exported by CC_EXPORT. They will be instantiated in every
  // compilation units that included this header, and compilation can fail
  // because T may be incomplete.
  ~PropertyTree();
  PropertyTree<T>& operator=(const PropertyTree<T>&);

  // Property tree node starts from index 0.
  static const int kInvalidNodeId = -1;
  static const int kRootNodeId = 0;

  bool operator==(const PropertyTree<T>& other) const;

  int Insert(const T& tree_node, int parent_id);

  T* Node(int i) {
    // TODO(vollick): remove this.
    CHECK(i < static_cast<int>(nodes_.size()));
    return i > kInvalidNodeId ? &nodes_[i] : nullptr;
  }
  const T* Node(int i) const {
    // TODO(vollick): remove this.
    CHECK(i < static_cast<int>(nodes_.size()));
    return i > kInvalidNodeId ? &nodes_[i] : nullptr;
  }

  T* parent(const T* t) { return Node(t->parent_id); }
  const T* parent(const T* t) const { return Node(t->parent_id); }

  T* back() { return size() ? &nodes_.back() : nullptr; }
  const T* back() const { return size() ? &nodes_.back() : nullptr; }

  void clear();
  size_t size() const { return nodes_.size(); }

  void set_needs_update(bool needs_update) { needs_update_ = needs_update; }
  bool needs_update() const { return needs_update_; }

  std::vector<T>& nodes() { return nodes_; }
  const std::vector<T>& nodes() const { return nodes_; }

  int next_available_id() const { return static_cast<int>(size()); }

  void SetPropertyTrees(PropertyTrees* property_trees) {
    property_trees_ = property_trees;
  }
  PropertyTrees* property_trees() const { return property_trees_; }

  void AsValueInto(base::trace_event::TracedValue* value) const;

 private:
  std::vector<T> nodes_;

  friend class TransformTree;
  bool needs_update_;
  PropertyTrees* property_trees_;
};

struct StickyPositionNodeData {
  int scroll_ancestor;
  LayerStickyPositionConstraint constraints;

  // This is the offset that blink has already applied to counteract the main
  // thread scroll offset of the scroll ancestor. We need to account for this
  // by computing the additional offset necessary to keep the element stuck.
  gfx::Vector2dF main_thread_offset;

  StickyPositionNodeData() : scroll_ancestor(-1) {}
};

class CC_EXPORT TransformTree final : public PropertyTree<TransformNode> {
 public:
  TransformTree();

  // These C++ special member functions cannot be implicit inline because
  // they are exported by CC_EXPORT. They will be instantiated in every
  // compilation units that included this header, and compilation can fail
  // because TransformCachedNodeData may be incomplete.
  TransformTree(const TransformTree&) = delete;
  ~TransformTree();
  TransformTree& operator=(const TransformTree&);

  bool operator==(const TransformTree& other) const;

  static const int kContentsRootNodeId = 1;

  int Insert(const TransformNode& tree_node, int parent_id);

  void clear();

  void OnTransformAnimated(const gfx::Transform& transform,
                           int id,
                           LayerTreeImpl* layer_tree_impl);
  // Computes the change of basis transform from node |source_id| to |dest_id|.
  // This is used by scroll children to compute transform from their scroll
  // parent space (source) to their parent space (destination) and it can atmost
  // be a translation. This function assumes that the path from source to
  // destination has only translations. So, it should not be called when there
  // can be intermediate 3d transforms but the end result is a translation.
  bool ComputeTranslation(int source_id,
                          int dest_id,
                          gfx::Transform* transform) const;

  void ResetChangeTracking();
  // Updates the parent, target, and screen space transforms and snapping.
  void UpdateTransforms(int id);
  void UpdateTransformChanged(TransformNode* node,
                              TransformNode* parent_node,
                              TransformNode* source_node);
  void UpdateNodeAndAncestorsAreAnimatedOrInvertible(
      TransformNode* node,
      TransformNode* parent_node);

  void set_needs_update(bool needs_update);

  // A TransformNode's source_to_parent value is used to account for the fact
  // that fixed-position layers are positioned by Blink wrt to their layer tree
  // parent (their "source"), but are parented in the transform tree by their
  // fixed-position container. This value needs to be updated on main-thread
  // property trees (for position changes initiated by Blink), but not on the
  // compositor thread (since the offset from a node corresponding to a
  // fixed-position layer to its fixed-position container is unaffected by
  // compositor-driven effects).
  void set_source_to_parent_updates_allowed(bool allowed) {
    source_to_parent_updates_allowed_ = allowed;
  }
  bool source_to_parent_updates_allowed() const {
    return source_to_parent_updates_allowed_;
  }

  // We store the page scale factor on the transform tree so that it can be
  // easily be retrieved and updated in UpdatePageScale.
  void set_page_scale_factor(float page_scale_factor) {
    page_scale_factor_ = page_scale_factor;
  }
  float page_scale_factor() const { return page_scale_factor_; }

  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }
  float device_scale_factor() const { return device_scale_factor_; }

  void SetRootTransformsAndScales(float device_scale_factor,
                                  float page_scale_factor_for_root,
                                  const gfx::Transform& device_transform,
                                  gfx::PointF root_position);
  float device_transform_scale_factor() const {
    return device_transform_scale_factor_;
  }

  void UpdateInnerViewportContainerBoundsDelta();

  void UpdateOuterViewportContainerBoundsDelta();

  void AddNodeAffectedByInnerViewportBoundsDelta(int node_id);
  void AddNodeAffectedByOuterViewportBoundsDelta(int node_id);

  bool HasNodesAffectedByInnerViewportBoundsDelta() const;
  bool HasNodesAffectedByOuterViewportBoundsDelta() const;

  const std::vector<int>& nodes_affected_by_inner_viewport_bounds_delta()
      const {
    return nodes_affected_by_inner_viewport_bounds_delta_;
  }
  const std::vector<int>& nodes_affected_by_outer_viewport_bounds_delta()
      const {
    return nodes_affected_by_outer_viewport_bounds_delta_;
  }

  const gfx::Transform& FromScreen(int node_id) const;
  void SetFromScreen(int node_id, const gfx::Transform& transform);

  const gfx::Transform& ToScreen(int node_id) const;
  void SetToScreen(int node_id, const gfx::Transform& transform);

  int TargetId(int node_id) const;
  void SetTargetId(int node_id, int target_id);

  int ContentTargetId(int node_id) const;
  void SetContentTargetId(int node_id, int content_target_id);

  const std::vector<TransformCachedNodeData>& cached_data() const {
    return cached_data_;
  }

  StickyPositionNodeData* StickyPositionData(int node_id);

  // Computes the combined transform between |source_id| and |dest_id|. These
  // two nodes must be on the same ancestor chain.
  void CombineTransformsBetween(int source_id,
                                int dest_id,
                                gfx::Transform* transform) const;

  // Computes the combined inverse transform between |source_id| and |dest_id|
  // and returns false if the inverse of a singular transform was used. These
  // two nodes must be on the same ancestor chain.
  bool CombineInversesBetween(int source_id,
                              int dest_id,
                              gfx::Transform* transform) const;

 private:
  // Returns true iff the node at |desc_id| is a descendant of the node at
  // |anc_id|.
  bool IsDescendant(int desc_id, int anc_id) const;

  void UpdateLocalTransform(TransformNode* node);
  void UpdateScreenSpaceTransform(TransformNode* node,
                                  TransformNode* parent_node,
                                  TransformNode* target_node);
  void UpdateAnimationProperties(TransformNode* node,
                                 TransformNode* parent_node);
  void UndoSnapping(TransformNode* node);
  void UpdateSnapping(TransformNode* node);
  void UpdateNodeAndAncestorsHaveIntegerTranslations(
      TransformNode* node,
      TransformNode* parent_node);
  bool NeedsSourceToParentUpdate(TransformNode* node);

  bool source_to_parent_updates_allowed_;
  // When to_screen transform has perspective, the transform node's sublayer
  // scale is calculated using page scale factor, device scale factor and the
  // scale factor of device transform. So we need to store them explicitly.
  float page_scale_factor_;
  float device_scale_factor_;
  float device_transform_scale_factor_;
  std::vector<int> nodes_affected_by_inner_viewport_bounds_delta_;
  std::vector<int> nodes_affected_by_outer_viewport_bounds_delta_;
  std::vector<TransformCachedNodeData> cached_data_;
  std::vector<StickyPositionNodeData> sticky_position_data_;
};

class CC_EXPORT ClipTree final : public PropertyTree<ClipNode> {
 public:
  bool operator==(const ClipTree& other) const;

  static const int kViewportNodeId = 1;

  void SetViewportClip(gfx::RectF viewport_rect);
  gfx::RectF ViewportClip() const;
};

class CC_EXPORT EffectTree final : public PropertyTree<EffectNode> {
 public:
  EffectTree();
  ~EffectTree();

  EffectTree& operator=(const EffectTree& from);
  bool operator==(const EffectTree& other) const;

  static const int kContentsRootNodeId = 1;

  void clear();

  float EffectiveOpacity(const EffectNode* node) const;

  void UpdateSurfaceContentsScale(EffectNode* node);

  void OnOpacityAnimated(float opacity, int id, LayerTreeImpl* layer_tree_impl);
  void OnFilterAnimated(const FilterOperations& filters,
                        int id,
                        LayerTreeImpl* layer_tree_impl);

  void UpdateEffects(int id);

  void UpdateEffectChanged(EffectNode* node, EffectNode* parent_node);

  void AddCopyRequest(int node_id, std::unique_ptr<CopyOutputRequest> request);
  void PushCopyRequestsTo(EffectTree* other_tree);
  void TakeCopyRequestsAndTransformToSurface(
      int node_id,
      std::vector<std::unique_ptr<CopyOutputRequest>>* requests);
  bool HasCopyRequests() const;
  void ClearCopyRequests();

  int ClosestAncestorWithCopyRequest(int id) const;

  void AddMaskLayerId(int id);
  const std::vector<int>& mask_layer_ids() const { return mask_layer_ids_; }

  bool ContributesToDrawnSurface(int id);

  void ResetChangeTracking();

 private:
  void UpdateOpacities(EffectNode* node, EffectNode* parent_node);
  void UpdateIsDrawn(EffectNode* node, EffectNode* parent_node);
  void UpdateBackfaceVisibility(EffectNode* node, EffectNode* parent_node);

  // Stores copy requests, keyed by node id.
  std::unordered_multimap<int, std::unique_ptr<CopyOutputRequest>>
      copy_requests_;

  // Unsorted list of all mask layer ids that effect nodes refer to.
  std::vector<int> mask_layer_ids_;
};

class CC_EXPORT ScrollTree final : public PropertyTree<ScrollNode> {
 public:
  ScrollTree();
  ~ScrollTree();

  ScrollTree& operator=(const ScrollTree& from);
  bool operator==(const ScrollTree& other) const;

  void clear();

  typedef std::unordered_map<int, scoped_refptr<SyncedScrollOffset>>
      ScrollOffsetMap;
  typedef std::unordered_map<int, bool> ScrollbarsEnabledMap;

  gfx::ScrollOffset MaxScrollOffset(int scroll_node_id) const;
  void OnScrollOffsetAnimated(int layer_id,
                              int transform_tree_index,
                              int scroll_tree_index,
                              const gfx::ScrollOffset& scroll_offset,
                              LayerTreeImpl* layer_tree_impl);
  gfx::Size scroll_clip_layer_bounds(int scroll_node_id) const;
  ScrollNode* CurrentlyScrollingNode();
  const ScrollNode* CurrentlyScrollingNode() const;
#if DCHECK_IS_ON()
  int CurrentlyScrollingNodeId() const;
#endif
  void set_currently_scrolling_node(int scroll_node_id);
  gfx::Transform ScreenSpaceTransform(int scroll_node_id) const;

  const gfx::ScrollOffset current_scroll_offset(int layer_id) const;
  void CollectScrollDeltas(ScrollAndScaleSet* scroll_info,
                           int inner_viewport_layer_id);
  void UpdateScrollOffsetMap(ScrollOffsetMap* new_scroll_offset_map,
                             LayerTreeImpl* layer_tree_impl);
  ScrollOffsetMap& scroll_offset_map();
  const ScrollOffsetMap& scroll_offset_map() const;
  void ApplySentScrollDeltasFromAbortedCommit();
  bool SetBaseScrollOffset(int layer_id,
                           const gfx::ScrollOffset& scroll_offset);
  bool SetScrollOffset(int layer_id, const gfx::ScrollOffset& scroll_offset);
  void SetScrollOffsetClobberActiveValue(int layer_id) {
    synced_scroll_offset(layer_id)->set_clobber_active_value();
  }
  bool UpdateScrollOffsetBaseForTesting(int layer_id,
                                        const gfx::ScrollOffset& offset);
  bool SetScrollOffsetDeltaForTesting(int layer_id,
                                      const gfx::Vector2dF& delta);
  const gfx::ScrollOffset GetScrollOffsetBaseForTesting(int layer_id) const;
  const gfx::ScrollOffset GetScrollOffsetDeltaForTesting(int layer_id) const;
  void CollectScrollDeltasForTesting();

  void DistributeScroll(ScrollNode* scroll_node, ScrollState* scroll_state);
  gfx::Vector2dF ScrollBy(ScrollNode* scroll_node,
                          const gfx::Vector2dF& scroll,
                          LayerTreeImpl* layer_tree_impl);
  gfx::ScrollOffset ClampScrollOffsetToLimits(gfx::ScrollOffset offset,
                                              ScrollNode* scroll_node) const;

 private:
  int currently_scrolling_node_id_;
  ScrollOffsetMap layer_id_to_scroll_offset_map_;
  ScrollbarsEnabledMap layer_id_to_scrollbars_enabled_map_;

  SyncedScrollOffset* synced_scroll_offset(int layer_id);
  const SyncedScrollOffset* synced_scroll_offset(int layer_id) const;
  gfx::ScrollOffset PullDeltaForMainThread(SyncedScrollOffset* scroll_offset);
  void UpdateScrollOffsetMapEntry(int key,
                                  ScrollOffsetMap* new_scroll_offset_map,
                                  LayerTreeImpl* layer_tree_impl);
};

struct AnimationScaleData {
  // Variable used to invalidate cached animation scale data when transform tree
  // updates.
  int update_number;

  // Current animations, considering only scales at keyframes not including the
  // starting keyframe of each animation.
  float local_maximum_animation_target_scale;

  // The maximum scale that this node's |local| transform will have during
  // current animatons, considering only the starting scale of each animation.
  float local_starting_animation_scale;

  // The maximum scale that this node's |to_target| transform will have during
  // current animations, considering only scales at keyframes not incuding the
  // starting keyframe of each animation.
  float combined_maximum_animation_target_scale;

  // The maximum scale that this node's |to_target| transform will have during
  // current animations, considering only the starting scale of each animation.
  float combined_starting_animation_scale;

  bool to_screen_has_scale_animation;

  AnimationScaleData() {
    update_number = -1;
    local_maximum_animation_target_scale = 0.f;
    local_starting_animation_scale = 0.f;
    combined_maximum_animation_target_scale = 0.f;
    combined_starting_animation_scale = 0.f;
    to_screen_has_scale_animation = false;
  }
};

struct CombinedAnimationScale {
  float maximum_animation_scale;
  float starting_animation_scale;

  CombinedAnimationScale(float maximum, float starting)
      : maximum_animation_scale(maximum), starting_animation_scale(starting) {}
  bool operator==(const CombinedAnimationScale& other) const {
    return maximum_animation_scale == other.maximum_animation_scale &&
           starting_animation_scale == other.starting_animation_scale;
  }
};

struct DrawTransforms {
  // We compute invertibility of a draw transforms lazily.
  // Might_be_invertible is true if we have not computed the inverse of either
  // to_target or from_target, or to_target / from_target is invertible.
  bool might_be_invertible;
  // From_valid is true if the from_target is already computed directly or
  // computed by inverting an invertible to_target.
  bool from_valid;
  // To_valid is true if to_target stores a valid result, similar to from_valid.
  bool to_valid;
  gfx::Transform from_target;
  gfx::Transform to_target;

  DrawTransforms(gfx::Transform from, gfx::Transform to)
      : might_be_invertible(true),
        from_valid(false),
        to_valid(false),
        from_target(from),
        to_target(to) {}
  bool operator==(const DrawTransforms& other) const {
    return from_valid == other.from_valid && to_valid == other.to_valid &&
           from_target == other.from_target && to_target == other.to_target;
  }
};

struct DrawTransformData {
  int update_number;
  int target_id;

  DrawTransforms transforms;

  // TODO(sunxd): Move screen space transforms here if it can improve
  // performance.
  DrawTransformData()
      : update_number(-1),
        target_id(-1),
        transforms(gfx::Transform(), gfx::Transform()) {}
};

struct PropertyTreesCachedData {
  int property_tree_update_number;
  std::vector<AnimationScaleData> animation_scales;
  mutable std::vector<std::vector<DrawTransformData>> draw_transforms;

  PropertyTreesCachedData();
  ~PropertyTreesCachedData();
};

class CC_EXPORT PropertyTrees final {
 public:
  PropertyTrees();
  PropertyTrees(const PropertyTrees& other) = delete;
  ~PropertyTrees();

  bool operator==(const PropertyTrees& other) const;
  PropertyTrees& operator=(const PropertyTrees& from);

  std::unordered_map<int, int> transform_id_to_index_map;
  std::unordered_map<int, int> effect_id_to_index_map;
  std::unordered_map<int, int> clip_id_to_index_map;
  std::unordered_map<int, int> scroll_id_to_index_map;
  enum TreeType { TRANSFORM, EFFECT, CLIP, SCROLL };

  std::vector<int> always_use_active_tree_opacity_effect_ids;
  TransformTree transform_tree;
  EffectTree effect_tree;
  ClipTree clip_tree;
  ScrollTree scroll_tree;
  bool needs_rebuild;
  bool non_root_surfaces_enabled;
  // Change tracking done on property trees needs to be preserved across commits
  // (when they are not rebuild). We cache a global bool which stores whether
  // we did any change tracking so that we can skip copying the change status
  // between property trees when this bool is false.
  bool changed;
  // We cache a global bool for full tree damages to avoid walking the entire
  // tree.
  // TODO(jaydasika): Changes to transform and effects that damage the entire
  // tree should be tracked by this bool. Currently, they are tracked by the
  // individual nodes.
  bool full_tree_damaged;
  int sequence_number;
  bool is_main_thread;
  bool is_active;

  void clear();

  void SetInnerViewportContainerBoundsDelta(gfx::Vector2dF bounds_delta);
  void SetOuterViewportContainerBoundsDelta(gfx::Vector2dF bounds_delta);
  void SetInnerViewportScrollBoundsDelta(gfx::Vector2dF bounds_delta);
  void PushOpacityIfNeeded(PropertyTrees* target_tree);
  void RemoveIdFromIdToIndexMaps(int id);
  bool IsInIdToIndexMap(TreeType tree_type, int id);
  void UpdateChangeTracking();
  void PushChangeTrackingTo(PropertyTrees* tree);
  void ResetAllChangeTracking();

  gfx::Vector2dF inner_viewport_container_bounds_delta() const {
    return inner_viewport_container_bounds_delta_;
  }

  gfx::Vector2dF outer_viewport_container_bounds_delta() const {
    return outer_viewport_container_bounds_delta_;
  }

  gfx::Vector2dF inner_viewport_scroll_bounds_delta() const {
    return inner_viewport_scroll_bounds_delta_;
  }

  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  CombinedAnimationScale GetAnimationScales(int transform_node_id,
                                            LayerTreeImpl* layer_tree_impl);
  void SetAnimationScalesForTesting(int transform_id,
                                    float maximum_animation_scale,
                                    float starting_animation_scale);

  bool GetToTarget(int transform_id,
                   int effect_id,
                   gfx::Transform* to_target) const;
  bool GetFromTarget(int transform_id,
                     int effect_id,
                     gfx::Transform* from_target) const;

  void ResetCachedData();
  void UpdateCachedNumber();
  gfx::Transform ToScreenSpaceTransformWithoutSurfaceContentsScale(
      int transform_id,
      int effect_id) const;

  bool ComputeTransformFromTarget(int transform_id,
                                  int effect_id,
                                  gfx::Transform* transform) const;

 private:
  gfx::Vector2dF inner_viewport_container_bounds_delta_;
  gfx::Vector2dF outer_viewport_container_bounds_delta_;
  gfx::Vector2dF inner_viewport_scroll_bounds_delta_;

  // GetDrawTransforms may change the value of cached_data_.
  DrawTransforms& GetDrawTransforms(int transform_id, int effect_id) const;
  DrawTransformData& FetchDrawTransformsDataFromCache(int transform_id,
                                                      int effect_id) const;

  PropertyTreesCachedData cached_data_;
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_H_
