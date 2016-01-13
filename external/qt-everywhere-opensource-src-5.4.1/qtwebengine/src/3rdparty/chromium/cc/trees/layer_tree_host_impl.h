// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_IMPL_H_
#define CC_TREES_LAYER_TREE_HOST_IMPL_H_

#include <list>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/base/cc_export.h"
#include "cc/debug/micro_benchmark_controller_impl.h"
#include "cc/input/input_handler.h"
#include "cc/input/layer_scroll_offset_delegate.h"
#include "cc/input/top_controls_manager_client.h"
#include "cc/layers/layer_lists.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/renderer.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/tile_manager.h"
#include "cc/scheduler/draw_result.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/rect.h"

namespace cc {

class CompletionEvent;
class CompositorFrameMetadata;
class DebugRectHistory;
class FrameRateCounter;
class LayerImpl;
class LayerTreeHostImplTimeSourceAdapter;
class LayerTreeImpl;
class MemoryHistory;
class PageScaleAnimation;
class PaintTimeCounter;
class PictureLayerImpl;
class RasterWorkerPool;
class RenderPassDrawQuad;
class RenderingStatsInstrumentation;
class ResourcePool;
class ScrollbarLayerImplBase;
class TextureMailboxDeleter;
class TopControlsManager;
class UIResourceBitmap;
class UIResourceRequest;
struct RendererCapabilitiesImpl;

// LayerTreeHost->Proxy callback interface.
class LayerTreeHostImplClient {
 public:
  virtual void UpdateRendererCapabilitiesOnImplThread() = 0;
  virtual void DidLoseOutputSurfaceOnImplThread() = 0;
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) = 0;
  virtual void SetEstimatedParentDrawTime(base::TimeDelta draw_time) = 0;
  virtual void SetMaxSwapsPendingOnImplThread(int max) = 0;
  virtual void DidSwapBuffersOnImplThread() = 0;
  virtual void DidSwapBuffersCompleteOnImplThread() = 0;
  virtual void BeginFrame(const BeginFrameArgs& args) = 0;
  virtual void OnCanDrawStateChanged(bool can_draw) = 0;
  virtual void NotifyReadyToActivate() = 0;
  // Please call these 3 functions through
  // LayerTreeHostImpl's SetNeedsRedraw(), SetNeedsRedrawRect() and
  // SetNeedsAnimate().
  virtual void SetNeedsRedrawOnImplThread() = 0;
  virtual void SetNeedsRedrawRectOnImplThread(const gfx::Rect& damage_rect) = 0;
  virtual void SetNeedsAnimateOnImplThread() = 0;
  virtual void DidInitializeVisibleTileOnImplThread() = 0;
  virtual void SetNeedsCommitOnImplThread() = 0;
  virtual void SetNeedsManageTilesOnImplThread() = 0;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> events) = 0;
  // Returns true if resources were deleted by this call.
  virtual bool ReduceContentsTextureMemoryOnImplThread(
      size_t limit_bytes,
      int priority_cutoff) = 0;
  virtual bool IsInsideDraw() = 0;
  virtual void RenewTreePriority() = 0;
  virtual void PostDelayedScrollbarFadeOnImplThread(
      const base::Closure& start_fade,
      base::TimeDelta delay) = 0;
  virtual void DidActivatePendingTree() = 0;
  virtual void DidManageTiles() = 0;

 protected:
  virtual ~LayerTreeHostImplClient() {}
};

