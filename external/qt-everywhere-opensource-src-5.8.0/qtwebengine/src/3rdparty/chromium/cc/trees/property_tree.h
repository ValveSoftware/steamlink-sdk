// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROPERTY_TREE_H_
#define CC_TREES_PROPERTY_TREE_H_

#include <stddef.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "cc/animation/element_id.h"
#include "cc/base/cc_export.h"
#include "cc/base/synced_property.h"
#include "cc/output/filter_operations.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {

namespace proto {
class ClipNodeData;
class EffectNodeData;
class PropertyTree;
class PropertyTrees;
class ScrollNodeData;
class TransformNodeData;
class TransformCachedNodeData;
class TransformTreeData;
class TreeNode;
}  // namespace proto

class CopyOutputRequest;
class LayerTreeImpl;
class RenderSurfaceImpl;
class ScrollState;
struct ScrollAndScaleSet;

// ------------------------------*IMPORTANT*---------------------------------
// Each class declared here has a corresponding proto defined in
// cc/proto/property_tree.proto. When making any changes to a class structure
// including addition/deletion/updation of a field, please also make the
// change to its proto and the ToProtobuf and FromProtobuf methods for that
// class.

typedef SyncedProperty<AdditionGroup<gfx::ScrollOffset>> SyncedScrollOffset;

template <typename T>
struct CC_EXPORT TreeNode {
  TreeNode() : id(-1), parent_id(-1), owner_id(-1), data() {}
  int id;
  int parent_id;
  int owner_id;
  T data;

  bool operator==(const TreeNode<T>& other) const;

  void ToProtobuf(proto::TreeNode* proto) const;
  void FromProtobuf(const proto::TreeNode& proto);

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

struct CC_EXPORT TransformNodeData {
  TransformNodeData();
  TransformNodeData(const TransformNodeData& other);
  ~TransformNodeData();

  // The local transform information is combined to form to_parent (ignoring
  // snapping) as follows:
  //
  //   to_parent = M_post_local * T_scroll * M_local * M_pre_local.
  //
  // The pre/post may seem odd when read LTR, but we multiply our points from
  // the right, so the pre_local matrix affects the result "first". This lines
  // up with the notions of pre/post used in skia and gfx::Transform.
  //
  // TODO(vollick): The values labeled with "will be moved..." take up a lot of
  // space, but are only necessary for animated or scrolled nodes (otherwise
  // we'll just use the baked to_parent). These values will be ultimately stored
  // directly on the transform/scroll display list items when that's possible,
  // or potentially in a scroll tree.
  //
  // TODO(vollick): will be moved when accelerated effects are implemented.
  gfx::Transform pre_local;
  gfx::Transform local;
  gfx::Transform post_local;

  gfx::Transform to_parent;

  // This is the node with respect to which source_offset is defined. This will
  // not be needed once layerization moves to cc, but is needed in order to
  // efficiently update the transform tree for changes to position in the layer
  // tree.
  int source_node_id;

  // This id determines which 3d rendering context the node is in. 0 is a
  // special value and indicates that the node is not in any 3d rendering
  // context.
  int sorting_context_id;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  bool needs_local_transform_update : 1;

  bool node_and_ancestors_are_animated_or_invertible : 1;

  bool is_invertible : 1;
  bool ancestors_are_invertible : 1;

  bool has_potential_animation : 1;
  bool is_currently_animating : 1;
  bool to_screen_is_potentially_animated : 1;
  bool has_only_translation_animations : 1;

  // Flattening, when needed, is only applied to a node's inherited transform,
  // never to its local transform.
  bool flattens_inherited_transform : 1;

  // This is true if the to_parent transform at every node on the path to the
  // root is flat.
  bool node_and_ancestors_are_flat : 1;

  // This is needed to know if a layer can use lcd text.
  bool node_and_ancestors_have_only_integer_translation : 1;

  bool scrolls : 1;

  bool needs_sublayer_scale : 1;

  // These are used to position nodes wrt the right or bottom of the inner or
  // outer viewport.
  bool affected_by_inner_viewport_bounds_delta_x : 1;
  bool affected_by_inner_viewport_bounds_delta_y : 1;
  bool affected_by_outer_viewport_bounds_delta_x : 1;
  bool affected_by_outer_viewport_bounds_delta_y : 1;

