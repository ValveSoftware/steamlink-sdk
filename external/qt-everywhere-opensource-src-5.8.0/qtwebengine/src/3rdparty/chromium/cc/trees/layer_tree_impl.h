// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_IMPL_H_
#define CC_TREES_LAYER_TREE_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "cc/base/synced_property.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/renderer.h"
#include "cc/output/swap_promise.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/property_tree.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {

class ContextProvider;
class DebugRectHistory;
class FrameRateCounter;
class HeadsUpDisplayLayerImpl;
class ImageDecodeController;
class LayerExternalScrollOffsetListener;
class LayerScrollOffsetDelegate;
class LayerTreeDebugState;
class LayerTreeImpl;
class LayerTreeSettings;
class MemoryHistory;
class OutputSurface;
class PageScaleAnimation;
class PictureLayerImpl;
class TaskRunnerProvider;
class ResourceProvider;
class TileManager;
class UIResourceRequest;
class VideoFrameControllerClient;
struct PendingPageScaleAnimation;
struct RendererCapabilities;

typedef std::vector<UIResourceRequest> UIResourceRequestQueue;
typedef SyncedProperty<AdditionGroup<float>> SyncedTopControls;
typedef SyncedProperty<AdditionGroup<gfx::Vector2dF>> SyncedElasticOverscroll;

class CC_EXPORT LayerTreeImpl {
 public:
  // This is the number of times a fixed point has to be hit contiuously by a
  // layer to consider it as jittering.
  const int kFixedPointHitsThreshold = 3;
  static std::unique_ptr<LayerTreeImpl> create(
      LayerTreeHostImpl* layer_tree_host_impl,
      scoped_refptr<SyncedProperty<ScaleGroup>> page_scale_factor,
      scoped_refptr<SyncedTopControls> top_controls_shown_ratio,
      scoped_refptr<SyncedElasticOverscroll> elastic_overscroll) {
    return base::WrapUnique(
        new LayerTreeImpl(layer_tree_host_impl, page_scale_factor,
                          top_controls_shown_ratio, elastic_overscroll));
  }
  virtual ~LayerTreeImpl();

  void Shutdown();
  void ReleaseResources();
  void RecreateResources();

  // Methods called by the layer tree that pass-through or access LTHI.
  // ---------------------------------------------------------------------------
  const LayerTreeSettings& settings() const;
  const LayerTreeDebugState& debug_state() const;
  const RendererCapabilitiesImpl& GetRendererCapabilities() const;
  ContextProvider* context_provider() const;
  OutputSurface* output_surface() const;
  ResourceProvider* resource_provider() const;
  TileManager* tile_manager() const;
  ImageDecodeController* image_decode_controller() const;
  FrameRateCounter* frame_rate_counter() const;
  MemoryHistory* memory_history() const;
  gfx::Size device_viewport_size() const;
  DebugRectHistory* debug_rect_history() const;
  bool IsActiveTree() const;
  bool IsPendingTree() const;
  bool IsRecycleTree() const;
  bool IsSyncTree() const;
  LayerImpl* FindActiveTreeLayerById(int id);
  LayerImpl* FindPendingTreeLayerById(int id);
  bool PinchGestureActive() const;
  BeginFrameArgs CurrentBeginFrameArgs() const;
  base::TimeDelta CurrentBeginFrameInterval() const;
  gfx::Rect DeviceViewport() const;
  gfx::Size DrawViewportSize() const;
  const gfx::Rect ViewportRectForTilePriority() const;
  std::unique_ptr<ScrollbarAnimationController>
  CreateScrollbarAnimationController(int scroll_layer_id);
  void DidAnimateScrollOffset();
  bool use_gpu_rasterization() const;
  GpuRasterizationStatus GetGpuRasterizationStatus() const;
  bool create_low_res_tiling() const;
  bool RequiresHighResToDraw() const;
  bool SmoothnessTakesPriority() const;
  VideoFrameControllerClient* GetVideoFrameControllerClient() const;
  AnimationHost* animation_host() const {
    return layer_tree_host_impl_->animation_host();
  }

