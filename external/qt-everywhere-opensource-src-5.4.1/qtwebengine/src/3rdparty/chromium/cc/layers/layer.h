// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_H_
#define CC_LAYERS_LAYER_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/animation/layer_animation_value_provider.h"
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/layer_lists.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/paint_properties.h"
#include "cc/layers/render_surface.h"
#include "cc/output/filter_operations.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace gfx {
class BoxF;
}

namespace base {
namespace debug {
class ConvertableToTraceFormat;
}
}

namespace cc {

class Animation;
class AnimationDelegate;
struct AnimationEvent;
class CopyOutputRequest;
class LayerAnimationDelegate;
class LayerAnimationEventObserver;
class LayerClient;
class LayerImpl;
class LayerTreeHost;
class LayerTreeImpl;
class PriorityCalculator;
class RenderingStatsInstrumentation;
class ResourceUpdateQueue;
class ScrollbarLayerInterface;
struct AnimationEvent;
template <typename LayerType>
class OcclusionTracker;

// Base class for composited layers. Special layer types are derived from
// this class.
class CC_EXPORT Layer : public base::RefCounted<Layer>,
                        public LayerAnimationValueObserver,
                        public LayerAnimationValueProvider {
 public:
  typedef RenderSurfaceLayerList RenderSurfaceListType;
  typedef LayerList LayerListType;
  typedef RenderSurface RenderSurfaceType;

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
  // first), then the callback is called with a NULL/empty result.
  void RequestCopyOfOutput(scoped_ptr<CopyOutputRequest> request);
  bool HasCopyRequest() const {
    return !copy_requests_.empty();
  }

  virtual void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return background_color_; }
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

  virtual void SetNeedsDisplayRect(const gfx::RectF& dirty_rect);
  void SetNeedsDisplay() { SetNeedsDisplayRect(gfx::RectF(bounds())); }

  void SetOpacity(float opacity);
  float opacity() const { return opacity_; }
  bool OpacityIsAnimating() const;
  virtual bool OpacityCanAnimateOnImplThread() const;

  void SetBlendMode(SkXfermode::Mode blend_mode);
  SkXfermode::Mode blend_mode() const { return blend_mode_; }

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

  void SetIsContainerForFixedPositionLayers(bool container);
  bool IsContainerForFixedPositionLayers() const;