  // Layer scale factor is used as a fallback when we either cannot adjust
  // raster scale or if the raster scale cannot be extracted from the screen
  // space transform. For layers in the subtree of the page scale layer, the
  // layer scale factor should include the page scale factor.
  bool in_subtree_of_page_scale_layer : 1;

  // We need to track changes to to_screen transform to compute the damage rect.
  bool transform_changed : 1;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  float post_local_scale_factor;

  gfx::Vector2dF sublayer_scale;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  gfx::ScrollOffset scroll_offset;

  // We scroll snap where possible, but this means fixed-pos elements must be
  // adjusted.  This value stores the snapped amount for this purpose.
  gfx::Vector2dF scroll_snap;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  gfx::Vector2dF source_offset;
  gfx::Vector2dF source_to_parent;

  bool operator==(const TransformNodeData& other) const;

  void set_to_parent(const gfx::Transform& transform) {
    to_parent = transform;
    is_invertible = to_parent.IsInvertible();
  }

  void update_pre_local_transform(const gfx::Point3F& transform_origin);

  void update_post_local_transform(const gfx::PointF& position,
                                   const gfx::Point3F& transform_origin);

  void ToProtobuf(proto::TreeNode* proto) const;
  void FromProtobuf(const proto::TreeNode& proto);

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

// TODO(sunxd): move this into PropertyTrees::cached_data_.
struct CC_EXPORT TransformCachedNodeData {
  TransformCachedNodeData();
  TransformCachedNodeData(const TransformCachedNodeData& other);
  ~TransformCachedNodeData();

  gfx::Transform from_target;
  gfx::Transform to_target;
  gfx::Transform from_screen;
  gfx::Transform to_screen;
  int target_id;
  // This id is used for all content that draws into a render surface associated
  // with this transform node.
  int content_target_id;

  bool operator==(const TransformCachedNodeData& other) const;

  void ToProtobuf(proto::TransformCachedNodeData* proto) const;
  void FromProtobuf(const proto::TransformCachedNodeData& proto);
};

typedef TreeNode<TransformNodeData> TransformNode;

struct CC_EXPORT ClipNodeData {
  ClipNodeData();
  ClipNodeData(const ClipNodeData& other);

  // The clip rect that this node contributes, expressed in the space of its
  // transform node.
  gfx::RectF clip;

  // Clip nodes are uses for two reasons. First, they are used for determining
  // which parts of each layer are visible. Second, they are used for
  // determining whether a clip needs to be applied when drawing a layer, and if
  // so, the rect that needs to be used. These can be different since not all
  // clips need to be applied directly to each layer. For example, a layer is
  // implicitly clipped by the bounds of its target render surface and by clips
  // applied to this surface. |combined_clip_in_target_space| is used for
  // computing visible rects, and |clip_in_target_space| is used for computing
  // clips applied at draw time. Both rects are expressed in the space of the
  // target transform node, and may include clips contributed by ancestors.
  gfx::RectF combined_clip_in_target_space;
  gfx::RectF clip_in_target_space;

  // The id of the transform node that defines the clip node's local space.
  int transform_id;

  // The id of the transform node that defines the clip node's target space.
  int target_id;

  // Whether this node contributes a new clip (that is, whether |clip| needs to
  // be applied), rather than only inheriting ancestor clips.
  bool applies_local_clip : 1;

  // When true, |clip_in_target_space| does not include clips from ancestor
  // nodes.
  bool layer_clipping_uses_only_local_clip : 1;

  // True if target surface needs to be drawn with a clip applied.
  bool target_is_clipped : 1;

  // True if layers with this clip tree node need to be drawn with a clip
  // applied.
  bool layers_are_clipped : 1;
  bool layers_are_clipped_when_surfaces_disabled : 1;

  // Nodes that correspond to unclipped surfaces disregard ancestor clips.
  bool resets_clip : 1;

  bool operator==(const ClipNodeData& other) const;