  // Tree specific methods exposed to layer-impl tree.
  // ---------------------------------------------------------------------------
  void SetNeedsRedraw();

  // Tracing methods.
  // ---------------------------------------------------------------------------
  void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const;
  void AsValueInto(base::trace_event::TracedValue* dict) const;

  // Other public methods
  // ---------------------------------------------------------------------------
  LayerImpl* root_layer_for_testing() {
    return layer_list_.empty() ? nullptr : layer_list_[0];
  }
  RenderSurfaceImpl* RootRenderSurface() const;
  bool LayerListIsEmpty() const;
  void SetRootLayerForTesting(std::unique_ptr<LayerImpl>);
  void SetRootLayerFromLayerListForTesting();
  void OnCanDrawStateChangedForTree();
  bool IsRootLayer(const LayerImpl* layer) const;
  std::unique_ptr<OwnedLayerImplList> DetachLayers();

  void SetPropertyTrees(PropertyTrees* property_trees);
  PropertyTrees* property_trees() { return &property_trees_; }

  void UpdatePropertyTreesForBoundsDelta();

  void PushPropertiesTo(LayerTreeImpl* tree_impl);

  void MoveChangeTrackingToLayers();

  LayerImplList::const_iterator begin() const;
  LayerImplList::const_iterator end() const;
  LayerImplList::reverse_iterator rbegin();
  LayerImplList::reverse_iterator rend();

  void AddToOpacityAnimationsMap(int id, float opacity);
  void AddToTransformAnimationsMap(int id, gfx::Transform transform);

  int source_frame_number() const { return source_frame_number_; }
  void set_source_frame_number(int frame_number) {
    source_frame_number_ = frame_number;
  }

  bool is_first_frame_after_commit() const {
    return source_frame_number_ != is_first_frame_after_commit_tracker_;
  }

  void set_is_first_frame_after_commit(bool is_first_frame_after_commit) {
    is_first_frame_after_commit_tracker_ =
        is_first_frame_after_commit ? -1 : source_frame_number_;
  }

  HeadsUpDisplayLayerImpl* hud_layer() { return hud_layer_; }
  void set_hud_layer(HeadsUpDisplayLayerImpl* layer_impl) {
    hud_layer_ = layer_impl;
  }

  LayerImpl* InnerViewportScrollLayer() const;
  // This function may return NULL, it is the caller's responsibility to check.
  LayerImpl* OuterViewportScrollLayer() const;
  gfx::ScrollOffset TotalScrollOffset() const;
  gfx::ScrollOffset TotalMaxScrollOffset() const;

  LayerImpl* InnerViewportContainerLayer() const;
  LayerImpl* OuterViewportContainerLayer() const;
  LayerImpl* CurrentlyScrollingLayer() const;
  int LastScrolledLayerId() const;
  void SetCurrentlyScrollingLayer(LayerImpl* layer);
  void ClearCurrentlyScrollingLayer();

  void SetViewportLayersFromIds(int overscroll_elasticity_layer,
                                int page_scale_layer_id,
                                int inner_viewport_scroll_layer_id,
                                int outer_viewport_scroll_layer_id);
  void ClearViewportLayers();
  LayerImpl* OverscrollElasticityLayer() {
    return LayerById(overscroll_elasticity_layer_id_);
  }
  LayerImpl* PageScaleLayer() { return LayerById(page_scale_layer_id_); }
  void ApplySentScrollAndScaleDeltasFromAbortedCommit();

  SkColor background_color() const { return background_color_; }
  void set_background_color(SkColor color) { background_color_ = color; }

  bool has_transparent_background() const {
    return has_transparent_background_;
  }
  void set_has_transparent_background(bool transparent) {
    has_transparent_background_ = transparent;
  }

