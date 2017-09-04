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
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/paint_properties.h"
#include "cc/output/filter_operations.h"
#include "cc/trees/element_id.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/target_property.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {

class CopyOutputRequest;
class LayerClient;
class LayerImpl;
class LayerTreeHost;
class LayerTreeHostCommon;
class LayerTreeImpl;
class MutatorHost;
class ScrollbarLayerInterface;

namespace proto {
class LayerNode;
class LayerProperties;
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

  int id() const { return inputs_.layer_id; }

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

  const LayerList& children() const { return inputs_.children; }
  Layer* child_at(size_t index) { return inputs_.children[index].get(); }

  // This requests the layer and its subtree be rendered and given to the
  // callback. If the copy is unable to be produced (the layer is destroyed
  // first), then the callback is called with a nullptr/empty result. If the
  // request's source property is set, any prior uncommitted requests having the
  // same source will be aborted.
  void RequestCopyOfOutput(std::unique_ptr<CopyOutputRequest> request);
  bool HasCopyRequest() const { return !inputs_.copy_requests.empty(); }

  void TakeCopyRequests(
      std::vector<std::unique_ptr<CopyOutputRequest>>* requests);

  virtual void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return inputs_.background_color; }
  void SetSafeOpaqueBackgroundColor(SkColor background_color);
  // If contents_opaque(), return an opaque color else return a
  // non-opaque color.  Tries to return background_color(), if possible.
  SkColor SafeOpaqueBackgroundColor() const;

  // A layer's bounds are in logical, non-page-scaled pixels (however, the
  // root layer's bounds are in physical pixels).
  void SetBounds(const gfx::Size& bounds);
  gfx::Size bounds() const { return inputs_.bounds; }

  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return inputs_.masks_to_bounds; }

  void SetMaskLayer(Layer* mask_layer);
  Layer* mask_layer() { return inputs_.mask_layer.get(); }
  const Layer* mask_layer() const { return inputs_.mask_layer.get(); }

  virtual void SetNeedsDisplayRect(const gfx::Rect& dirty_rect);
  void SetNeedsDisplay() { SetNeedsDisplayRect(gfx::Rect(bounds())); }

  virtual void SetOpacity(float opacity);
  float opacity() const { return inputs_.opacity; }
  float EffectiveOpacity() const;
  virtual bool OpacityCanAnimateOnImplThread() const;

  virtual bool AlwaysUseActiveTreeOpacity() const;

  void SetBlendMode(SkXfermode::Mode blend_mode);
  SkXfermode::Mode blend_mode() const { return inputs_.blend_mode; }

  void set_draw_blend_mode(SkXfermode::Mode blend_mode) {
    if (draw_blend_mode_ == blend_mode)
      return;
    draw_blend_mode_ = blend_mode;
    SetNeedsPushProperties();
  }
  SkXfermode::Mode draw_blend_mode() const { return draw_blend_mode_; }

  // A layer is root for an isolated group when it and all its descendants are
  // drawn over a black and fully transparent background, creating an isolated
  // group. It should be used along with SetBlendMode(), in order to restrict
  // layers within the group to blend with layers outside this group.
  void SetIsRootForIsolatedGroup(bool root);
  bool is_root_for_isolated_group() const {
    return inputs_.is_root_for_isolated_group;
  }

  void SetFilters(const FilterOperations& filters);
  const FilterOperations& filters() const { return inputs_.filters; }

  // Background filters are filters applied to what is behind this layer, when
  // they are viewed through non-opaque regions in this layer. They are used
  // through the WebLayer interface, and are not exposed to HTML.
  void SetBackgroundFilters(const FilterOperations& filters);
  const FilterOperations& background_filters() const {
    return inputs_.background_filters;
  }

  virtual void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return inputs_.contents_opaque; }

  void SetPosition(const gfx::PointF& position);
  gfx::PointF position() const { return inputs_.position; }

  // A layer that is a container for fixed position layers cannot be both
  // scrollable and have a non-identity transform.
  void SetIsContainerForFixedPositionLayers(bool container);
  bool IsContainerForFixedPositionLayers() const;

  gfx::Vector2dF FixedContainerSizeDelta() const {
    return gfx::Vector2dF();
  }

  void SetPositionConstraint(const LayerPositionConstraint& constraint);
  const LayerPositionConstraint& position_constraint() const {
    return inputs_.position_constraint;
  }

  void SetStickyPositionConstraint(
      const LayerStickyPositionConstraint& constraint);
  const LayerStickyPositionConstraint& sticky_position_constraint() const {
    return inputs_.sticky_position_constraint;
  }

  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return inputs_.transform; }

  void SetTransformOrigin(const gfx::Point3F&);
  gfx::Point3F transform_origin() const { return inputs_.transform_origin; }

  void SetScrollParent(Layer* parent);

  Layer* scroll_parent() { return inputs_.scroll_parent; }

  std::set<Layer*>* scroll_children() { return scroll_children_.get(); }
  const std::set<Layer*>* scroll_children() const {
    return scroll_children_.get();
  }

  void SetClipParent(Layer* ancestor);

  Layer* clip_parent() { return inputs_.clip_parent; }

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

  gfx::ScrollOffset scroll_offset() const { return inputs_.scroll_offset; }
  void SetScrollOffsetFromImplSide(const gfx::ScrollOffset& scroll_offset);

  void SetScrollClipLayerId(int clip_layer_id);
  bool scrollable() const { return inputs_.scroll_clip_layer_id != INVALID_ID; }
  Layer* scroll_clip_layer() const;

  void SetUserScrollable(bool horizontal, bool vertical);
  bool user_scrollable_horizontal() const {
    return inputs_.user_scrollable_horizontal;
  }
  bool user_scrollable_vertical() const {
    return inputs_.user_scrollable_vertical;
  }

  void AddMainThreadScrollingReasons(uint32_t main_thread_scrolling_reasons);
  void ClearMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons_to_clear);
  uint32_t main_thread_scrolling_reasons() const {
    return inputs_.main_thread_scrolling_reasons;
  }
  bool should_scroll_on_main_thread() const {
    return !!inputs_.main_thread_scrolling_reasons;
  }

  void SetNonFastScrollableRegion(const Region& non_fast_scrollable_region);
  const Region& non_fast_scrollable_region() const {
    return inputs_.non_fast_scrollable_region;
  }

  void SetTouchEventHandlerRegion(const Region& touch_event_handler_region);
  const Region& touch_event_handler_region() const {
    return inputs_.touch_event_handler_region;
  }

  void set_did_scroll_callback(const base::Closure& callback) {
    inputs_.did_scroll_callback = callback;
  }

  void SetForceRenderSurfaceForTesting(bool force_render_surface);
  bool force_render_surface_for_testing() const {
    return force_render_surface_for_testing_;
  }

  gfx::ScrollOffset CurrentScrollOffset() const {
    return inputs_.scroll_offset;
  }

  void SetDoubleSided(bool double_sided);
  bool double_sided() const { return inputs_.double_sided; }

  void SetShouldFlattenTransform(bool flatten);
  bool should_flatten_transform() const {
    return inputs_.should_flatten_transform;
  }

  bool Is3dSorted() const { return inputs_.sorting_context_id != 0; }

  void SetUseParentBackfaceVisibility(bool use);
  bool use_parent_backface_visibility() const {
    return inputs_.use_parent_backface_visibility;
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
  bool hide_layer_and_subtree() const { return inputs_.hide_layer_and_subtree; }

  void SetFiltersOrigin(const gfx::PointF& origin);
  gfx::PointF filters_origin() const { return inputs_.filters_origin; }

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
  virtual void didUpdateMainThreadScrollingReasons();

  void SetLayerClient(LayerClient* client) { inputs_.client = client; }

  virtual bool IsSnapped();

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
  virtual void ToLayerNodeProto(proto::LayerNode* proto) const;

  // This method is similar to PushPropertiesTo, but instead of pushing to
  // a LayerImpl, it pushes the properties to proto::LayerProperties. It is
  // called only on layers that have changed properties. The properties
  // themselves are pushed to proto::LayerProperties.
  virtual void ToLayerPropertiesProto(proto::LayerProperties* proto);

  LayerTreeHost* GetLayerTreeHostForTesting() const { return layer_tree_host_; }
  LayerTree* GetLayerTree() const;

  virtual ScrollbarLayerInterface* ToScrollbarLayer();

  virtual sk_sp<SkPicture> GetPicture() const;

  // Constructs a LayerImpl of the correct runtime type for this Layer type.
  virtual std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);

  bool NeedsDisplayForTesting() const { return !inputs_.update_rect.IsEmpty(); }
  void ResetNeedsDisplayForTesting() { inputs_.update_rect = gfx::Rect(); }

  const PaintProperties& paint_properties() const {
    return paint_properties_;
  }

  void SetNeedsPushProperties();
  void ResetNeedsPushPropertiesForTesting();

  virtual void RunMicroBenchmark(MicroBenchmark* benchmark);

  void Set3dSortingContextId(int id);
  int sorting_context_id() const { return inputs_.sorting_context_id; }

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

  void SetSubtreePropertyChanged();
  bool subtree_property_changed() const { return subtree_property_changed_; }

  void SetMayContainVideo(bool yes);

  void DidBeginTracing();

  int num_copy_requests_in_target_subtree();

  void SetElementId(ElementId id);
  ElementId element_id() const { return inputs_.element_id; }

  void SetMutableProperties(uint32_t properties);
  uint32_t mutable_properties() const { return inputs_.mutable_properties; }

  bool HasActiveAnimationForTesting() const;

  void SetHasWillChangeTransformHint(bool has_will_change);
  bool has_will_change_transform_hint() const {
    return inputs_.has_will_change_transform_hint;
  }

  // The preferred raster bounds are the ideal resolution at which to raster the
  // contents of this Layer's bitmap. This may not be the same size as the Layer
  // bounds, in cases where the contents have an "intrinsic" size that differs.
  // Consider for example an image with a given intrinsic size that is being
  // scaled into a Layer of a different size.
  void SetPreferredRasterBounds(const gfx::Size& preferred_Raster_bounds);
  bool has_preferred_raster_bounds() const {
    return inputs_.has_preferred_raster_bounds;
  }
  const gfx::Size& preferred_raster_bounds() const {
    return inputs_.preferred_raster_bounds;
  }
  void ClearPreferredRasterBounds();

  MutatorHost* GetMutatorHost() const;

  ElementListType GetElementTypeForAnimation() const;

  // Tests in remote mode need to explicitly set the layer id so it matches the
  // layer id for the corresponding Layer on the engine.
  void SetLayerIdForTesting(int id);

  void SetScrollbarsHiddenFromImplSide(bool hidden);

  const gfx::Rect& update_rect() const { return inputs_.update_rect; }

 protected:
  friend class LayerImpl;
  friend class TreeSynchronizer;
  virtual ~Layer();
  Layer();

  LayerTreeHost* layer_tree_host() { return layer_tree_host_; }

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

  // When true, the layer is about to perform an update. Any commit requests
  // will be handled implicitly after the update completes.
  bool ignore_set_needs_commit_;

 private:
  friend class base::RefCounted<Layer>;
  friend class LayerTreeHostCommon;
  friend class LayerTree;
  friend class LayerInternalsForTest;

  // Interactions with attached animations.
  gfx::ScrollOffset ScrollOffsetForAnimation() const;
  void OnFilterAnimated(const FilterOperations& filters);
  void OnOpacityAnimated(float opacity);
  void OnTransformAnimated(const gfx::Transform& transform);
  void OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset);

  void OnIsAnimatingChanged(const PropertyAnimationState& mask,
                            const PropertyAnimationState& state);

  bool FilterIsAnimating() const;
  bool TransformIsAnimating() const;
  bool ScrollOffsetAnimationWasInterrupted() const;
  bool HasOnlyTranslationTransforms() const;

  void AddScrollChild(Layer* child);
  void RemoveScrollChild(Layer* child);

  void AddClipChild(Layer* child);
  void RemoveClipChild(Layer* child);

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

  // Encapsulates all data, callbacks or interfaces received from the embedder.
  // TODO(khushalsagar): This is only valid when PropertyTrees are built
  // internally in cc. Update this for the SPv2 path where blink generates
  // PropertyTrees.
  struct Inputs {
    explicit Inputs(int layer_id);
    ~Inputs();

    int layer_id;

    LayerList children;

    // The update rect is the region of the compositor resource that was
    // actually updated by the compositor. For layers that may do updating
    // outside the compositor's control (i.e. plugin layers), this information
    // is not available and the update rect will remain empty.
    // Note this rect is in layer space (not content space).
    gfx::Rect update_rect;

    gfx::Size bounds;
    bool masks_to_bounds;

    scoped_refptr<Layer> mask_layer;

    float opacity;
    SkXfermode::Mode blend_mode;

    bool is_root_for_isolated_group : 1;

    bool contents_opaque : 1;

    gfx::PointF position;
    gfx::Transform transform;
    gfx::Point3F transform_origin;

    bool is_drawable : 1;

    bool double_sided : 1;
    bool should_flatten_transform : 1;

    // Layers that share a sorting context id will be sorted together in 3d
    // space.  0 is a special value that means this layer will not be sorted
    // and will be drawn in paint order.
    int sorting_context_id;

    bool use_parent_backface_visibility : 1;

    SkColor background_color;

    FilterOperations filters;
    FilterOperations background_filters;
    gfx::PointF filters_origin;

    gfx::ScrollOffset scroll_offset;

    // This variable indicates which ancestor layer (if any) whose size,
    // transformed relative to this layer, defines the maximum scroll offset
    // for this layer.
    int scroll_clip_layer_id;
    bool user_scrollable_horizontal : 1;
    bool user_scrollable_vertical : 1;

    uint32_t main_thread_scrolling_reasons;
    Region non_fast_scrollable_region;

    Region touch_event_handler_region;

    bool is_container_for_fixed_position_layers : 1;
    LayerPositionConstraint position_constraint;

    LayerStickyPositionConstraint sticky_position_constraint;

    ElementId element_id;

    uint32_t mutable_properties;

    Layer* scroll_parent;
    Layer* clip_parent;

    bool has_will_change_transform_hint : 1;
    bool has_preferred_raster_bounds : 1;

    bool hide_layer_and_subtree : 1;

    // The following elements can not and are not serialized.
    LayerClient* client;
    base::Closure did_scroll_callback;
    std::vector<std::unique_ptr<CopyOutputRequest>> copy_requests;

    gfx::Size preferred_raster_bounds;
  };

  Layer* parent_;

  // Layer instances have a weak pointer to their LayerTreeHost.
  // This pointer value is nil when a Layer is not in a tree and is
  // updated via SetLayerTreeHost() if a layer moves between trees.
  LayerTreeHost* layer_tree_host_;
  LayerTree* layer_tree_;

  Inputs inputs_;

  int num_descendants_that_draw_content_;
  int transform_tree_index_;
  int effect_tree_index_;
  int clip_tree_index_;
  int scroll_tree_index_;
  int property_tree_sequence_number_;
  gfx::Vector2dF offset_to_transform_parent_;
  bool should_flatten_transform_from_property_tree_ : 1;
  bool draws_content_ : 1;
  bool use_local_transform_for_backface_visibility_ : 1;
  bool should_check_backface_visibility_ : 1;
  bool force_render_surface_for_testing_ : 1;
  bool subtree_property_changed_ : 1;
  bool may_contain_video_ : 1;
  SkColor safe_opaque_background_color_;
  // draw_blend_mode may be different than blend_mode_,
  // when a RenderSurface re-parents the layer's blend_mode.
  SkXfermode::Mode draw_blend_mode_;
  std::unique_ptr<std::set<Layer*>> scroll_children_;

  std::unique_ptr<std::set<Layer*>> clip_children_;

  PaintProperties paint_properties_;

  // These all act like draw properties, so don't need push properties.
  gfx::Rect visible_layer_rect_;
  size_t num_unclipped_descendants_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_H_