  void ToProtobuf(proto::TreeNode* proto) const;
  void FromProtobuf(const proto::TreeNode& proto);
  void AsValueInto(base::trace_event::TracedValue* value) const;
};

typedef TreeNode<ClipNodeData> ClipNode;

struct CC_EXPORT EffectNodeData {
  EffectNodeData();
  EffectNodeData(const EffectNodeData& other);

  float opacity;
  float screen_space_opacity;

  FilterOperations background_filters;

  bool has_render_surface;
  RenderSurfaceImpl* render_surface;
  bool has_copy_request;
  bool hidden_by_backface_visibility;
  bool double_sided;
  bool is_drawn;
  // TODO(jaydasika) : Delete this after implementation of
  // SetHideLayerAndSubtree is cleaned up. (crbug.com/595843)
  bool subtree_hidden;
  bool has_potential_opacity_animation;
  bool is_currently_animating_opacity;
  // We need to track changes to effects on the compositor to compute damage
  // rect.
  bool effect_changed;
  int num_copy_requests_in_subtree;
  bool has_unclipped_descendants;
  int transform_id;
  int clip_id;
  // Effect node id of which this effect contributes to.
  int target_id;
  int mask_layer_id;
  int replica_layer_id;
  int replica_mask_layer_id;

  bool operator==(const EffectNodeData& other) const;

  void ToProtobuf(proto::TreeNode* proto) const;
  void FromProtobuf(const proto::TreeNode& proto);
  void AsValueInto(base::trace_event::TracedValue* value) const;
};

typedef TreeNode<EffectNodeData> EffectNode;

struct CC_EXPORT ScrollNodeData {
  ScrollNodeData();
  ScrollNodeData(const ScrollNodeData& other);

  bool scrollable;
  uint32_t main_thread_scrolling_reasons;
  bool contains_non_fast_scrollable_region;
  gfx::Size scroll_clip_layer_bounds;
  gfx::Size bounds;
  bool max_scroll_offset_affected_by_page_scale;
  bool is_inner_viewport_scroll_layer;
  bool is_outer_viewport_scroll_layer;
  gfx::Vector2dF offset_to_transform_parent;
  bool should_flatten;
  bool user_scrollable_horizontal;
  bool user_scrollable_vertical;
  ElementId element_id;
  int transform_id;
  // Number of drawn layers pointing to this node or any of its descendants.
  int num_drawn_descendants;

  bool operator==(const ScrollNodeData& other) const;

  void ToProtobuf(proto::TreeNode* proto) const;
  void FromProtobuf(const proto::TreeNode& proto);
  void AsValueInto(base::trace_event::TracedValue* value) const;
};

typedef TreeNode<ScrollNodeData> ScrollNode;

class PropertyTrees;

template <typename T>
class CC_EXPORT PropertyTree {
 public:
  PropertyTree();
  PropertyTree(const PropertyTree& other) = delete;
  ~PropertyTree();

  bool operator==(const PropertyTree<T>& other) const;

  int Insert(const T& tree_node, int parent_id);

  T* Node(int i) {
    // TODO(vollick): remove this.
    CHECK(i < static_cast<int>(nodes_.size()));
    return i > -1 ? &nodes_[i] : nullptr;
  }
  const T* Node(int i) const {
    // TODO(vollick): remove this.
    CHECK(i < static_cast<int>(nodes_.size()));
    return i > -1 ? &nodes_[i] : nullptr;
  }

  T* parent(const T* t) { return Node(t->parent_id); }
  const T* parent(const T* t) const { return Node(t->parent_id); }

  T* back() { return size() ? &nodes_[nodes_.size() - 1] : nullptr; }
  const T* back() const {
    return size() ? &nodes_[nodes_.size() - 1] : nullptr;
  }

  void clear();
  size_t size() const { return nodes_.size(); }

  void set_needs_update(bool needs_update) { needs_update_ = needs_update; }
  bool needs_update() const { return needs_update_; }

  std::vector<T>& nodes() { return nodes_; }
  const std::vector<T>& nodes() const { return nodes_; }

  int next_available_id() const { return static_cast<int>(size()); }

