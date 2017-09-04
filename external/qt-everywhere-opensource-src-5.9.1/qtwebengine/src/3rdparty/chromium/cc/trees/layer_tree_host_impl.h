// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_IMPL_H_
#define CC_TREES_LAYER_TREE_HOST_IMPL_H_

#include <stddef.h>

#include <bitset>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/base/synced_property.h"
#include "cc/debug/micro_benchmark_controller_impl.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/output/context_cache_controller.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/scheduler/begin_frame_tracker.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/video_frame_controller.h"
#include "cc/tiles/image_decode_controller.h"
#include "cc/tiles/tile_manager.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/task_runner_provider.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class BrowserControlsOffsetManager;
class CompositorFrameMetadata;
class CompositorFrameSink;
class DebugRectHistory;
class EvictionTilePriorityQueue;
class FrameRateCounter;
class LayerImpl;
class LayerTreeImpl;
class MemoryHistory;
class MutatorEvents;
class MutatorHost;
class PageScaleAnimation;
class PendingTreeDurationHistogramTimer;
class RasterTilePriorityQueue;
class RasterBufferProvider;
class RenderingStatsInstrumentation;
class ResourcePool;
class ScrollElasticityHelper;
class SwapPromise;
class SwapPromiseMonitor;
class SynchronousTaskGraphRunner;
class TaskGraphRunner;
class UIResourceBitmap;
struct ScrollAndScaleSet;
class Viewport;

using BeginFrameCallbackList = std::vector<base::Closure>;

enum class GpuRasterizationStatus {
  ON,
  ON_FORCED,
  OFF_DEVICE,
  OFF_VIEWPORT,
  MSAA_CONTENT,
  OFF_CONTENT
};

// LayerTreeHost->Proxy callback interface.
class LayerTreeHostImplClient {
 public:
  virtual void DidLoseCompositorFrameSinkOnImplThread() = 0;
  virtual void SetBeginFrameSource(BeginFrameSource* source) = 0;
  virtual void DidReceiveCompositorFrameAckOnImplThread() = 0;
  virtual void OnCanDrawStateChanged(bool can_draw) = 0;
  virtual void NotifyReadyToActivate() = 0;
  virtual void NotifyReadyToDraw() = 0;
  // Please call these 2 functions through
  // LayerTreeHostImpl's SetNeedsRedraw() and SetNeedsOneBeginImplFrame().
  virtual void SetNeedsRedrawOnImplThread() = 0;
  virtual void SetNeedsOneBeginImplFrameOnImplThread() = 0;
  virtual void SetNeedsCommitOnImplThread() = 0;
  virtual void SetNeedsPrepareTilesOnImplThread() = 0;
  virtual void SetVideoNeedsBeginFrames(bool needs_begin_frames) = 0;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      std::unique_ptr<MutatorEvents> events) = 0;
  virtual bool IsInsideDraw() = 0;
  virtual void RenewTreePriority() = 0;
  virtual void PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                                    base::TimeDelta delay) = 0;
  virtual void DidActivateSyncTree() = 0;
  virtual void WillPrepareTiles() = 0;
  virtual void DidPrepareTiles() = 0;

  // Called when page scale animation has completed on the impl thread.
  virtual void DidCompletePageScaleAnimationOnImplThread() = 0;

  // Called when output surface asks for a draw.
  virtual void OnDrawForCompositorFrameSink(
      bool resourceless_software_draw) = 0;

 protected:
  virtual ~LayerTreeHostImplClient() {}
};