  void UpdatePropertyTreeScrollingAndAnimationFromMainThread();
  void UpdatePropertyTreeScrollOffset(PropertyTrees* property_trees) {
    property_trees_.scroll_tree.UpdateScrollOffsetMap(
        &property_trees->scroll_tree.scroll_offset_map(), this);
  }
  void SetPageScaleOnActiveTree(float active_page_scale);
  void PushPageScaleFromMainThread(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);
  float current_page_scale_factor() const {
    return page_scale_factor()->Current(IsActiveTree());
  }
  float min_page_scale_factor() const { return min_page_scale_factor_; }
  float max_page_scale_factor() const { return max_page_scale_factor_; }

  float page_scale_delta() const { return page_scale_factor()->Delta(); }

  SyncedProperty<ScaleGroup>* page_scale_factor();
  const SyncedProperty<ScaleGroup>* page_scale_factor() const;

  void SetDeviceScaleFactor(float device_scale_factor);
  float device_scale_factor() const { return device_scale_factor_; }

  void set_painted_device_scale_factor(float painted_device_scale_factor) {
    painted_device_scale_factor_ = painted_device_scale_factor;
  }
  float painted_device_scale_factor() const {
    return painted_device_scale_factor_;
  }

  SyncedElasticOverscroll* elastic_overscroll() {
    return elastic_overscroll_.get();
  }
  const SyncedElasticOverscroll* elastic_overscroll() const {
    return elastic_overscroll_.get();
  }

  SyncedTopControls* top_controls_shown_ratio() {
    return top_controls_shown_ratio_.get();
  }
  const SyncedTopControls* top_controls_shown_ratio() const {
    return top_controls_shown_ratio_.get();
  }

  void SetElementIdsForTesting();

  // Updates draw properties and render surface layer list, as well as tile
  // priorities. Returns false if it was unable to update.  Updating lcd
  // text may cause invalidations, so should only be done after a commit.
  bool UpdateDrawProperties(bool update_lcd_text);
  void BuildPropertyTreesForTesting();
  void BuildLayerListAndPropertyTreesForTesting();

  void set_needs_update_draw_properties() {
    needs_update_draw_properties_ = true;
  }
  bool needs_update_draw_properties() const {
    return needs_update_draw_properties_;
  }

  bool is_in_resourceless_software_draw_mode() {
    return (layer_tree_host_impl_->GetDrawMode() ==
            DRAW_MODE_RESOURCELESS_SOFTWARE);
  }

  void set_needs_full_tree_sync(bool needs) { needs_full_tree_sync_ = needs; }
  bool needs_full_tree_sync() const { return needs_full_tree_sync_; }

  void ForceRedrawNextActivation() { next_activation_forces_redraw_ = true; }

  void set_has_ever_been_drawn(bool has_drawn) {
    has_ever_been_drawn_ = has_drawn;
  }
  bool has_ever_been_drawn() const { return has_ever_been_drawn_; }

  void set_ui_resource_request_queue(const UIResourceRequestQueue& queue);

  const LayerImplList& RenderSurfaceLayerList() const;
  const Region& UnoccludedScreenSpaceRegion() const;

  // These return the size of the root scrollable area and the size of
  // the user-visible scrolling viewport, in CSS layout coordinates.
  gfx::SizeF ScrollableSize() const;
  gfx::SizeF ScrollableViewportSize() const;

  gfx::Rect RootScrollLayerDeviceViewportBounds() const;

  LayerImpl* LayerById(int id) const;

  // TODO(vollick): this is deprecated. It is used by
  // animation/compositor-worker to look up layers to mutate, but in future, we
  // will update property trees.
  LayerImpl* LayerByElementId(ElementId element_id) const;
  void AddToElementMap(LayerImpl* layer);
  void RemoveFromElementMap(LayerImpl* layer);

  void AddLayerShouldPushProperties(LayerImpl* layer);
  void RemoveLayerShouldPushProperties(LayerImpl* layer);
  std::unordered_set<LayerImpl*>& LayersThatShouldPushProperties();
  bool LayerNeedsPushPropertiesForTesting(LayerImpl* layer);