  void ToProtobuf(proto::PropertyTree* proto) const;
  void FromProtobuf(const proto::PropertyTree& proto,
                    std::unordered_map<int, int>* node_id_to_index_map);

  void SetPropertyTrees(PropertyTrees* property_trees) {
    property_trees_ = property_trees;
  }
  PropertyTrees* property_trees() const { return property_trees_; }

  void AsValueInto(base::trace_event::TracedValue* value) const;

 private:
  std::vector<T> nodes_;

  bool needs_update_;
  PropertyTrees* property_trees_;
};

class CC_EXPORT TransformTree final : public PropertyTree<TransformNode> {
 public:
  TransformTree();
  ~TransformTree();

  bool operator==(const TransformTree& other) const;

  int Insert(const TransformNode& tree_node, int parent_id);

  void clear();

  // Computes the change of basis transform from node |source_id| to |dest_id|.
  // The function returns false iff the inverse of a singular transform was
  // used (and the result should, therefore, not be trusted). Transforms may
  // be computed between any pair of nodes that have an ancestor/descendant
  // relationship. Transforms between other pairs of nodes may only be computed
  // if the following condition holds: let id1 the larger id and let id2 be the
  // other id; then the nearest ancestor of node id1 whose id is smaller than
  // id2 is the lowest common ancestor of the pair of nodes, and the transform
  // from this lowest common ancestor to node id2 is only a 2d translation.
  bool ComputeTransform(int source_id,
                        int dest_id,
                        gfx::Transform* transform) const;

  // Computes the change of basis transform from node |source_id| to |dest_id|,
  // including any sublayer scale at |dest_id|.  The function returns false iff
  // the inverse of a singular transform was used (and the result should,
  // therefore, not be trusted).
  bool ComputeTransformWithDestinationSublayerScale(
      int source_id,
      int dest_id,
      gfx::Transform* transform) const;

  // Computes the change of basis transform from node |source_id| to |dest_id|,
  // including any sublayer scale at |source_id|.  The function returns false
  // iff the inverse of a singular transform was used (and the result should,
  // therefore, not be trusted).
  bool ComputeTransformWithSourceSublayerScale(int source_id,
                                               int dest_id,
                                               gfx::Transform* transform) const;

  // Returns true iff the nodes indexed by |source_id| and |dest_id| are 2D axis
  // aligned with respect to one another.
  bool Are2DAxisAligned(int source_id, int dest_id) const;

  void ResetChangeTracking();
  // Updates the parent, target, and screen space transforms and snapping.
  void UpdateTransforms(int id);
  void UpdateTransformChanged(TransformNode* node,
                              TransformNode* parent_node,
                              TransformNode* source_node);
  void UpdateNodeAndAncestorsAreAnimatedOrInvertible(
      TransformNode* node,
      TransformNode* parent_node);

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

  void SetDeviceTransform(const gfx::Transform& transform,
                          gfx::PointF root_position);
  void SetDeviceTransformScaleFactor(const gfx::Transform& transform);
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

  const gfx::Transform& FromTarget(int node_id) const;
  void SetFromTarget(int node_id, const gfx::Transform& transform);

  const gfx::Transform& ToTarget(int node_id) const;
  void SetToTarget(int node_id, const gfx::Transform& transform);

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

  gfx::Transform ToScreenSpaceTransformWithoutSublayerScale(int id) const;

  void ToProtobuf(proto::PropertyTree* proto) const;
  void FromProtobuf(const proto::PropertyTree& proto,
                    std::unordered_map<int, int>* node_id_to_index_map);

 private:
  // Returns true iff the node at |desc_id| is a descendant of the node at
  // |anc_id|.
  bool IsDescendant(int desc_id, int anc_id) const;

  // Computes the combined transform between |source_id| and |dest_id| and
  // returns false if the inverse of a singular transform was used. These two
  // nodes must be on the same ancestor chain.
  bool CombineTransformsBetween(int source_id,
                                int dest_id,
                                gfx::Transform* transform) const;

  // Computes the combined inverse transform between |source_id| and |dest_id|
  // and returns false if the inverse of a singular transform was used. These
  // two nodes must be on the same ancestor chain.
  bool CombineInversesBetween(int source_id,
                              int dest_id,
                              gfx::Transform* transform) const;