// LayerTreeHostImpl owns the LayerImpl trees as well as associated rendering
// state.
class CC_EXPORT LayerTreeHostImpl
    : public InputHandler,
      public TileManagerClient,
      public CompositorFrameSinkClient,
      public BrowserControlsOffsetManagerClient,
      public ScrollbarAnimationControllerClient,
      public VideoFrameControllerClient,
      public LayerTreeMutatorClient,
      public MutatorHostClient,
      public base::SupportsWeakPtr<LayerTreeHostImpl> {
 public:
  static std::unique_ptr<LayerTreeHostImpl> Create(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<MutatorHost> mutator_host,
      int id);
  ~LayerTreeHostImpl() override;

  // InputHandler implementation
  void BindToClient(InputHandlerClient* client) override;
  InputHandler::ScrollStatus ScrollBegin(
      ScrollState* scroll_state,
      InputHandler::ScrollInputType type) override;
  InputHandler::ScrollStatus RootScrollBegin(
      ScrollState* scroll_state,
      InputHandler::ScrollInputType type) override;
  ScrollStatus ScrollAnimatedBegin(const gfx::Point& viewport_point) override;
  InputHandler::ScrollStatus ScrollAnimated(
      const gfx::Point& viewport_point,
      const gfx::Vector2dF& scroll_delta,
      base::TimeDelta delayed_by = base::TimeDelta()) override;
  void ApplyScroll(ScrollNode* scroll_node, ScrollState* scroll_state);
  InputHandlerScrollResult ScrollBy(ScrollState* scroll_state) override;
  void RequestUpdateForSynchronousInputHandler() override;
  void SetSynchronousInputHandlerRootScrollOffset(
      const gfx::ScrollOffset& root_offset) override;
  void ScrollEnd(ScrollState* scroll_state) override;
  InputHandler::ScrollStatus FlingScrollBegin() override;

  void MouseDown() override;
  void MouseUp() override;
  void MouseMoveAt(const gfx::Point& viewport_point) override;
  void MouseLeave() override;

  void PinchGestureBegin() override;
  void PinchGestureUpdate(float magnify_delta,
                          const gfx::Point& anchor) override;
  void PinchGestureEnd() override;
  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool anchor_point,
                               float page_scale,
                               base::TimeDelta duration);
  void SetNeedsAnimateInput() override;
  bool IsCurrentlyScrollingViewport() const override;
  bool IsCurrentlyScrollingLayerAt(
      const gfx::Point& viewport_point,
      InputHandler::ScrollInputType type) const override;
  EventListenerProperties GetEventListenerProperties(
      EventListenerClass event_class) const override;
  bool DoTouchEventsBlockScrollAt(const gfx::Point& viewport_port) override;
  std::unique_ptr<SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) override;
  ScrollElasticityHelper* CreateScrollElasticityHelper() override;
  bool GetScrollOffsetForLayer(int layer_id,
                               gfx::ScrollOffset* offset) override;
  bool ScrollLayerTo(int layer_id, const gfx::ScrollOffset& offset) override;

  // BrowserControlsOffsetManagerClient implementation.
  float TopControlsHeight() const override;
  float BottomControlsHeight() const override;
  void SetCurrentBrowserControlsShownRatio(float offset) override;
  float CurrentBrowserControlsShownRatio() const override;
  void DidChangeBrowserControlsPosition() override;
  bool HaveRootScrollLayer() const override;

  void UpdateViewportContainerSizes();

  void set_resourceless_software_draw_for_testing() {
    resourceless_software_draw_ = true;
  }

  struct CC_EXPORT FrameData : public RenderPassSink {
    FrameData();
    ~FrameData() override;
    void AsValueInto(base::trace_event::TracedValue* value) const;

    std::vector<gfx::Rect> occluding_screen_space_rects;
    std::vector<gfx::Rect> non_occluding_screen_space_rects;
    RenderPassList render_passes;
    const LayerImplList* render_surface_layer_list;
    LayerImplList will_draw_layers;
    bool has_no_damage;
    bool may_contain_video;

    // RenderPassSink implementation.
    void AppendRenderPass(std::unique_ptr<RenderPass> render_pass) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameData);
  };

  virtual void BeginMainFrameAborted(
      CommitEarlyOutReason reason,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises);
  virtual void ReadyToCommit() {}  // For tests.
  virtual void BeginCommit();
  virtual void CommitComplete();
  virtual void UpdateAnimationState(bool start_ready_animations);
  bool Mutate(base::TimeTicks monotonic_time);
  void ActivateAnimations();
  void Animate();
  void AnimatePendingTreeAfterCommit();
  void MainThreadHasStoppedFlinging();
  void DidAnimateScrollOffset();
  void SetFullViewportDamage();
  void SetViewportDamage(const gfx::Rect& damage_rect);

  void SetTreeLayerFilterMutated(ElementId element_id,
                                 LayerTreeImpl* tree,
                                 const FilterOperations& filters);
  void SetTreeLayerOpacityMutated(ElementId element_id,
                                  LayerTreeImpl* tree,
                                  float opacity);
  void SetTreeLayerTransformMutated(ElementId element_id,
                                    LayerTreeImpl* tree,
                                    const gfx::Transform& transform);
  void SetTreeLayerScrollOffsetMutated(ElementId element_id,
                                       LayerTreeImpl* tree,
                                       const gfx::ScrollOffset& scroll_offset);
  bool AnimationsPreserveAxisAlignment(const LayerImpl* layer) const;
  void SetNeedUpdateGpuRasterizationStatus();

  // MutatorHostClient implementation.
  bool IsElementInList(ElementId element_id,
                       ElementListType list_type) const override;
  void SetMutatorsNeedCommit() override;
  void SetMutatorsNeedRebuildPropertyTrees() override;
  void SetElementFilterMutated(ElementId element_id,
                               ElementListType list_type,
                               const FilterOperations& filters) override;
  void SetElementOpacityMutated(ElementId element_id,
                                ElementListType list_type,
                                float opacity) override;
  void SetElementTransformMutated(ElementId element_id,
                                  ElementListType list_type,
                                  const gfx::Transform& transform) override;
  void SetElementScrollOffsetMutated(
      ElementId element_id,
      ElementListType list_type,
      const gfx::ScrollOffset& scroll_offset) override;
  void ElementIsAnimatingChanged(ElementId element_id,
                                 ElementListType list_type,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state) override;

  void ScrollOffsetAnimationFinished() override;
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

  virtual bool PrepareTiles();

  // Returns DRAW_SUCCESS unless problems occured preparing the frame, and we
  // should try to avoid displaying the frame. If PrepareToDraw is called,
  // DidDrawAllLayers must also be called, regardless of whether DrawLayers is
  // called between the two.
  virtual DrawResult PrepareToDraw(FrameData* frame);
  virtual bool DrawLayers(FrameData* frame);
  // Must be called if and only if PrepareToDraw was called.
  void DidDrawAllLayers(const FrameData& frame);

  const LayerTreeSettings& settings() const { return settings_; }

  // Evict all textures by enforcing a memory policy with an allocation of 0.
  void EvictTexturesForTesting();

  // When blocking, this prevents client_->NotifyReadyToActivate() from being
  // called. When disabled, it calls client_->NotifyReadyToActivate()
  // immediately if any notifications had been blocked while blocking.
  virtual void BlockNotifyReadyToActivateForTesting(bool block);

  // Resets all of the trees to an empty state.
  void ResetTreesForTesting();

  size_t SourceAnimationFrameNumberForTesting() const;

  void RegisterScrollbarAnimationController(int scroll_layer_id);
  void UnregisterScrollbarAnimationController(int scroll_layer_id);
  ScrollbarAnimationController* ScrollbarAnimationControllerForId(
      int scroll_layer_id) const;

  DrawMode GetDrawMode() const;

  // Viewport size in draw space: this size is in physical pixels and is used
  // for draw properties, tilings, quads and render passes.
  gfx::Size DrawViewportSize() const;

  // Viewport rect in view space used for tiling prioritization.
  const gfx::Rect ViewportRectForTilePriority() const;

  // TileManagerClient implementation.
  void NotifyReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void NotifyAllTileTasksCompleted() override;
  void NotifyTileStateChanged(const Tile* tile) override;
  std::unique_ptr<RasterTilePriorityQueue> BuildRasterQueue(
      TreePriority tree_priority,
      RasterTilePriorityQueue::Type type) override;
  std::unique_ptr<EvictionTilePriorityQueue> BuildEvictionQueue(
      TreePriority tree_priority) override;
  void SetIsLikelyToRequireADraw(bool is_likely_to_require_a_draw) override;
  gfx::ColorSpace GetTileColorSpace() const override;

  // ScrollbarAnimationControllerClient implementation.
  void PostDelayedScrollbarAnimationTask(const base::Closure& task,
                                         base::TimeDelta delay) override;
  void SetNeedsAnimateForScrollbarAnimation() override;
  void SetNeedsRedrawForScrollbarAnimation() override;
  ScrollbarSet ScrollbarsFor(int scroll_layer_id) const override;
  void DidChangeScrollbarVisibility() override;

  // VideoBeginFrameSource implementation.
  void AddVideoFrameController(VideoFrameController* controller) override;
  void RemoveVideoFrameController(VideoFrameController* controller) override;

  // CompositorFrameSinkClient implementation.
  void SetBeginFrameSource(BeginFrameSource* source) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;
  void DidLoseCompositorFrameSink() override;
  void DidReceiveCompositorFrameAck() override;
  void ReclaimResources(const ReturnedResourceArray& resources) override;
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(const base::Closure& callback) override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override;

  // LayerTreeMutatorClient.
  void SetNeedsMutate() override;

  // Called from LayerTreeImpl.
  void OnCanDrawStateChangedForTree();

  // Implementation.
  int id() const { return id_; }
  bool CanDraw() const;
  CompositorFrameSink* compositor_frame_sink() const {
    return compositor_frame_sink_;
  }
  void ReleaseCompositorFrameSink();

  std::string LayerTreeAsJson() const;

  int RequestedMSAASampleCount() const;

  // TODO(danakj): Rename this, there is no renderer.
  virtual bool InitializeRenderer(CompositorFrameSink* compositor_frame_sink);
  TileManager* tile_manager() { return &tile_manager_; }

  void SetHasGpuRasterizationTrigger(bool flag);
  void SetContentIsSuitableForGpuRasterization(bool flag);
  bool CanUseGpuRasterization();
  bool use_gpu_rasterization() const { return use_gpu_rasterization_; }
  bool use_msaa() const { return use_msaa_; }

  GpuRasterizationStatus gpu_rasterization_status() const {
    return gpu_rasterization_status_;
  }

  bool create_low_res_tiling() const {
    return settings_.create_low_res_tiling && !use_gpu_rasterization_;
  }
  ResourcePool* resource_pool() { return resource_pool_.get(); }
  ImageDecodeController* image_decode_controller() {
    return image_decode_controller_.get();
  }

  virtual void WillBeginImplFrame(const BeginFrameArgs& args);
  virtual void DidFinishImplFrame();
  void DidModifyTilePriorities();

  LayerTreeImpl* active_tree() { return active_tree_.get(); }
  const LayerTreeImpl* active_tree() const { return active_tree_.get(); }
  LayerTreeImpl* pending_tree() { return pending_tree_.get(); }
  const LayerTreeImpl* pending_tree() const { return pending_tree_.get(); }
  LayerTreeImpl* recycle_tree() { return recycle_tree_.get(); }
  const LayerTreeImpl* recycle_tree() const { return recycle_tree_.get(); }
  // Returns the tree LTH synchronizes with.
  LayerTreeImpl* sync_tree() const {
    // TODO(enne): This is bogus.  It should return based on the value of
    // CommitToActiveTree() and not whether the pending tree exists.
    return pending_tree_ ? pending_tree_.get() : active_tree_.get();
  }
  virtual void CreatePendingTree();
  virtual void ActivateSyncTree();

  // Shortcuts to layers on the active tree.
  LayerImpl* InnerViewportScrollLayer() const;
  LayerImpl* OuterViewportScrollLayer() const;
  LayerImpl* CurrentlyScrollingLayer() const;

  bool scroll_affects_scroll_handler() const {
    return scroll_affects_scroll_handler_;
  }
  void QueueSwapPromiseForMainThreadScrollUpdate(
      std::unique_ptr<SwapPromise> swap_promise);

  bool IsActivelyScrolling() const;

  virtual void SetVisible(bool visible);
  bool visible() const { return visible_; }

  void SetNeedsCommit() { client_->SetNeedsCommitOnImplThread(); }
  void SetNeedsOneBeginImplFrame();
  void SetNeedsRedraw();

  ManagedMemoryPolicy ActualManagedMemoryPolicy() const;

  void SetViewportSize(const gfx::Size& device_viewport_size);
  gfx::Size device_viewport_size() const { return device_viewport_size_; }

  const gfx::Transform& DrawTransform() const;

  std::unique_ptr<BeginFrameCallbackList> ProcessLayerTreeMutations();

  std::unique_ptr<ScrollAndScaleSet> ProcessScrollDeltas();

  void set_max_memory_needed_bytes(size_t bytes) {
    max_memory_needed_bytes_ = bytes;
  }

  FrameRateCounter* fps_counter() { return fps_counter_.get(); }
  MemoryHistory* memory_history() { return memory_history_.get(); }
  DebugRectHistory* debug_rect_history() { return debug_rect_history_.get(); }
  ResourceProvider* resource_provider() { return resource_provider_.get(); }
  BrowserControlsOffsetManager* browser_controls_manager() {
    return browser_controls_offset_manager_.get();
  }
  const GlobalStateThatImpactsTilePriority& global_tile_state() {
    return global_tile_state_;
  }

  TaskRunnerProvider* task_runner_provider() const {
    return task_runner_provider_;
  }

  MutatorHost* mutator_host() const { return mutator_host_.get(); }

  void SetDebugState(const LayerTreeDebugState& new_debug_state);
  const LayerTreeDebugState& debug_state() const { return debug_state_; }

  gfx::Vector2dF accumulated_root_overscroll() const {
    return accumulated_root_overscroll_;
  }

  bool pinch_gesture_active() const { return pinch_gesture_active_; }

  void SetTreePriority(TreePriority priority);
  TreePriority GetTreePriority() const;

  // TODO(mithro): Remove this methods which exposes the internal
  // BeginFrameArgs to external callers.
  virtual BeginFrameArgs CurrentBeginFrameArgs() const;

  // Expected time between two begin impl frame calls.
  base::TimeDelta CurrentBeginFrameInterval() const;

  void AsValueWithFrameInto(FrameData* frame,
                            base::trace_event::TracedValue* value) const;
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValueWithFrame(
      FrameData* frame) const;
  void ActivationStateAsValueInto(base::trace_event::TracedValue* value) const;

  bool page_scale_animation_active() const { return !!page_scale_animation_; }

  virtual void CreateUIResource(UIResourceId uid,
                                const UIResourceBitmap& bitmap);
  // Deletes a UI resource.  May safely be called more than once.
  virtual void DeleteUIResource(UIResourceId uid);
  void EvictAllUIResources();
  bool EvictedUIResourcesExist() const;

  virtual ResourceId ResourceIdForUIResource(UIResourceId uid) const;

  virtual bool IsUIResourceOpaque(UIResourceId uid) const;

  struct UIResourceData {
    ResourceId resource_id;
    bool opaque;
  };

  // Returns the amount of delta that can be applied to scroll_node, taking
  // page scale into account.
  gfx::Vector2dF ComputeScrollDelta(ScrollNode* scroll_node,
                                    const gfx::Vector2dF& delta);

  void ScheduleMicroBenchmark(std::unique_ptr<MicroBenchmarkImpl> benchmark);

  CompositorFrameMetadata MakeCompositorFrameMetadata() const;
  // Viewport rectangle and clip in device space.  These rects are used to
  // prioritize raster and determine what is submitted in a CompositorFrame.
  gfx::Rect DeviceViewport() const;

  // When a SwapPromiseMonitor is created on the impl thread, it calls
  // InsertSwapPromiseMonitor() to register itself with LayerTreeHostImpl.
  // When the monitor is destroyed, it calls RemoveSwapPromiseMonitor()
  // to unregister itself.
  void InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor);
  void RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor);

  // TODO(weiliangc): Replace RequiresHighResToDraw with scheduler waits for
  // ReadyToDraw. crbug.com/469175
  void SetRequiresHighResToDraw() { requires_high_res_to_draw_ = true; }
  void ResetRequiresHighResToDraw() { requires_high_res_to_draw_ = false; }
  bool RequiresHighResToDraw() const { return requires_high_res_to_draw_; }

  // Only valid for synchronous (non-scheduled) single-threaded case.
  void SynchronouslyInitializeAllTiles();

  bool SupportsImplScrolling() const;
  bool CommitToActiveTree() const;

  virtual void CreateResourceAndRasterBufferProvider(
      std::unique_ptr<RasterBufferProvider>* raster_buffer_provider,
      std::unique_ptr<ResourcePool>* resource_pool);

  bool prepare_tiles_needed() const { return tile_priorities_dirty_; }

  gfx::Vector2dF ScrollSingleNode(ScrollNode* scroll_node,
                                  const gfx::Vector2dF& delta,
                                  const gfx::Point& viewport_point,
                                  bool is_direct_manipulation,
                                  ScrollTree* scroll_tree);

  base::SingleThreadTaskRunner* GetTaskRunner() const {
    DCHECK(task_runner_provider_);
    return task_runner_provider_->HasImplThread()
               ? task_runner_provider_->ImplThreadTaskRunner()
               : task_runner_provider_->MainThreadTaskRunner();
  }

  InputHandler::ScrollStatus TryScroll(const gfx::PointF& screen_space_point,
                                       InputHandler::ScrollInputType type,
                                       const ScrollTree& scroll_tree,
                                       ScrollNode* scroll_node) const;

  // Returns true if a scroll offset animation is created and false if we scroll
  // by the desired amount without an animation.
  bool ScrollAnimationCreate(ScrollNode* scroll_node,
                             const gfx::Vector2dF& scroll_amount,
                             base::TimeDelta delayed_by);

  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator);
  LayerTreeMutator* mutator() { return mutator_.get(); }

  LayerImpl* ViewportMainScrollLayer();

 protected:
  LayerTreeHostImpl(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<MutatorHost> mutator_host,
      int id);

  // Virtual for testing.
  virtual bool AnimateLayers(base::TimeTicks monotonic_time);

  bool is_likely_to_require_a_draw() const {
    return is_likely_to_require_a_draw_;
  }

  // Removes empty or orphan RenderPasses from the frame.
  static void RemoveRenderPasses(FrameData* frame);

  LayerTreeHostImplClient* client_;
  TaskRunnerProvider* task_runner_provider_;

  BeginFrameTracker current_begin_frame_tracker_;

 private:
  gfx::Vector2dF ScrollNodeWithViewportSpaceDelta(
      ScrollNode* scroll_node,
      const gfx::PointF& viewport_point,
      const gfx::Vector2dF& viewport_delta,
      ScrollTree* scroll_tree);

  void CleanUpTileManagerAndUIResources();
  void CreateTileManagerResources();
  void ReleaseTreeResources();
  void ReleaseTileResources();
  void RecreateTileResources();

  void AnimateInternal(bool active_tree);

  // Returns true if status changed.
  bool UpdateGpuRasterizationStatus();
  void UpdateTreeResourcesForGpuRasterizationIfNeeded();

  Viewport* viewport() const { return viewport_.get(); }

  InputHandler::ScrollStatus ScrollBeginImpl(
      ScrollState* scroll_state,
      LayerImpl* scrolling_layer_impl,
      InputHandler::ScrollInputType type);
  void DistributeScrollDelta(ScrollState* scroll_state);

  bool AnimatePageScale(base::TimeTicks monotonic_time);
  bool AnimateScrollbars(base::TimeTicks monotonic_time);
  bool AnimateBrowserControls(base::TimeTicks monotonic_time);

  void TrackDamageForAllSurfaces(
      const LayerImplList& render_surface_layer_list);

  void UpdateTileManagerMemoryPolicy(const ManagedMemoryPolicy& policy);

  // This function should only be called from PrepareToDraw, as DidDrawAllLayers
  // must be called if this helper function is called.  Returns DRAW_SUCCESS if
  // the frame should be drawn.
  DrawResult CalculateRenderPasses(FrameData* frame);

  void ClearCurrentlyScrollingLayer();

  LayerImpl* FindScrollLayerForDeviceViewportPoint(
      const gfx::PointF& device_viewport_point,
      InputHandler::ScrollInputType type,
      LayerImpl* layer_hit_by_point,
      bool* scroll_on_main_thread,
      uint32_t* main_thread_scrolling_reason) const;
  float DeviceSpaceDistanceToLayer(const gfx::PointF& device_viewport_point,
                                   LayerImpl* layer_impl);
  void StartScrollbarFadeRecursive(LayerImpl* layer);
  void SetManagedMemoryPolicy(const ManagedMemoryPolicy& policy);

  void MarkUIResourceNotEvicted(UIResourceId uid);
  void ClearUIResources();

  void NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  void NotifySwapPromiseMonitorsOfForwardingToMainThread();

  void UpdateRootLayerStateForSynchronousInputHandler();

  void ScrollAnimationAbort(LayerImpl* layer_impl);

  bool ScrollAnimationUpdateTarget(ScrollNode* scroll_node,
                                   const gfx::Vector2dF& scroll_delta,
                                   base::TimeDelta delayed_by);

  void SetContextVisibility(bool is_visible);

  using UIResourceMap = std::unordered_map<UIResourceId, UIResourceData>;
  UIResourceMap ui_resource_map_;

  // Resources that were evicted by EvictAllUIResources. Resources are removed
  // from this when they are touched by a create or destroy from the UI resource
  // request queue.
  std::set<UIResourceId> evicted_ui_resources_;

  CompositorFrameSink* compositor_frame_sink_;

  // The following scoped variables must not outlive the
  // |compositor_frame_sink_|.
  // These should be transfered to ContextCacheController's
  // ClientBecameNotVisible() before the output surface is destroyed.
  std::unique_ptr<ContextCacheController::ScopedVisibility>
      compositor_context_visibility_;
  std::unique_ptr<ContextCacheController::ScopedVisibility>
      worker_context_visibility_;

  std::unique_ptr<ResourceProvider> resource_provider_;
  bool need_update_gpu_rasterization_status_;
  bool content_is_suitable_for_gpu_rasterization_;
  bool has_gpu_rasterization_trigger_;
  bool use_gpu_rasterization_;
  bool use_msaa_;
  GpuRasterizationStatus gpu_rasterization_status_;
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  std::unique_ptr<ResourcePool> resource_pool_;
  std::unique_ptr<ImageDecodeController> image_decode_controller_;

  GlobalStateThatImpactsTilePriority global_tile_state_;

  // Tree currently being drawn.
  std::unique_ptr<LayerTreeImpl> active_tree_;

  // In impl-side painting mode, tree with possibly incomplete rasterized
  // content. May be promoted to active by ActivatePendingTree().
  std::unique_ptr<LayerTreeImpl> pending_tree_;

  // In impl-side painting mode, inert tree with layers that can be recycled
  // by the next sync from the main thread.
  std::unique_ptr<LayerTreeImpl> recycle_tree_;

  InputHandlerClient* input_handler_client_;
  bool did_lock_scrolling_layer_;
  bool wheel_scrolling_;
  bool scroll_affects_scroll_handler_;
  int scroll_layer_id_mouse_currently_over_;

  std::vector<std::unique_ptr<SwapPromise>>
      swap_promises_for_main_thread_scroll_update_;

  // An object to implement the ScrollElasticityHelper interface and
  // hold all state related to elasticity. May be NULL if never requested.
  std::unique_ptr<ScrollElasticityHelper> scroll_elasticity_helper_;

  bool tile_priorities_dirty_;

  const LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;
  bool visible_;
  ManagedMemoryPolicy cached_managed_memory_policy_;

  const bool is_synchronous_single_threaded_;
  TileManager tile_manager_;

  gfx::Vector2dF accumulated_root_overscroll_;

  bool pinch_gesture_active_;
  bool pinch_gesture_end_should_clear_scrolling_layer_;

  std::unique_ptr<BrowserControlsOffsetManager>
      browser_controls_offset_manager_;

  std::unique_ptr<PageScaleAnimation> page_scale_animation_;

  std::unique_ptr<FrameRateCounter> fps_counter_;
  std::unique_ptr<MemoryHistory> memory_history_;
  std::unique_ptr<DebugRectHistory> debug_rect_history_;

  // The maximum memory that would be used by the prioritized resource
  // manager, if there were no limit on memory usage.
  size_t max_memory_needed_bytes_;

  // Viewport size passed in from the main thread, in physical pixels.  This
  // value is the default size for all concepts of physical viewport (draw
  // viewport, scrolling viewport and device viewport), but it can be
  // overridden.
  gfx::Size device_viewport_size_;

  // Optional top-level constraints that can be set by the CompositorFrameSink.
  // - external_transform_ applies a transform above the root layer
  // - external_viewport_ is used DrawProperties, tile management and
  // glViewport/window projection matrix.
  // - viewport_rect_for_tile_priority_ is the rect in view space used for
  // tiling priority.
  gfx::Transform external_transform_;
  gfx::Rect external_viewport_;
  gfx::Rect viewport_rect_for_tile_priority_;
  bool resourceless_software_draw_;

  gfx::Rect viewport_damage_rect_;

  std::unique_ptr<MutatorHost> mutator_host_;
  std::set<VideoFrameController*> video_frame_controllers_;

  // Map from scroll layer ID to scrollbar animation controller.
  // There is one animation controller per pair of overlay scrollbars.
  std::unordered_map<int, std::unique_ptr<ScrollbarAnimationController>>
      scrollbar_animation_controllers_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;
  MicroBenchmarkControllerImpl micro_benchmark_controller_;
  std::unique_ptr<SynchronousTaskGraphRunner>
      single_thread_synchronous_task_graph_runner_;

  // Optional callback to notify of new tree activations.
  base::Closure tree_activation_callback_;

  TaskGraphRunner* task_graph_runner_;
  int id_;

  std::set<SwapPromiseMonitor*> swap_promise_monitor_;

  bool requires_high_res_to_draw_;
  bool is_likely_to_require_a_draw_;

  // TODO(danakj): Delete the compositor frame sink and all resources when
  // it's lost instead of having this bool.
  bool has_valid_compositor_frame_sink_;

  std::unique_ptr<Viewport> viewport_;

  std::unique_ptr<LayerTreeMutator> mutator_;

  std::unique_ptr<PendingTreeDurationHistogramTimer>
      pending_tree_duration_timer_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHostImpl);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_IMPL_H_