// LayerTreeHostImpl owns the LayerImpl trees as well as associated rendering
// state.
class CC_EXPORT LayerTreeHostImpl
    : public InputHandler,
      public RendererClient,
      public TileManagerClient,
      public OutputSurfaceClient,
      public TopControlsManagerClient,
      public ScrollbarAnimationControllerClient,
      public base::SupportsWeakPtr<LayerTreeHostImpl> {
 public:
  static scoped_ptr<LayerTreeHostImpl> Create(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      Proxy* proxy,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      SharedBitmapManager* manager,
      int id);
  virtual ~LayerTreeHostImpl();

  // InputHandler implementation
  virtual void BindToClient(InputHandlerClient* client) OVERRIDE;
  virtual InputHandler::ScrollStatus ScrollBegin(
      const gfx::Point& viewport_point,
      InputHandler::ScrollInputType type) OVERRIDE;
  virtual bool ScrollBy(const gfx::Point& viewport_point,
                        const gfx::Vector2dF& scroll_delta) OVERRIDE;
  virtual bool ScrollVerticallyByPage(const gfx::Point& viewport_point,
                                      ScrollDirection direction) OVERRIDE;
  virtual void SetRootLayerScrollOffsetDelegate(
      LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate) OVERRIDE;
  virtual void OnRootLayerDelegatedScrollOffsetChanged() OVERRIDE;
  virtual void ScrollEnd() OVERRIDE;
  virtual InputHandler::ScrollStatus FlingScrollBegin() OVERRIDE;
  virtual void MouseMoveAt(const gfx::Point& viewport_point) OVERRIDE;
  virtual void PinchGestureBegin() OVERRIDE;
  virtual void PinchGestureUpdate(float magnify_delta,
                                  const gfx::Point& anchor) OVERRIDE;
  virtual void PinchGestureEnd() OVERRIDE;
  virtual void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                       bool anchor_point,
                                       float page_scale,
                                       base::TimeDelta duration) OVERRIDE;
  virtual void SetNeedsAnimate() OVERRIDE;
  virtual bool IsCurrentlyScrollingLayerAt(
      const gfx::Point& viewport_point,
      InputHandler::ScrollInputType type) OVERRIDE;
  virtual bool HaveTouchEventHandlersAt(
      const gfx::Point& viewport_port) OVERRIDE;
  virtual scoped_ptr<SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) OVERRIDE;

  // TopControlsManagerClient implementation.
  virtual void DidChangeTopControlsPosition() OVERRIDE;
  virtual bool HaveRootScrollLayer() const OVERRIDE;

  struct CC_EXPORT FrameData : public RenderPassSink {
    FrameData();
    virtual ~FrameData();
    scoped_ptr<base::Value> AsValue() const;

    std::vector<gfx::Rect> occluding_screen_space_rects;
    std::vector<gfx::Rect> non_occluding_screen_space_rects;
    RenderPassList render_passes;
    RenderPassIdHashMap render_passes_by_id;
    const LayerImplList* render_surface_layer_list;
    LayerImplList will_draw_layers;
    bool contains_incomplete_tile;
    bool has_no_damage;

    // RenderPassSink implementation.
    virtual void AppendRenderPass(scoped_ptr<RenderPass> render_pass) OVERRIDE;
  };

  virtual void BeginMainFrameAborted(bool did_handle);
  virtual void BeginCommit();
  virtual void CommitComplete();
  virtual void Animate(base::TimeTicks monotonic_time);
  virtual void UpdateAnimationState(bool start_ready_animations);
  void ActivateAnimations();
  void MainThreadHasStoppedFlinging();
  void UpdateBackgroundAnimateTicking(bool should_background_tick);
  void DidAnimateScrollOffset();
  void SetViewportDamage(const gfx::Rect& damage_rect);

  virtual void ManageTiles();

  // Returns DRAW_SUCCESS unless problems occured preparing the frame, and we
  // should try to avoid displaying the frame. If PrepareToDraw is called,
  // DidDrawAllLayers must also be called, regardless of whether DrawLayers is
  // called between the two.
  virtual DrawResult PrepareToDraw(FrameData* frame);
  virtual void DrawLayers(FrameData* frame, base::TimeTicks frame_begin_time);
  // Must be called if and only if PrepareToDraw was called.
  void DidDrawAllLayers(const FrameData& frame);

  const LayerTreeSettings& settings() const { return settings_; }

  // Evict all textures by enforcing a memory policy with an allocation of 0.
  void EvictTexturesForTesting();

  // When blocking, this prevents client_->NotifyReadyToActivate() from being
  // called. When disabled, it calls client_->NotifyReadyToActivate()
  // immediately if any notifications had been blocked while blocking.
  virtual void BlockNotifyReadyToActivateForTesting(bool block);

  // This allows us to inject DidInitializeVisibleTile events for testing.
  void DidInitializeVisibleTileForTesting();

  // Resets all of the trees to an empty state.
  void ResetTreesForTesting();

  bool device_viewport_valid_for_tile_management() const {
    return device_viewport_valid_for_tile_management_;
  }

  // Viewport size in draw space: this size is in physical pixels and is used
  // for draw properties, tilings, quads and render passes.
  gfx::Size DrawViewportSize() const;

  // Viewport size for scrolling and fixed-position compensation. This value
  // excludes the URL bar and non-overlay scrollbars and is in DIP (and
  // invariant relative to page scale).
  gfx::SizeF UnscaledScrollableViewportSize() const;
  float VerticalAdjust() const;

  // RendererClient implementation.
  virtual void SetFullRootLayerDamage() OVERRIDE;
  virtual void RunOnDemandRasterTask(Task* on_demand_raster_task) OVERRIDE;

  // TileManagerClient implementation.
  virtual const std::vector<PictureLayerImpl*>& GetPictureLayers() OVERRIDE;
  virtual void NotifyReadyToActivate() OVERRIDE;
  virtual void NotifyTileStateChanged(const Tile* tile) OVERRIDE;

  // ScrollbarAnimationControllerClient implementation.
  virtual void PostDelayedScrollbarFade(const base::Closure& start_fade,
                                        base::TimeDelta delay) OVERRIDE;
  virtual void SetNeedsScrollbarAnimationFrame() OVERRIDE;

  // OutputSurfaceClient implementation.
  virtual void DeferredInitialize() OVERRIDE;
  virtual void ReleaseGL() OVERRIDE;
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) OVERRIDE;
  virtual void SetNeedsRedrawRect(const gfx::Rect& rect) OVERRIDE;
  virtual void BeginFrame(const BeginFrameArgs& args) OVERRIDE;
  virtual void SetExternalDrawConstraints(
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      bool valid_for_tile_management) OVERRIDE;
  virtual void DidLoseOutputSurface() OVERRIDE;
  virtual void DidSwapBuffers() OVERRIDE;
  virtual void DidSwapBuffersComplete() OVERRIDE;
  virtual void ReclaimResources(const CompositorFrameAck* ack) OVERRIDE;
  virtual void SetMemoryPolicy(const ManagedMemoryPolicy& policy) OVERRIDE;
  virtual void SetTreeActivationCallback(const base::Closure& callback)
      OVERRIDE;

  // Called from LayerTreeImpl.
  void OnCanDrawStateChangedForTree();

  // Implementation.
  int id() const { return id_; }
  bool CanDraw() const;
  OutputSurface* output_surface() const { return output_surface_.get(); }

  std::string LayerTreeAsJson() const;

  void FinishAllRendering();
  int SourceAnimationFrameNumber() const;

  virtual bool InitializeRenderer(scoped_ptr<OutputSurface> output_surface);
  bool IsContextLost();
  TileManager* tile_manager() { return tile_manager_.get(); }
  void SetUseGpuRasterization(bool use_gpu);
  bool use_gpu_rasterization() const { return use_gpu_rasterization_; }
  bool create_low_res_tiling() const {
    return settings_.create_low_res_tiling && !use_gpu_rasterization_;
  }
  ResourcePool* resource_pool() { return resource_pool_.get(); }
  Renderer* renderer() { return renderer_.get(); }
  const RendererCapabilitiesImpl& GetRendererCapabilities() const;

  virtual bool SwapBuffers(const FrameData& frame);
  void SetNeedsBeginFrame(bool enable);
  virtual void WillBeginImplFrame(const BeginFrameArgs& args);
  void DidModifyTilePriorities();

  LayerTreeImpl* active_tree() { return active_tree_.get(); }
  const LayerTreeImpl* active_tree() const { return active_tree_.get(); }
  LayerTreeImpl* pending_tree() { return pending_tree_.get(); }
  const LayerTreeImpl* pending_tree() const { return pending_tree_.get(); }
  const LayerTreeImpl* recycle_tree() const { return recycle_tree_.get(); }
  // Returns the tree LTH synchronizes with.
  LayerTreeImpl* sync_tree() {
    // In impl-side painting, synchronize to the pending tree so that it has
    // time to raster before being displayed.
    return settings_.impl_side_painting ? pending_tree_.get()
                                        : active_tree_.get();
  }
  virtual void CreatePendingTree();
  virtual void UpdateVisibleTiles();
  virtual void ActivatePendingTree();

  // Shortcuts to layers on the active tree.
  LayerImpl* RootLayer() const;
  LayerImpl* InnerViewportScrollLayer() const;
  LayerImpl* OuterViewportScrollLayer() const;
  LayerImpl* CurrentlyScrollingLayer() const;

  int scroll_layer_id_when_mouse_over_scrollbar() const {
    return scroll_layer_id_when_mouse_over_scrollbar_;
  }
  bool scroll_affects_scroll_handler() const {
    return scroll_affects_scroll_handler_;
  }

  bool IsCurrentlyScrolling() const;

  virtual void SetVisible(bool visible);
  bool visible() const { return visible_; }

  void SetNeedsCommit() { client_->SetNeedsCommitOnImplThread(); }
  void SetNeedsRedraw();

  ManagedMemoryPolicy ActualManagedMemoryPolicy() const;

  size_t memory_allocation_limit_bytes() const;
  int memory_allocation_priority_cutoff() const;

  void SetViewportSize(const gfx::Size& device_viewport_size);
  gfx::Size device_viewport_size() const { return device_viewport_size_; }

  void SetOverdrawBottomHeight(float overdraw_bottom_height);
  float overdraw_bottom_height() const { return overdraw_bottom_height_; }

  void SetOverhangUIResource(UIResourceId overhang_ui_resource_id,
                             const gfx::Size& overhang_ui_resource_size);

  void SetDeviceScaleFactor(float device_scale_factor);
  float device_scale_factor() const { return device_scale_factor_; }

  const gfx::Transform& DrawTransform() const;

  scoped_ptr<ScrollAndScaleSet> ProcessScrollDeltas();

  bool needs_animate_layers() const {
    return !animation_registrar_->active_animation_controllers().empty();
  }

  void set_max_memory_needed_bytes(size_t bytes) {
    max_memory_needed_bytes_ = bytes;
  }

  FrameRateCounter* fps_counter() {
    return fps_counter_.get();
  }
  PaintTimeCounter* paint_time_counter() {
    return paint_time_counter_.get();
  }
  MemoryHistory* memory_history() {
    return memory_history_.get();
  }
  DebugRectHistory* debug_rect_history() {
    return debug_rect_history_.get();
  }
  ResourceProvider* resource_provider() {
    return resource_provider_.get();
  }
  TopControlsManager* top_controls_manager() {
    return top_controls_manager_.get();
  }
  const GlobalStateThatImpactsTilePriority& global_tile_state() {
    return global_tile_state_;
  }

  Proxy* proxy() const { return proxy_; }

  AnimationRegistrar* animation_registrar() const {
    return animation_registrar_.get();
  }

  void SetDebugState(const LayerTreeDebugState& new_debug_state);
  const LayerTreeDebugState& debug_state() const { return debug_state_; }

  class CC_EXPORT CullRenderPassesWithNoQuads {
   public:
    bool ShouldRemoveRenderPass(const RenderPassDrawQuad& quad,
                                const FrameData& frame) const;

    // Iterates in draw order, so that when a surface is removed, and its
    // target becomes empty, then its target can be removed also.
    size_t RenderPassListBegin(const RenderPassList& list) const { return 0; }
    size_t RenderPassListEnd(const RenderPassList& list) const {
      return list.size();
    }
    size_t RenderPassListNext(size_t it) const { return it + 1; }
  };

  template <typename RenderPassCuller>
      static void RemoveRenderPasses(RenderPassCuller culler, FrameData* frame);

  gfx::Vector2dF accumulated_root_overscroll() const {
    return accumulated_root_overscroll_;
  }

  bool pinch_gesture_active() const { return pinch_gesture_active_; }

  void SetTreePriority(TreePriority priority);

  void UpdateCurrentFrameTime();
  void ResetCurrentFrameTimeForNextFrame();
  virtual base::TimeTicks CurrentFrameTimeTicks();

  // Expected time between two begin impl frame calls.
  base::TimeDelta begin_impl_frame_interval() const {
    return begin_impl_frame_interval_;
  }

  scoped_ptr<base::Value> AsValue() const { return AsValueWithFrame(NULL); }
  scoped_ptr<base::Value> AsValueWithFrame(FrameData* frame) const;
  scoped_ptr<base::Value> ActivationStateAsValue() const;

  bool page_scale_animation_active() const { return !!page_scale_animation_; }

  virtual void CreateUIResource(UIResourceId uid,
                                const UIResourceBitmap& bitmap);
  // Deletes a UI resource.  May safely be called more than once.
  virtual void DeleteUIResource(UIResourceId uid);
  void EvictAllUIResources();
  bool EvictedUIResourcesExist() const;

  virtual ResourceProvider::ResourceId ResourceIdForUIResource(
      UIResourceId uid) const;

  virtual bool IsUIResourceOpaque(UIResourceId uid) const;

  struct UIResourceData {
    ResourceProvider::ResourceId resource_id;
    gfx::Size size;
    bool opaque;
  };

  void ScheduleMicroBenchmark(scoped_ptr<MicroBenchmarkImpl> benchmark);

  CompositorFrameMetadata MakeCompositorFrameMetadata() const;
  // Viewport rectangle and clip in nonflipped window space.  These rects
  // should only be used by Renderer subclasses to populate glViewport/glClip
  // and their software-mode equivalents.
  gfx::Rect DeviceViewport() const;
  gfx::Rect DeviceClip() const;

  // When a SwapPromiseMonitor is created on the impl thread, it calls
  // InsertSwapPromiseMonitor() to register itself with LayerTreeHostImpl.
  // When the monitor is destroyed, it calls RemoveSwapPromiseMonitor()
  // to unregister itself.
  void InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor);
  void RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor);

  void RegisterPictureLayerImpl(PictureLayerImpl* layer);
  void UnregisterPictureLayerImpl(PictureLayerImpl* layer);

 protected:
  LayerTreeHostImpl(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      Proxy* proxy,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      SharedBitmapManager* manager,
      int id);

  gfx::SizeF ComputeInnerViewportContainerSize() const;
  void UpdateInnerViewportContainerSize();

  // Virtual for testing.
  virtual void AnimateLayers(base::TimeTicks monotonic_time);

  // Virtual for testing.
  virtual base::TimeDelta LowFrequencyAnimationInterval() const;

  const AnimationRegistrar::AnimationControllerMap&
      active_animation_controllers() const {
    return animation_registrar_->active_animation_controllers();
  }

  bool manage_tiles_needed() const { return tile_priorities_dirty_; }

  LayerTreeHostImplClient* client_;
  Proxy* proxy_;

 private:
  void CreateAndSetRenderer();
  void CreateAndSetTileManager();
  void DestroyTileManager();
  void ReleaseTreeResources();
  void EnforceZeroBudget(bool zero_budget);

  bool UseZeroCopyTextureUpload() const;
  bool UseOneCopyTextureUpload() const;

  void ScrollViewportBy(gfx::Vector2dF scroll_delta);
  void AnimatePageScale(base::TimeTicks monotonic_time);
  void AnimateScrollbars(base::TimeTicks monotonic_time);
  void AnimateTopControls(base::TimeTicks monotonic_time);

  gfx::Vector2dF ScrollLayerWithViewportSpaceDelta(
      LayerImpl* layer_impl,
      float scale_from_viewport_to_screen_space,
      const gfx::PointF& viewport_point,
      const gfx::Vector2dF& viewport_delta);

  void TrackDamageForAllSurfaces(
      LayerImpl* root_draw_layer,
      const LayerImplList& render_surface_layer_list);

  void UpdateTileManagerMemoryPolicy(const ManagedMemoryPolicy& policy);

  // This function should only be called from PrepareToDraw, as DidDrawAllLayers
  // must be called if this helper function is called.  Returns DRAW_SUCCESS if
  // the frame should be drawn.
  DrawResult CalculateRenderPasses(FrameData* frame);

  void ClearCurrentlyScrollingLayer();

  bool HandleMouseOverScrollbar(LayerImpl* layer_impl,
                                const gfx::PointF& device_viewport_point);

  void AnimateScrollbarsRecursive(LayerImpl* layer,
                                  base::TimeTicks time);

  LayerImpl* FindScrollLayerForDeviceViewportPoint(
      const gfx::PointF& device_viewport_point,
      InputHandler::ScrollInputType type,
      LayerImpl* layer_hit_by_point,
      bool* scroll_on_main_thread,
      bool* optional_has_ancestor_scroll_handler) const;
  float DeviceSpaceDistanceToLayer(const gfx::PointF& device_viewport_point,
                                   LayerImpl* layer_impl);
  void StartScrollbarFadeRecursive(LayerImpl* layer);
  void SetManagedMemoryPolicy(const ManagedMemoryPolicy& policy,
                              bool zero_budget);
  void EnforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy);

  void DidInitializeVisibleTile();

  void MarkUIResourceNotEvicted(UIResourceId uid);

  void NotifySwapPromiseMonitorsOfSetNeedsRedraw();

  typedef base::hash_map<UIResourceId, UIResourceData>
      UIResourceMap;
  UIResourceMap ui_resource_map_;

  // Resources that were evicted by EvictAllUIResources. Resources are removed
  // from this when they are touched by a create or destroy from the UI resource
  // request queue.
  std::set<UIResourceId> evicted_ui_resources_;

  scoped_ptr<OutputSurface> output_surface_;
  scoped_refptr<ContextProvider> offscreen_context_provider_;

  // |resource_provider_| and |tile_manager_| can be NULL, e.g. when using tile-
  // free rendering - see OutputSurface::ForcedDrawToSoftwareDevice().
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<TileManager> tile_manager_;
  bool use_gpu_rasterization_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<ResourcePool> staging_resource_pool_;
  scoped_ptr<Renderer> renderer_;

  TaskGraphRunner synchronous_task_graph_runner_;
  TaskGraphRunner* on_demand_task_graph_runner_;
  NamespaceToken on_demand_task_namespace_;

  GlobalStateThatImpactsTilePriority global_tile_state_;

  // Tree currently being drawn.
  scoped_ptr<LayerTreeImpl> active_tree_;

  // In impl-side painting mode, tree with possibly incomplete rasterized
  // content. May be promoted to active by ActivatePendingTree().
  scoped_ptr<LayerTreeImpl> pending_tree_;

  // In impl-side painting mode, inert tree with layers that can be recycled
  // by the next sync from the main thread.
  scoped_ptr<LayerTreeImpl> recycle_tree_;

  InputHandlerClient* input_handler_client_;
  bool did_lock_scrolling_layer_;
  bool should_bubble_scrolls_;
  bool wheel_scrolling_;
  bool scroll_affects_scroll_handler_;
  int scroll_layer_id_when_mouse_over_scrollbar_;

  bool tile_priorities_dirty_;

  // The optional delegate for the root layer scroll offset.
  LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate_;
  LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;
  bool visible_;
  ManagedMemoryPolicy cached_managed_memory_policy_;

  gfx::Vector2dF accumulated_root_overscroll_;

  bool pinch_gesture_active_;
  bool pinch_gesture_end_should_clear_scrolling_layer_;
  gfx::Point previous_pinch_anchor_;

  scoped_ptr<TopControlsManager> top_controls_manager_;

  scoped_ptr<PageScaleAnimation> page_scale_animation_;

  // This is used for ticking animations slowly when hidden.
  scoped_ptr<LayerTreeHostImplTimeSourceAdapter> time_source_client_adapter_;

  scoped_ptr<FrameRateCounter> fps_counter_;
  scoped_ptr<PaintTimeCounter> paint_time_counter_;
  scoped_ptr<MemoryHistory> memory_history_;
  scoped_ptr<DebugRectHistory> debug_rect_history_;

  scoped_ptr<TextureMailboxDeleter> texture_mailbox_deleter_;

  // The maximum memory that would be used by the prioritized resource
  // manager, if there were no limit on memory usage.
  size_t max_memory_needed_bytes_;

  bool zero_budget_;

  // Viewport size passed in from the main thread, in physical pixels.  This
  // value is the default size for all concepts of physical viewport (draw
  // viewport, scrolling viewport and device viewport), but it can be
  // overridden.
  gfx::Size device_viewport_size_;

  // Conversion factor from CSS pixels to physical pixels when
  // pageScaleFactor=1.
  float device_scale_factor_;

  // UI resource to use for drawing overhang gutters.
  UIResourceId overhang_ui_resource_id_;
  gfx::Size overhang_ui_resource_size_;

  // Vertical amount of the viewport size that's known to covered by a
  // browser-side UI element, such as an on-screen-keyboard.  This affects
  // scrollable size since we want to still be able to scroll to the bottom of
  // the page when the keyboard is up.
  float overdraw_bottom_height_;

  // Optional top-level constraints that can be set by the OutputSurface.
  // - external_transform_ applies a transform above the root layer
  // - external_viewport_ is used DrawProperties, tile management and
  // glViewport/window projection matrix.
  // - external_clip_ specifies a top-level clip rect
  gfx::Transform external_transform_;
  gfx::Rect external_viewport_;
  gfx::Rect external_clip_;
  bool device_viewport_valid_for_tile_management_;

  gfx::Rect viewport_damage_rect_;

  base::TimeTicks current_frame_timeticks_;

  // Expected time between two begin impl frame calls.
  base::TimeDelta begin_impl_frame_interval_;

  scoped_ptr<AnimationRegistrar> animation_registrar_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;
  MicroBenchmarkControllerImpl micro_benchmark_controller_;

  bool need_to_update_visible_tiles_before_draw_;
#if DCHECK_IS_ON
  bool did_lose_called_;
#endif

  // Optional callback to notify of new tree activations.
  base::Closure tree_activation_callback_;

  SharedBitmapManager* shared_bitmap_manager_;
  int id_;

  std::set<SwapPromiseMonitor*> swap_promise_monitor_;

  size_t transfer_buffer_memory_limit_;

  std::vector<PictureLayerImpl*> picture_layers_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHostImpl);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_IMPL_H_