  void UpdateLocalTransform(TransformNode* node);
  void UpdateScreenSpaceTransform(TransformNode* node,
                                  TransformNode* parent_node,
                                  TransformNode* target_node);
  void UpdateSublayerScale(TransformNode* node);
  void UpdateTargetSpaceTransform(TransformNode* node,
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
};

class CC_EXPORT ClipTree final : public PropertyTree<ClipNode> {
 public:
  bool operator==(const ClipTree& other) const;

  void SetViewportClip(gfx::RectF viewport_rect);
  gfx::RectF ViewportClip();

  void ToProtobuf(proto::PropertyTree* proto) const;
  void FromProtobuf(const proto::PropertyTree& proto,
                    std::unordered_map<int, int>* node_id_to_index_map);
};

class CC_EXPORT EffectTree final : public PropertyTree<EffectNode> {
 public:
  EffectTree();
  ~EffectTree();

  EffectTree& operator=(const EffectTree& from);
  bool operator==(const EffectTree& other) const;

  void clear();

  float EffectiveOpacity(const EffectNode* node) const;

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

  void AddMaskOrReplicaLayerId(int id);
  const std::vector<int>& mask_replica_layer_ids() const {
    return mask_replica_layer_ids_;
  }

  bool ContributesToDrawnSurface(int id);

  void ResetChangeTracking();

  void ToProtobuf(proto::PropertyTree* proto) const;
  void FromProtobuf(const proto::PropertyTree& proto,
                    std::unordered_map<int, int>* node_id_to_index_map);

 private:
  void UpdateOpacities(EffectNode* node, EffectNode* parent_node);
  void UpdateIsDrawn(EffectNode* node, EffectNode* parent_node);
  void UpdateBackfaceVisibility(EffectNode* node, EffectNode* parent_node);

  // Stores copy requests, keyed by node id.
  std::unordered_multimap<int, std::unique_ptr<CopyOutputRequest>>
      copy_requests_;

  // Unsorted list of all mask, replica, and replica mask layer ids that
  // effect nodes refer to.
  std::vector<int> mask_replica_layer_ids_;
};

class CC_EXPORT ScrollTree final : public PropertyTree<ScrollNode> {
 public:
  ScrollTree();
  ~ScrollTree();

  ScrollTree& operator=(const ScrollTree& from);
  bool operator==(const ScrollTree& other) const;

  void ToProtobuf(proto::PropertyTree* proto) const;
  void FromProtobuf(const proto::PropertyTree& proto,
                    std::unordered_map<int, int>* node_id_to_index_map);

  void clear();

  typedef std::unordered_map<int, scoped_refptr<SyncedScrollOffset>>
      ScrollOffsetMap;

  gfx::ScrollOffset MaxScrollOffset(int scroll_node_id) const;
  gfx::Size scroll_clip_layer_bounds(int scroll_node_id) const;
  ScrollNode* CurrentlyScrollingNode();
  const ScrollNode* CurrentlyScrollingNode() const;
  void set_currently_scrolling_node(int scroll_node_id);
  gfx::Transform ScreenSpaceTransform(int scroll_node_id) const;

  const gfx::ScrollOffset current_scroll_offset(int layer_id) const;
  void CollectScrollDeltas(ScrollAndScaleSet* scroll_info);
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

struct PropertyTreesCachedData {
  int property_tree_update_number;
  std::vector<AnimationScaleData> animation_scales;

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

  void ToProtobuf(proto::PropertyTrees* proto) const;
  void FromProtobuf(const proto::PropertyTrees& proto);

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
  void ResetCachedData();
  void UpdateCachedNumber();

 private:
  gfx::Vector2dF inner_viewport_container_bounds_delta_;
  gfx::Vector2dF outer_viewport_container_bounds_delta_;
  gfx::Vector2dF inner_viewport_scroll_bounds_delta_;

  PropertyTreesCachedData cached_data_;
};

}  // namespace cc

#endif  // CC_TREES_PROPERTY_TREE_H_