  void SetPositionConstraint(const LayerPositionConstraint& constraint);
  const LayerPositionConstraint& position_constraint() const {
    return position_constraint_;
  }

  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return transform_; }
  bool TransformIsAnimating() const;
  bool transform_is_invertible() const { return transform_is_invertible_; }

  void SetTransformOrigin(const gfx::Point3F&);
  gfx::Point3F transform_origin() { return transform_origin_; }

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

  DrawProperties<Layer>& draw_properties() { return draw_properties_; }
  const DrawProperties<Layer>& draw_properties() const {
    return draw_properties_;
  }

  // The following are shortcut accessors to get various information from
  // draw_properties_
  const gfx::Transform& draw_transform() const {
    return draw_properties_.target_space_transform;
  }
  const gfx::Transform& screen_space_transform() const {
    return draw_properties_.screen_space_transform;
  }
  float draw_opacity() const { return draw_properties_.opacity; }
  bool draw_opacity_is_animating() const {
    return draw_properties_.opacity_is_animating;
  }
  bool draw_transform_is_animating() const {
    return draw_properties_.target_space_transform_is_animating;
  }
  bool screen_space_transform_is_animating() const {
    return draw_properties_.screen_space_transform_is_animating;
  }
  bool screen_space_opacity_is_animating() const {
    return draw_properties_.screen_space_opacity_is_animating;
  }
  bool can_use_lcd_text() const { return draw_properties_.can_use_lcd_text; }
  bool is_clipped() const { return draw_properties_.is_clipped; }
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }
  gfx::Rect drawable_content_rect() const {
    return draw_properties_.drawable_content_rect;
  }
  gfx::Rect visible_content_rect() const {
    return draw_properties_.visible_content_rect;
  }
  Layer* render_target() {
    DCHECK(!draw_properties_.render_target ||
           draw_properties_.render_target->render_surface());
    return draw_properties_.render_target;
  }
  const Layer* render_target() const {
    DCHECK(!draw_properties_.render_target ||
           draw_properties_.render_target->render_surface());
    return draw_properties_.render_target;
  }
  RenderSurface* render_surface() const {
    return draw_properties_.render_surface.get();
  }
  int num_unclipped_descendants() const {
    return draw_properties_.num_unclipped_descendants;
  }

  void SetScrollOffset(gfx::Vector2d scroll_offset);
  gfx::Vector2d scroll_offset() const { return scroll_offset_; }
  void SetScrollOffsetFromImplSide(const gfx::Vector2d& scroll_offset);

  void SetScrollClipLayerId(int clip_layer_id);
  bool scrollable() const { return scroll_clip_layer_id_ != INVALID_ID; }

  void SetUserScrollable(bool horizontal, bool vertical);
  bool user_scrollable_horizontal() const {
    return user_scrollable_horizontal_;
  }
  bool user_scrollable_vertical() const { return user_scrollable_vertical_; }

  void SetShouldScrollOnMainThread(bool should_scroll_on_main_thread);
  bool should_scroll_on_main_thread() const {
    return should_scroll_on_main_thread_;
  }

  void SetHaveWheelEventHandlers(bool have_wheel_event_handlers);
  bool have_wheel_event_handlers() const { return have_wheel_event_handlers_; }

  void SetHaveScrollEventHandlers(bool have_scroll_event_handlers);
  bool have_scroll_event_handlers() const {
    return have_scroll_event_handlers_;
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

  void SetDrawCheckerboardForMissingTiles(bool checkerboard);
  bool draw_checkerboard_for_missing_tiles() const {
    return draw_checkerboard_for_missing_tiles_;
  }

  void SetForceRenderSurface(bool force_render_surface);
  bool force_render_surface() const { return force_render_surface_; }

  gfx::Vector2d ScrollDelta() const { return gfx::Vector2d(); }
  gfx::Vector2dF TotalScrollOffset() const {
    // Floating point to match the LayerImpl version.
    return scroll_offset() + ScrollDelta();
  }

  void SetDoubleSided(bool double_sided);
  bool double_sided() const { return double_sided_; }

  void SetShouldFlattenTransform(bool flatten);
  bool should_flatten_transform() const { return should_flatten_transform_; }

  bool Is3dSorted() const { return sorting_context_id_ != 0; }

  void set_use_parent_backface_visibility(bool use) {
    use_parent_backface_visibility_ = use;
  }
  bool use_parent_backface_visibility() const {
    return use_parent_backface_visibility_;
  }

  virtual void SetLayerTreeHost(LayerTreeHost* host);

  bool HasDelegatedContent() const { return false; }
  bool HasContributingDelegatedRenderPasses() const { return false; }

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

  // These methods typically need to be overwritten by derived classes.
  virtual bool DrawsContent() const;
  virtual void SavePaintProperties();
  // Returns true iff any resources were updated that need to be committed.
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion);
  virtual bool NeedMoreUpdates();
  virtual void SetIsMask(bool is_mask) {}
  virtual void ReduceMemoryUsage() {}
  virtual void OnOutputSurfaceCreated() {}
  virtual bool IsSuitableForGpuRasterization() const;

  virtual scoped_refptr<base::debug::ConvertableToTraceFormat> TakeDebugInfo();

  void SetLayerClient(LayerClient* client) { client_ = client; }

  virtual void PushPropertiesTo(LayerImpl* layer);

  void CreateRenderSurface();
  void ClearRenderSurface();
  void ClearRenderSurfaceLayerList();

  // The contents scale converts from logical, non-page-scaled pixels to target
  // pixels. The contents scale is 1 for the root layer as it is already in
  // physical pixels. By default contents scale is forced to be 1 except for
  // subclasses of ContentsScalingLayer.
  float contents_scale_x() const { return draw_properties_.contents_scale_x; }
  float contents_scale_y() const { return draw_properties_.contents_scale_y; }
  gfx::Size content_bounds() const { return draw_properties_.content_bounds; }

  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      float maximum_animation_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds);

  LayerTreeHost* layer_tree_host() { return layer_tree_host_; }
  const LayerTreeHost* layer_tree_host() const { return layer_tree_host_; }

  // Set the priority of all desired textures in this layer.
  virtual void SetTexturePriorities(const PriorityCalculator& priority_calc) {}

  bool AddAnimation(scoped_ptr<Animation> animation);
  void PauseAnimation(int animation_id, double time_offset);
  void RemoveAnimation(int animation_id);

  LayerAnimationController* layer_animation_controller() {
    return layer_animation_controller_.get();
  }
  void SetLayerAnimationControllerForTest(
      scoped_refptr<LayerAnimationController> controller);

  void set_layer_animation_delegate(AnimationDelegate* delegate) {
    layer_animation_controller_->set_layer_animation_delegate(delegate);
  }

  bool HasActiveAnimation() const;

  void AddLayerAnimationEventObserver(
      LayerAnimationEventObserver* animation_observer);
  void RemoveLayerAnimationEventObserver(
      LayerAnimationEventObserver* animation_observer);

  virtual Region VisibleContentOpaqueRegion() const;

  virtual ScrollbarLayerInterface* ToScrollbarLayer();

  gfx::Rect LayerRectToContentRect(const gfx::RectF& layer_rect) const;

  virtual skia::RefPtr<SkPicture> GetPicture() const;

  // Constructs a LayerImpl of the correct runtime type for this Layer type.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);

  bool NeedsDisplayForTesting() const { return !update_rect_.IsEmpty(); }
  void ResetNeedsDisplayForTesting() { update_rect_ = gfx::RectF(); }

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const;

  const PaintProperties& paint_properties() const {
    return paint_properties_;
  }

  // The scale at which contents should be rastered, to match the scale at
  // which they will drawn to the screen. This scale is a component of the
  // contents scale but does not include page/device scale factors.
  // TODO(danakj): This goes away when TiledLayer goes away.
  void set_raster_scale(float scale) { raster_scale_ = scale; }
  float raster_scale() const { return raster_scale_; }
  bool raster_scale_is_unknown() const { return raster_scale_ == 0.f; }

  virtual bool SupportsLCDText() const;

  void SetNeedsPushProperties();
  bool needs_push_properties() const { return needs_push_properties_; }
  bool descendant_needs_push_properties() const {
    return num_dependents_need_push_properties_ > 0;
  }
  void reset_needs_push_properties_for_testing() {
    needs_push_properties_ = false;
  }

  virtual void RunMicroBenchmark(MicroBenchmark* benchmark);

  void Set3dSortingContextId(int id);
  int sorting_context_id() const { return sorting_context_id_; }

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
  // Called when there's been a change in layer structure.  Implies both
  // SetNeedsUpdate and SetNeedsCommit, but not SetNeedsPushProperties.
  void SetNeedsFullTreeSync();

  // Called when the next commit should wait until the pending tree is activated
  // before finishing the commit and unblocking the main thread. Used to ensure
  // unused resources on the impl thread are returned before commit completes.
  void SetNextCommitWaitsForActivation();

  void AddDependentNeedsPushProperties();
  void RemoveDependentNeedsPushProperties();
  bool parent_should_know_need_push_properties() const {
    return needs_push_properties() || descendant_needs_push_properties();
  }

  bool IsPropertyChangeAllowed() const;

  // If this layer has a scroll parent, it removes |this| from its list of
  // scroll children.
  void RemoveFromScrollTree();

  // If this layer has a clip parent, it removes |this| from its list of clip
  // children.
  void RemoveFromClipTree();

  void reset_raster_scale_to_unknown() { raster_scale_ = 0.f; }

  // This flag is set when the layer needs to push properties to the impl
  // side.
  bool needs_push_properties_;

  // The number of direct children or dependent layers that need to be recursed
  // to in order for them or a descendent of them to push properties to the impl
  // side.
  int num_dependents_need_push_properties_;

  // Tracks whether this layer may have changed stacking order with its
  // siblings.
  bool stacking_order_changed_;

  // The update rect is the region of the compositor resource that was
  // actually updated by the compositor. For layers that may do updating
  // outside the compositor's control (i.e. plugin layers), this information
  // is not available and the update rect will remain empty.
  // Note this rect is in layer space (not content space).
  gfx::RectF update_rect_;

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

  void SetParent(Layer* layer);
  bool DescendantIsFixedToContainerLayer() const;

  // Returns the index of the child or -1 if not found.
  int IndexOfChild(const Layer* reference);

  // This should only be called from RemoveFromParent().
  void RemoveChildOrDependent(Layer* child);

  // LayerAnimationValueProvider implementation.
  virtual gfx::Vector2dF ScrollOffsetForAnimation() const OVERRIDE;

  // LayerAnimationValueObserver implementation.
  virtual void OnFilterAnimated(const FilterOperations& filters) OVERRIDE;
  virtual void OnOpacityAnimated(float opacity) OVERRIDE;
  virtual void OnTransformAnimated(const gfx::Transform& transform) OVERRIDE;
  virtual void OnScrollOffsetAnimated(
      const gfx::Vector2dF& scroll_offset) OVERRIDE;
  virtual void OnAnimationWaitingForDeletion() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;

  LayerList children_;
  Layer* parent_;

  // Layer instances have a weak pointer to their LayerTreeHost.
  // This pointer value is nil when a Layer is not in a tree and is
  // updated via SetLayerTreeHost() if a layer moves between trees.
  LayerTreeHost* layer_tree_host_;

  scoped_refptr<LayerAnimationController> layer_animation_controller_;

  // Layer properties.
  gfx::Size bounds_;

  gfx::Vector2d scroll_offset_;
  // This variable indicates which ancestor layer (if any) whose size,
  // transformed relative to this layer, defines the maximum scroll offset for
  // this layer.
  int scroll_clip_layer_id_;
  bool should_scroll_on_main_thread_ : 1;
  bool have_wheel_event_handlers_ : 1;
  bool have_scroll_event_handlers_ : 1;
  bool user_scrollable_horizontal_ : 1;
  bool user_scrollable_vertical_ : 1;
  bool is_root_for_isolated_group_ : 1;
  bool is_container_for_fixed_position_layers_ : 1;
  bool is_drawable_ : 1;
  bool hide_layer_and_subtree_ : 1;
  bool masks_to_bounds_ : 1;
  bool contents_opaque_ : 1;
  bool double_sided_ : 1;
  bool should_flatten_transform_ : 1;
  bool use_parent_backface_visibility_ : 1;
  bool draw_checkerboard_for_missing_tiles_ : 1;
  bool force_render_surface_ : 1;
  bool transform_is_invertible_ : 1;
  Region non_fast_scrollable_region_;
  Region touch_event_handler_region_;
  gfx::PointF position_;
  SkColor background_color_;
  float opacity_;
  SkXfermode::Mode blend_mode_;
  FilterOperations filters_;
  FilterOperations background_filters_;
  LayerPositionConstraint position_constraint_;
  Layer* scroll_parent_;
  scoped_ptr<std::set<Layer*> > scroll_children_;

  Layer* clip_parent_;
  scoped_ptr<std::set<Layer*> > clip_children_;

  gfx::Transform transform_;
  gfx::Point3F transform_origin_;

  // Replica layer used for reflections.
  scoped_refptr<Layer> replica_layer_;

  // Transient properties.
  float raster_scale_;

  LayerClient* client_;

  ScopedPtrVector<CopyOutputRequest> copy_requests_;

  base::Closure did_scroll_callback_;

  DrawProperties<Layer> draw_properties_;

  PaintProperties paint_properties_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_H_