  // These should be called by LayerImpl's ctor/dtor.
  void RegisterLayer(LayerImpl* layer);
  void UnregisterLayer(LayerImpl* layer);

  // These manage ownership of the LayerImpl.
  void AddLayer(std::unique_ptr<LayerImpl> layer);
  std::unique_ptr<LayerImpl> RemoveLayer(int id);

  size_t NumLayers();

  void DidBecomeActive();

  // Set on the active tree when the viewport size recently changed
  // and the active tree's size is now out of date.
  bool ViewportSizeInvalid() const;
  void SetViewportSizeInvalid();
  void ResetViewportSizeInvalid();

  // Used for accessing the task runner and debug assertions.
  TaskRunnerProvider* task_runner_provider() const;

  // Distribute the root scroll between outer and inner viewport scroll layer.
  // The outer viewport scroll layer scrolls first.
  bool DistributeRootScrollOffset(const gfx::ScrollOffset& root_offset);

  void ApplyScroll(ScrollNode* scroll_node, ScrollState* scroll_state) {
    layer_tree_host_impl_->ApplyScroll(scroll_node, scroll_state);
  }

  // Call this function when you expect there to be a swap buffer.
  // See swap_promise.h for how to use SwapPromise.
  //
  // A swap promise queued by QueueSwapPromise travels with the layer
  // information currently associated with the tree. For example, when
  // a pending tree is activated, the swap promise is passed to the
  // active tree along with the layer information. Similarly, when a
  // new activation overwrites layer information on the active tree,
  // queued swap promises are broken.
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  // Queue a swap promise, pinned to this tree. Pinned swap promises
  // may only be queued on the active tree.
  //
  // An active tree pinned swap promise will see only DidSwap() or
  // DidNotSwap(SWAP_FAILS). No DidActivate() will be seen because
  // that has already happened prior to queueing of the swap promise.
  //
  // Pinned active tree swap promises will not be broken prematurely
  // on the active tree if a new tree is activated.
  void QueuePinnedSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  // Take the |new_swap_promise| and append it to |swap_promise_list_|.
  void PassSwapPromises(
      std::vector<std::unique_ptr<SwapPromise>>* new_swap_promise);
  void FinishSwapPromises(CompositorFrameMetadata* metadata);
  void BreakSwapPromises(SwapPromise::DidNotSwapReason reason);

  void DidModifyTilePriorities();

  ResourceId ResourceIdForUIResource(UIResourceId uid) const;
  void ProcessUIResourceRequestQueue();

  bool IsUIResourceOpaque(UIResourceId uid) const;

  void RegisterPictureLayerImpl(PictureLayerImpl* layer);
  void UnregisterPictureLayerImpl(PictureLayerImpl* layer);
  const std::vector<PictureLayerImpl*>& picture_layers() const {
    return picture_layers_;
  }

  void RegisterScrollbar(ScrollbarLayerImplBase* scrollbar_layer);
  void UnregisterScrollbar(ScrollbarLayerImplBase* scrollbar_layer);
  ScrollbarSet ScrollbarsFor(int scroll_layer_id) const;

  void RegisterScrollLayer(LayerImpl* layer);
  void UnregisterScrollLayer(LayerImpl* layer);

  void AddSurfaceLayer(LayerImpl* layer);
  void RemoveSurfaceLayer(LayerImpl* layer);
  const LayerImplList& SurfaceLayers() const { return surface_layers_; }

  LayerImpl* FindFirstScrollingLayerOrScrollbarLayerThatIsHitByPoint(
      const gfx::PointF& screen_space_point);

  LayerImpl* FindLayerThatIsHitByPoint(const gfx::PointF& screen_space_point);

  LayerImpl* FindLayerThatIsHitByPointInTouchHandlerRegion(
      const gfx::PointF& screen_space_point);

