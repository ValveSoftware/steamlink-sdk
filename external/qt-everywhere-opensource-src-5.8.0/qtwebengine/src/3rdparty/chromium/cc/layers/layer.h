// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_H_
#define CC_LAYERS_LAYER_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "cc/animation/element_id.h"
#include "cc/animation/target_property.h"
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/paint_properties.h"
#include "cc/output/filter_operations.h"
#include "cc/trees/property_tree.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace gfx {
class BoxF;
}

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {

class CopyOutputRequest;
class LayerAnimationEventObserver;
class LayerClient;
class LayerImpl;
class LayerTreeHost;
class LayerTreeHostCommon;
class LayerTreeImpl;
class LayerTreeSettings;
class RenderingStatsInstrumentation;
class ResourceUpdateQueue;
class ScrollbarLayerInterface;
class SimpleEnclosedRegion;

namespace proto {
class LayerNode;
class LayerProperties;
class LayerUpdate;
}  // namespace proto

// Base class for composited layers. Special layer types are derived from
// this class.
class CC_EXPORT Layer : public base::RefCounted<Layer> {
 public:
  using LayerListType = LayerList;
  using LayerIdMap = std::unordered_map<int, scoped_refptr<Layer>>;

  enum LayerIdLabels {
    INVALID_ID = -1,
  };

  static scoped_refptr<Layer> Create();

  int id() const { return layer_id_; }

  Layer* RootLayer();
  Layer* parent() { return parent_; }
  const Layer* parent() const { return parent_; }
  void AddChild(scoped_refptr<Layer> child);
  void InsertChild(scoped_refptr<Layer> child, size_t index);
  void ReplaceChild(Layer* reference, scoped_refptr<Layer> new_layer);
  void RemoveFromParent();
  void RemoveAllChildren();
  void SetChildren(const LayerList& children);
  bool HasAncestor(const Layer* ancestor) const;

  const LayerList& children() const { return children_; }
  Layer* child_at(size_t index) { return children_[index].get(); }

  // This requests the layer and its subtree be rendered and given to the
  // callback. If the copy is unable to be produced (the layer is destroyed
  // first), then the callback is called with a nullptr/empty result. If the
  // request's source property is set, any prior uncommitted requests having the
  // same source will be aborted.
  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> request);
  bool HasCopyRequest() const {
    return !copy_requests_.empty();
  }

  void TakeCopyRequests(
      std::vector<std::unique_ptr<CopyOutputRequest>>* requests);

  virtual void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return background_color_; }
  void SetSafeOpaqueBackgroundColor(SkColor background_color);
  // If contents_opaque(), return an opaque color else return a
  // non-opaque color.  Tries to return background_color(), if possible.
  SkColor SafeOpaqueBackgroundColor() const;

  // A layer's bounds are in logical, non-page-scaled pixels (however, the
  // root layer's bounds are in physical pixels).
  void SetBounds(const gfx::Size& bounds);
  gfx::Size bounds() const { return bounds_; }

  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return masks_to_bounds_; }

  void SetMaskLayer(Layer* mask_layer);
  Layer* mask_layer() { return mask_layer_.get(); }
  const Layer* mask_layer() const { return mask_layer_.get(); }

  virtual void SetNeedsDisplayRect(const gfx::Rect& dirty_rect);
  void SetNeedsDisplay() { SetNeedsDisplayRect(gfx::Rect(bounds())); }

  virtual void SetOpacity(float opacity);
  float opacity() const { return opacity_; }
  float EffectiveOpacity() const;
  bool OpacityIsAnimating() const;
  bool HasPotentiallyRunningOpacityAnimation() const;
  virtual bool OpacityCanAnimateOnImplThread() const;

  virtual bool AlwaysUseActiveTreeOpacity() const;

  void SetBlendMode(SkXfermode::Mode blend_mode);
  SkXfermode::Mode blend_mode() const { return blend_mode_; }

  void set_draw_blend_mode(SkXfermode::Mode blend_mode) {
    if (draw_blend_mode_ == blend_mode)
      return;
    draw_blend_mode_ = blend_mode;
    SetNeedsPushProperties();
  }
  SkXfermode::Mode draw_blend_mode() const { return draw_blend_mode_; }

  bool uses_default_blend_mode() const {
    return blend_mode_ == SkXfermode::kSrcOver_Mode;
  }

  // A layer is root for an isolated group when it and all its descendants are
  // drawn over a black and fully transparent background, creating an isolated
  // group. It should be used along with SetBlendMode(), in order to restrict
  // layers within the group to blend with layers outside this group.
  void SetIsRootForIsolatedGroup(bool root);
  bool is_root_for_isolated_group() const {
    return is_root_for_isolated_group_;
  }

  void SetFilters(const FilterOperations& filters);
  const FilterOperations& filters() const { return filters_; }
  bool FilterIsAnimating() const;
  bool HasPotentiallyRunningFilterAnimation() const;

  // Background filters are filters applied to what is behind this layer, when
  // they are viewed through non-opaque regions in this layer. They are used
  // through the WebLayer interface, and are not exposed to HTML.
  void SetBackgroundFilters(const FilterOperations& filters);
  const FilterOperations& background_filters() const {
    return background_filters_;
  }

  virtual void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return contents_opaque_; }

  void SetPosition(const gfx::PointF& position);
  gfx::PointF position() const { return position_; }

  // A layer that is a container for fixed position layers cannot be both
  // scrollable and have a non-identity transform.
  void SetIsContainerForFixedPositionLayers(bool container);
  bool IsContainerForFixedPositionLayers() const;

  gfx::Vector2dF FixedContainerSizeDelta() const {
    return gfx::Vector2dF();
  }

  void SetPositionConstraint(const LayerPositionConstraint& constraint);
  const LayerPositionConstraint& position_constraint() const {
    return position_constraint_;
  }

  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return transform_; }
  bool TransformIsAnimating() const;
  bool HasPotentiallyRunningTransformAnimation() const;
  bool HasOnlyTranslationTransforms() const;
  bool AnimationsPreserveAxisAlignment() const;

  bool MaximumTargetScale(float* max_scale) const;
  bool AnimationStartScale(float* start_scale) const;

  void SetTransformOrigin(const gfx::Point3F&);
  gfx::Point3F transform_origin() const { return transform_origin_; }

  bool HasAnyAnimationTargetingProperty(TargetProperty::Type property) const;

  bool ScrollOffsetAnimationWasInterrupted() const;

  void SetScrollParent(Layer* parent);

  Layer* scroll_parent() { return scroll_parent_; }
  const Layer* scroll_parent() const { return scroll_parent_; }

  void AddScrollChild(Layer* child);
  void RemoveScrollChild(Layer* child);

  std::set<Layer*>* scroll_children() { return scroll_children_.get(); }
  const std::set<Layer*>* scroll_children() const {
    return scroll_children_.get();
  }

  void SetClipParent(Layer* ancestor);

  Layer* clip_parent() { return clip_parent_; }
  const Layer* clip_parent() const {
    return clip_parent_;
  }

  void AddClipChild(Layer* child);
  void RemoveClipChild(Layer* child);

  std::set<Layer*>* clip_children() { return clip_children_.get(); }
  const std::set<Layer*>* clip_children() const {
    return clip_children_.get();
  }

  // TODO(enne): Fix style here (and everywhere) once LayerImpl does the same.
  gfx::Transform screen_space_transform() const;

  void set_num_unclipped_descendants(size_t descendants) {
    num_unclipped_descendants_ = descendants;
  }
  size_t num_unclipped_descendants() const {
    return num_unclipped_descendants_;
  }

  void SetScrollOffset(const gfx::ScrollOffset& scroll_offset);

  gfx::ScrollOffset scroll_offset() const { return scroll_offset_; }
  void SetScrollOffsetFromImplSide(const gfx::ScrollOffset& scroll_offset);

  void SetScrollClipLayerId(int clip_layer_id);
  bool scrollable() const { return scroll_clip_layer_id_ != INVALID_ID; }
  Layer* scroll_clip_layer() const;

  void SetUserScrollable(bool horizontal, bool vertical);
  bool user_scrollable_horizontal() const {
    return user_scrollable_horizontal_;
  }
  bool user_scrollable_vertical() const { return user_scrollable_vertical_; }

  void AddMainThreadScrollingReasons(uint32_t main_thread_scrolling_reasons);
  void ClearMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons_to_clear);
  uint32_t main_thread_scrolling_reasons() const {
    return main_thread_scrolling_reasons_;
  }
  bool should_scroll_on_main_thread() const {
    return !!main_thread_scrolling_reasons_;
  }

  void SetNonFastScrollableRegion(const Region& non_fast_scrollable_region);
  const Region& non_fast_scrollable_region() const {
    return non_fast_scrollable_region_;
  }

  void SetTouchEventHandlerRegion(const Region& touch_event_handler_region);
  const Region& touch_event_handler_region() const {
    return touch_event_handler_region_;
  }

  void set_did_scroll_callback(const base::Closure& callback) {
    did_scroll_callback_ = callback;
  }

  void SetForceRenderSurfaceForTesting(bool force_render_surface);
  bool force_render_surface_for_testing() const {
    return force_render_surface_for_testing_;
  }

  gfx::ScrollOffset CurrentScrollOffset() const { return scroll_offset_; }

  void SetDoubleSided(bool double_sided);
  bool double_sided() const { return double_sided_; }

  void SetShouldFlattenTransform(bool flatten);
  bool should_flatten_transform() const { return should_flatten_transform_; }

  bool Is3dSorted() const { return sorting_context_id_ != 0; }

  void SetUseParentBackfaceVisibility(bool use);
  bool use_parent_backface_visibility() const {
    return use_parent_backface_visibility_;
  }

  void SetUseLocalTransformForBackfaceVisibility(bool use_local);
  bool use_local_transform_for_backface_visibility() const {
    return use_local_transform_for_backface_visibility_;
  }

  void SetShouldCheckBackfaceVisibility(bool should_check_backface_visibility);
  bool should_check_backface_visibility() const {
    return should_check_backface_visibility_;
  }

  virtual void SetLayerTreeHost(LayerTreeHost* host);

  void SetIsDrawable(bool is_drawable);

  void SetHideLayerAndSubtree(bool hide);
  bool hide_layer_and_subtree() const { return hide_layer_and_subtree_; }

  void SetReplicaLayer(Layer* layer);
  Layer* replica_layer() { return replica_layer_.get(); }
  const Layer* replica_layer() const { return replica_layer_.get(); }

  bool has_mask() const { return !!mask_layer_.get(); }
  bool has_replica() const { return !!replica_layer_.get(); }
  bool replica_has_mask() const {
    return replica_layer_.get() &&
           (mask_layer_.get() || replica_layer_->mask_layer_.get());
  }

  int NumDescendantsThatDrawContent() const;

  // This is only virtual for tests.
  // TODO(awoloszyn): Remove this once we no longer need it for tests
  virtual bool DrawsContent() const;

  // This methods typically need to be overwritten by derived classes.
  // TODO(chrishtr): Blink no longer resizes anything during paint. We can
  // remove this.
  virtual void SavePaintProperties();
  // Returns true iff anything was updated that needs to be committed.
  virtual bool Update();
  virtual void SetIsMask(bool is_mask) {}
  virtual bool IsSuitableForGpuRasterization() const;

  virtual std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  TakeDebugInfo();

  void SetLayerClient(LayerClient* client) { client_ = client; }

  virtual void PushPropertiesTo(LayerImpl* layer);

  // Sets the type proto::LayerType that should be used for serialization
  // of the current layer by calling LayerNode::set_type(proto::LayerType).
  // TODO(nyquist): Start using a forward declared enum class when
  // https://github.com/google/protobuf/issues/67 has been fixed and rolled in.
  // This function would preferably instead return a proto::LayerType, but
  // since that is an enum (the protobuf library does not generate enum
  // classes), it can't be forward declared. We don't want to include
  // //cc/proto/layer.pb.h in this header file, as it requires that all
  // dependent targets would have to be given the config for how to include it.
  virtual void SetTypeForProtoSerialization(proto::LayerNode* proto) const;

  // Recursively iterate over this layer and all children and write the
  // hierarchical structure to the given LayerNode proto. In addition to the
  // structure itself, the Layer id and type is also written to facilitate
  // construction of the correct layer on the client.
  void ToLayerNodeProto(proto::LayerNode* proto) const;

  // Recursively iterate over this layer and all children and reset the
  // properties sent with the hierarchical structure in the LayerNode protos.
  // This must be done before deserializing the new LayerTree from the Layernode
  // protos.
  void ClearLayerTreePropertiesForDeserializationAndAddToMap(
      LayerIdMap* layer_map);

  // Recursively iterate over the given LayerNode proto and read the structure
  // into this node and its children. The |layer_map| should be used to look
  // for previously existing Layers, since they should be re-used between each
  // hierarchy update.
  void FromLayerNodeProto(const proto::LayerNode& proto,
                          const LayerIdMap& layer_map,
                          LayerTreeHost* layer_tree_host);

  // This method is similar to PushPropertiesTo, but instead of pushing to
  // a LayerImpl, it pushes the properties to proto::LayerProperties. It is
  // called only on layers that have changed properties. The properties
  // themselves are pushed to proto::LayerProperties.
  void ToLayerPropertiesProto(proto::LayerUpdate* layer_update);

  // Read all property values from the given LayerProperties object and update
  // the current layer. The values for |needs_push_properties_| and
  // |num_dependents_need_push_properties_| are always updated, but the rest
  // of |proto| is only read if |needs_push_properties_| is set.
  void FromLayerPropertiesProto(const proto::LayerProperties& proto);

  LayerTreeHost* layer_tree_host() { return layer_tree_host_; }
  const LayerTreeHost* layer_tree_host() const { return layer_tree_host_; }

  virtual ScrollbarLayerInterface* ToScrollbarLayer();

  virtual sk_sp<SkPicture> GetPicture() const;

  // Constructs a LayerImpl of the correct runtime type for this Layer type.
  virtual std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);

  bool NeedsDisplayForTesting() const { return !update_rect_.IsEmpty(); }
  void ResetNeedsDisplayForTesting() { update_rect_ = gfx::Rect(); }

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const;

  const PaintProperties& paint_properties() const {
    return paint_properties_;
  }

  void SetNeedsPushProperties();
  void ResetNeedsPushPropertiesForTesting();

  virtual void RunMicroBenchmark(MicroBenchmark* benchmark);

  void Set3dSortingContextId(int id);
  int sorting_context_id() const { return sorting_context_id_; }

  void set_property_tree_sequence_number(int sequence_number) {
    property_tree_sequence_number_ = sequence_number;
  }
  int property_tree_sequence_number() { return property_tree_sequence_number_; }

  void SetTransformTreeIndex(int index);
  int transform_tree_index() const;

  void SetClipTreeIndex(int index);
  int clip_tree_index() const;

  void SetEffectTreeIndex(int index);
  int effect_tree_index() const;

  void SetScrollTreeIndex(int index);
  int scroll_tree_index() const;

  void set_offset_to_transform_parent(gfx::Vector2dF offset) {
    if (offset_to_transform_parent_ == offset)
      return;
    offset_to_transform_parent_ = offset;
    SetNeedsPushProperties();
  }
  gfx::Vector2dF offset_to_transform_parent() const {
    return offset_to_transform_parent_;
  }

  // TODO(enne): This needs a different name.  It is a calculated value
  // from the property tree builder and not a synonym for "should
  // flatten transform".
  void set_should_flatten_transform_from_property_tree(bool should_flatten) {
    if (should_flatten_transform_from_property_tree_ == should_flatten)
      return;
    should_flatten_transform_from_property_tree_ = should_flatten;
    SetNeedsPushProperties();
  }
  bool should_flatten_transform_from_property_tree() const {
    return should_flatten_transform_from_property_tree_;
  }

  const gfx::Rect& visible_layer_rect_for_testing() const {
    return visible_layer_rect_;
  }
  void set_visible_layer_rect(const gfx::Rect& rect) {
    visible_layer_rect_ = rect;
  }

  void set_clip_rect(const gfx::Rect& rect) {}

  void SetSubtreePropertyChanged();
  bool subtree_property_changed() const { return subtree_property_changed_; }

  void SetLayerPropertyChanged();
  bool layer_property_changed() const { return layer_property_changed_; }

  void DidBeginTracing();

  int num_copy_requests_in_target_subtree();

  void SetElementId(ElementId id);
  ElementId element_id() const { return element_id_; }

  void SetMutableProperties(uint32_t properties);
  uint32_t mutable_properties() const { return mutable_properties_; }

  // Interactions with attached animations.
  gfx::ScrollOffset ScrollOffsetForAnimation() const;
  void OnFilterAnimated(const FilterOperations& filters);
  void OnOpacityAnimated(float opacity);
  void OnTransformAnimated(const gfx::Transform& transform);
  void OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset);
  void OnTransformIsCurrentlyAnimatingChanged(bool is_animating);
  void OnTransformIsPotentiallyAnimatingChanged(bool is_animating);
  void OnOpacityIsCurrentlyAnimatingChanged(bool is_currently_animating);
  void OnOpacityIsPotentiallyAnimatingChanged(bool has_potential_animation);
  bool HasActiveAnimationForTesting() const;

  void SetHasWillChangeTransformHint(bool has_will_change);
  bool has_will_change_transform_hint() const {
    return has_will_change_transform_hint_;
  }

 protected:
  friend class LayerImpl;
  friend class TreeSynchronizer;
  virtual ~Layer();
  Layer();

  // These SetNeeds functions are in order of severity of update:
  //
  // Called when this layer has been modified in some way, but isn't sure
  // that it needs a commit yet.  It needs CalcDrawProperties and UpdateLayers
  // before it knows whether or not a commit is required.
  void SetNeedsUpdate();
  // Called when a property has been modified in a way that the layer
  // knows immediately that a commit is required.  This implies SetNeedsUpdate
  // as well as SetNeedsPushProperties to push that property.
  void SetNeedsCommit();
  // This is identical to SetNeedsCommit, but the former requests a rebuild of
  // the property trees.
  void SetNeedsCommitNoRebuild();
  // Called when there's been a change in layer structure.  Implies both
  // SetNeedsUpdate and SetNeedsCommit, but not SetNeedsPushProperties.
  void SetNeedsFullTreeSync();

  // Called when the next commit should wait until the pending tree is activated
  // before finishing the commit and unblocking the main thread. Used to ensure
  // unused resources on the impl thread are returned before commit completes.
  void SetNextCommitWaitsForActivation();

  // Will recalculate whether the layer draws content and set draws_content_
  // appropriately.
  void UpdateDrawsContent(bool has_drawable_content);
  virtual bool HasDrawableContent() const;

  // Called when the layer's number of drawable descendants changes.
  void AddDrawableDescendants(int num);

  bool IsPropertyChangeAllowed() const;

  // Serialize all the necessary properties to be able to reconstruct this Layer
  // into proto::LayerProperties. This method is not marked as const
  // as some implementations need reset member fields, similarly to
  // PushPropertiesTo().
  virtual void LayerSpecificPropertiesToProto(proto::LayerProperties* proto);

  // Deserialize all the necessary properties from proto::LayerProperties into
  // this Layer.
  virtual void FromLayerSpecificPropertiesProto(
      const proto::LayerProperties& proto);

  // The update rect is the region of the compositor resource that was
  // actually updated by the compositor. For layers that may do updating
  // outside the compositor's control (i.e. plugin layers), this information
  // is not available and the update rect will remain empty.
  // Note this rect is in layer space (not content space).
  gfx::Rect update_rect_;

  scoped_refptr<Layer> mask_layer_;

  int layer_id_;

  // When true, the layer is about to perform an update. Any commit requests
  // will be handled implicitly after the update completes.
  bool ignore_set_needs_commit_;

  // Layers that share a sorting context id will be sorted together in 3d
  // space.  0 is a special value that means this layer will not be sorted and
  // will be drawn in paint order.
  int sorting_context_id_;

 private:
  friend class base::RefCounted<Layer>;
  friend class LayerSerializationTest;
  friend class LayerTreeHostCommon;

  void SetParent(Layer* layer);
  bool DescendantIsFixedToContainerLayer() const;

  // This should only be called from RemoveFromParent().
  void RemoveChildOrDependent(Layer* child);

  // If this layer has a scroll parent, it removes |this| from its list of
  // scroll children.
  void RemoveFromScrollTree();

  // If this layer has a clip parent, it removes |this| from its list of clip
  // children.
  void RemoveFromClipTree();

  // When we detach or attach layer to new LayerTreeHost, all property trees'
  // indices becomes invalid.
  void InvalidatePropertyTreesIndices();

  LayerList children_;
  Layer* parent_;

  // Layer instances have a weak pointer to their LayerTreeHost.
  // This pointer value is nil when a Layer is not in a tree and is
  // updated via SetLayerTreeHost() if a layer moves between trees.
  LayerTreeHost* layer_tree_host_;

  // Layer properties.
  gfx::Size bounds_;

  gfx::ScrollOffset scroll_offset_;
  // This variable indicates which ancestor layer (if any) whose size,
  // transformed relative to this layer, defines the maximum scroll offset for
  // this layer.
  int scroll_clip_layer_id_;
  int num_descendants_that_draw_content_;
  int transform_tree_index_;
  int effect_tree_index_;
  int clip_tree_index_;
  int scroll_tree_index_;
  int property_tree_sequence_number_;
  ElementId element_id_;
  uint32_t mutable_properties_;
  gfx::Vector2dF offset_to_transform_parent_;
  uint32_t main_thread_scrolling_reasons_;
  bool should_flatten_transform_from_property_tree_ : 1;
  bool user_scrollable_horizontal_ : 1;
  bool user_scrollable_vertical_ : 1;
  bool is_root_for_isolated_group_ : 1;
  bool is_container_for_fixed_position_layers_ : 1;
  bool is_drawable_ : 1;
  bool draws_content_ : 1;
  bool hide_layer_and_subtree_ : 1;
  bool masks_to_bounds_ : 1;
  bool contents_opaque_ : 1;
  bool double_sided_ : 1;
  bool should_flatten_transform_ : 1;
  bool use_parent_backface_visibility_ : 1;
  bool use_local_transform_for_backface_visibility_ : 1;
  bool should_check_backface_visibility_ : 1;
  bool force_render_surface_for_testing_ : 1;
  bool subtree_property_changed_ : 1;
  bool layer_property_changed_ : 1;
  bool has_will_change_transform_hint_ : 1;
  Region non_fast_scrollable_region_;
  Region touch_event_handler_region_;
  gfx::PointF position_;
  SkColor background_color_;
  SkColor safe_opaque_background_color_;
  float opacity_;
  SkXfermode::Mode blend_mode_;
  // draw_blend_mode may be different than blend_mode_,
  // when a RenderSurface re-parents the layer's blend_mode.
  SkXfermode::Mode draw_blend_mode_;
  FilterOperations filters_;
  FilterOperations background_filters_;
  LayerPositionConstraint position_constraint_;
  Layer* scroll_parent_;
  std::unique_ptr<std::set<Layer*>> scroll_children_;

  Layer* clip_parent_;
  std::unique_ptr<std::set<Layer*>> clip_children_;

  gfx::Transform transform_;
  gfx::Point3F transform_origin_;

  // Replica layer used for reflections.
  scoped_refptr<Layer> replica_layer_;

  LayerClient* client_;

  std::vector<std::unique_ptr<CopyOutputRequest>> copy_requests_;

  base::Closure did_scroll_callback_;

  PaintProperties paint_properties_;

  // These all act like draw properties, so don't need push properties.
  gfx::Rect visible_layer_rect_;
  size_t num_unclipped_descendants_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_H_