  void RegisterSelection(const LayerSelection& selection);

  // Compute the current selection handle location and visbility with respect to
  // the viewport.
  void GetViewportSelection(Selection<gfx::SelectionBound>* selection);

  void set_top_controls_shrink_blink_size(bool shrink);
  bool top_controls_shrink_blink_size() const {
    return top_controls_shrink_blink_size_;
  }
  bool SetCurrentTopControlsShownRatio(float ratio);
  float CurrentTopControlsShownRatio() const {
    return top_controls_shown_ratio_->Current(IsActiveTree());
  }
  void set_top_controls_height(float top_controls_height);
  float top_controls_height() const { return top_controls_height_; }
  void PushTopControlsFromMainThread(float top_controls_shown_ratio);

  void SetPendingPageScaleAnimation(
      std::unique_ptr<PendingPageScaleAnimation> pending_animation);
  std::unique_ptr<PendingPageScaleAnimation> TakePendingPageScaleAnimation();

  void DidUpdateScrollOffset(int layer_id, int transform_id);
  void DidUpdateScrollState(int layer_id);

  bool IsAnimatingFilterProperty(const LayerImpl* layer) const;
  bool IsAnimatingOpacityProperty(const LayerImpl* layer) const;
  bool IsAnimatingTransformProperty(const LayerImpl* layer) const;

  bool HasPotentiallyRunningFilterAnimation(const LayerImpl* layer) const;
  bool HasPotentiallyRunningOpacityAnimation(const LayerImpl* layer) const;
  bool HasPotentiallyRunningTransformAnimation(const LayerImpl* layer) const;

  bool HasAnyAnimationTargetingProperty(const LayerImpl* layer,
                                        TargetProperty::Type property) const;

  bool AnimationsPreserveAxisAlignment(const LayerImpl* layer) const;
  bool HasOnlyTranslationTransforms(const LayerImpl* layer) const;

  bool MaximumTargetScale(const LayerImpl* layer, float* max_scale) const;
  bool AnimationStartScale(const LayerImpl* layer, float* start_scale) const;

  bool HasFilterAnimationThatInflatesBounds(const LayerImpl* layer) const;
  bool HasTransformAnimationThatInflatesBounds(const LayerImpl* layer) const;
  bool HasAnimationThatInflatesBounds(const LayerImpl* layer) const;

  bool FilterAnimationBoundsForBox(const LayerImpl* layer,
                                   const gfx::BoxF& box,
                                   gfx::BoxF* bounds) const;
  bool TransformAnimationBoundsForBox(const LayerImpl* layer,
                                      const gfx::BoxF& box,
                                      gfx::BoxF* bounds) const;
  void ScrollAnimationAbort(bool needs_completion);

  bool have_scroll_event_handlers() const {
    return have_scroll_event_handlers_;
  }
  void set_have_scroll_event_handlers(bool have_event_handlers) {
    have_scroll_event_handlers_ = have_event_handlers;
  }

  EventListenerProperties event_listener_properties(
      EventListenerClass event_class) const {
    return event_listener_properties_[static_cast<size_t>(event_class)];
  }
  void set_event_listener_properties(EventListenerClass event_class,
                                     EventListenerProperties event_properties) {
    event_listener_properties_[static_cast<size_t>(event_class)] =
        event_properties;
  }

  void ResetAllChangeTracking();

  void AddToLayerList(LayerImpl* layer);

  void ClearLayerList();

  void BuildLayerListForTesting();

 protected:
  explicit LayerTreeImpl(
      LayerTreeHostImpl* layer_tree_host_impl,
      scoped_refptr<SyncedProperty<ScaleGroup>> page_scale_factor,
      scoped_refptr<SyncedTopControls> top_controls_shown_ratio,
      scoped_refptr<SyncedElasticOverscroll> elastic_overscroll);
  float ClampPageScaleFactorToLimits(float page_scale_factor) const;
  void PushPageScaleFactorAndLimits(const float* page_scale_factor,
                                    float min_page_scale_factor,
                                    float max_page_scale_factor);
  bool SetPageScaleFactorLimits(float min_page_scale_factor,
                                float max_page_scale_factor);
  bool IsViewportLayerId(int id) const;
  void UpdateScrollbars(int scroll_layer_id, int clip_layer_id);
  void DidUpdatePageScale();
  void PushTopControls(const float* top_controls_shown_ratio);
  bool ClampTopControlsShownRatio();

  LayerTreeHostImpl* layer_tree_host_impl_;
  int source_frame_number_;
  int is_first_frame_after_commit_tracker_;
  LayerImpl* root_layer_for_testing_;
  HeadsUpDisplayLayerImpl* hud_layer_;
  PropertyTrees property_trees_;
  SkColor background_color_;
  bool has_transparent_background_;

  int last_scrolled_layer_id_;
  int overscroll_elasticity_layer_id_;
  int page_scale_layer_id_;
  int inner_viewport_scroll_layer_id_;
  int outer_viewport_scroll_layer_id_;

  LayerSelection selection_;

  scoped_refptr<SyncedProperty<ScaleGroup>> page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;

  float device_scale_factor_;
  float painted_device_scale_factor_;

  scoped_refptr<SyncedElasticOverscroll> elastic_overscroll_;

  std::unique_ptr<OwnedLayerImplList> layers_;
  LayerImplMap layer_id_map_;
  LayerImplList layer_list_;
  // Set of layers that need to push properties.
  std::unordered_set<LayerImpl*> layers_that_should_push_properties_;

  std::unordered_map<ElementId, LayerImpl*, ElementIdHash> element_layers_map_;

  std::unordered_map<int, float> opacity_animations_map_;
  std::unordered_map<int, gfx::Transform> transform_animations_map_;

  // Maps from clip layer ids to scroll layer ids.  Note that this only includes
  // the subset of clip layers that act as scrolling containers.  (This is
  // derived from LayerImpl::scroll_clip_layer_ and exists to avoid O(n) walks.)
  std::unordered_map<int, int> clip_scroll_map_;

  // Maps scroll layer ids to scrollbar layer ids.  For each scroll layer, there
  // may be 1 or 2 scrollbar layers (for vertical and horizontal).  (This is
  // derived from ScrollbarLayerImplBase::scroll_layer_id_ and exists to avoid
  // O(n) walks.)
  std::multimap<int, int> scrollbar_map_;

  std::vector<PictureLayerImpl*> picture_layers_;
  LayerImplList surface_layers_;

  // List of visible layers for the most recently prepared frame.
  LayerImplList render_surface_layer_list_;
  // After drawing the |render_surface_layer_list_| the areas in this region
  // would not be fully covered by opaque content.
  Region unoccluded_screen_space_region_;

  bool viewport_size_invalid_;
  bool needs_update_draw_properties_;

  // In impl-side painting mode, this is true when the tree may contain
  // structural differences relative to the active tree.
  bool needs_full_tree_sync_;

  bool next_activation_forces_redraw_;

  bool has_ever_been_drawn_;

  std::vector<std::unique_ptr<SwapPromise>> swap_promise_list_;
  std::vector<std::unique_ptr<SwapPromise>> pinned_swap_promise_list_;

  UIResourceRequestQueue ui_resource_request_queue_;

  bool have_scroll_event_handlers_;
  EventListenerProperties event_listener_properties_[static_cast<size_t>(
      EventListenerClass::kNumClasses)];

  // Whether or not Blink's viewport size was shrunk by the height of the top
  // controls at the time of the last layout.
  bool top_controls_shrink_blink_size_;
  float top_controls_height_;

  // The amount that the top controls are shown from 0 (hidden) to 1 (fully
  // shown).
  scoped_refptr<SyncedTopControls> top_controls_shown_ratio_;

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerTreeImpl);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_IMPL_H_
