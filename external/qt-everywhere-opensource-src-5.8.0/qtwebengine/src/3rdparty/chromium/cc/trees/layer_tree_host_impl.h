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
#include "cc/animation/layer_tree_mutator.h"
#include "cc/base/cc_export.h"
#include "cc/base/synced_property.h"
#include "cc/debug/micro_benchmark_controller_impl.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/input/top_controls_manager_client.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/render_pass_sink.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/renderer.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/scheduler/begin_frame_tracker.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/video_frame_controller.h"
#include "cc/tiles/image_decode_controller.h"
#include "cc/tiles/tile_manager.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/task_runner_provider.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class AnimationEvents;
class AnimationHost;
class CompletionEvent;
class CompositorFrameMetadata;
class DebugRectHistory;
class EvictionTilePriorityQueue;
class FrameRateCounter;
class LayerImpl;
class LayerTreeImpl;
class MemoryHistory;
class PageScaleAnimation;
class PendingTreeDurationHistogramTimer;
class PictureLayerImpl;
class RasterTilePriorityQueue;
class TileTaskManager;
class RasterBufferProvider;
class RenderPassDrawQuad;
class RenderingStatsInstrumentation;
class ResourcePool;
class ScrollElasticityHelper;
class ScrollbarLayerImplBase;
class SwapPromise;
class SwapPromiseMonitor;
class SynchronousTaskGraphRunner;
class TaskGraphRunner;
class TextureMailboxDeleter;
class TopControlsManager;
class UIResourceBitmap;
class UIResourceRequest;
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
  virtual void UpdateRendererCapabilitiesOnImplThread() = 0;
  virtual void DidLoseOutputSurfaceOnImplThread() = 0;
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) = 0;
  virtual void SetBeginFrameSource(BeginFrameSource* source) = 0;
  virtual void SetEstimatedParentDrawTime(base::TimeDelta draw_time) = 0;
  virtual void DidSwapBuffersOnImplThread() = 0;
  virtual void DidSwapBuffersCompleteOnImplThread() = 0;
  virtual void OnCanDrawStateChanged(bool can_draw) = 0;
  virtual void NotifyReadyToActivate() = 0;
  virtual void NotifyReadyToDraw() = 0;
  // Please call these 3 functions through
  // LayerTreeHostImpl's SetNeedsRedraw(), SetNeedsRedrawRect() and
  // SetNeedsOneBeginImplFrame().
  virtual void SetNeedsRedrawOnImplThread() = 0;
  virtual void SetNeedsRedrawRectOnImplThread(const gfx::Rect& damage_rect) = 0;
  virtual void SetNeedsOneBeginImplFrameOnImplThread() = 0;
  virtual void SetNeedsCommitOnImplThread() = 0;
  virtual void SetNeedsPrepareTilesOnImplThread() = 0;
  virtual void SetVideoNeedsBeginFrames(bool needs_begin_frames) = 0;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      std::unique_ptr<AnimationEvents> events) = 0;
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
  virtual void OnDrawForOutputSurface(bool resourceless_software_draw) = 0;

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
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<AnimationHost> animation_host,
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
      const gfx::Vector2dF& scroll_delta) override;
  void ApplyScroll(ScrollNode* scroll_node, ScrollState* scroll_state);
  InputHandlerScrollResult ScrollBy(ScrollState* scroll_state) override;
  bool ScrollVerticallyByPage(const gfx::Point& viewport_point,
                              ScrollDirection direction) override;
  void RequestUpdateForSynchronousInputHandler() override;
  void SetSynchronousInputHandlerRootScrollOffset(
      const gfx::ScrollOffset& root_offset) override;
  void ScrollEnd(ScrollState* scroll_state) override;
  InputHandler::ScrollStatus FlingScrollBegin() override;
  void MouseMoveAt(const gfx::Point& viewport_point) override;
  void PinchGestureBegin() override;
  void PinchGestureUpdate(float magnify_delta,
                          const gfx::Point& anchor) override;
  void PinchGestureEnd() override;
  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool anchor_point,
                               float page_scale,
                               base::TimeDelta duration);
  void SetNeedsAnimateInput() override;
  bool IsCurrentlyScrollingInnerViewport() const override;
  bool IsCurrentlyScrollingLayerAt(
      const gfx::Point& viewport_point,
      InputHandler::ScrollInputType type) const override;
  EventListenerProperties GetEventListenerProperties(
      EventListenerClass event_class) const override;
  bool DoTouchEventsBlockScrollAt(const gfx::Point& viewport_port) override;
  std::unique_ptr<SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) override;
  ScrollElasticityHelper* CreateScrollElasticityHelper() override;

  // TopControlsManagerClient implementation.
  float TopControlsHeight() const override;
  void SetCurrentTopControlsShownRatio(float offset) override;
  float CurrentTopControlsShownRatio() const override;
  void DidChangeTopControlsPosition() override;
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

    // RenderPassSink implementation.
    void AppendRenderPass(std::unique_ptr<RenderPass> render_pass) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameData);
  };

  virtual void BeginMainFrameAborted(CommitEarlyOutReason reason);
  virtual void BeginCommit();
  virtual void CommitComplete();
  virtual void UpdateAnimationState(bool start_ready_animations);
  bool Mutate(base::TimeTicks monotonic_time);
  void ActivateAnimations();
  void Animate();
  void AnimatePendingTreeAfterCommit();
  void MainThreadHasStoppedFlinging();
  void DidAnimateScrollOffset();
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
  void ElementTransformIsAnimatingChanged(ElementId element_id,
                                          ElementListType list_type,
                                          AnimationChangeType change_type,
                                          bool is_animating) override;
  void ElementOpacityIsAnimatingChanged(ElementId element_id,
                                        ElementListType list_type,
                                        AnimationChangeType change_type,
                                        bool is_animating) override;
  void ScrollOffsetAnimationFinished() override;
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

  virtual bool PrepareTiles();

  // Returns DRAW_SUCCESS unless problems occured preparing the frame, and we
  // should try to avoid displaying the frame. If PrepareToDraw is called,
  // DidDrawAllLayers must also be called, regardless of whether DrawLayers is
  // called between the two.
  virtual DrawResult PrepareToDraw(FrameData* frame);
  virtual void DrawLayers(FrameData* frame);
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

  // RendererClient implementation.
  void SetFullRootLayerDamage() override;

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

  // ScrollbarAnimationControllerClient implementation.
  void PostDelayedScrollbarAnimationTask(const base::Closure& task,
                                         base::TimeDelta delay) override;
  void SetNeedsAnimateForScrollbarAnimation() override;
  void SetNeedsRedrawForScrollbarAnimation() override;
  ScrollbarSet ScrollbarsFor(int scroll_layer_id) const override;

  // VideoBeginFrameSource implementation.
  void AddVideoFrameController(VideoFrameController* controller) override;
  void RemoveVideoFrameController(VideoFrameController* controller) override;

  // OutputSurfaceClient implementation.
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void SetBeginFrameSource(BeginFrameSource* source) override;
  void SetNeedsRedrawRect(const gfx::Rect& rect) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;
  void DidLoseOutputSurface() override;
  void DidSwapBuffers() override;
  void DidSwapBuffersComplete() override;
  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override;
  void ReclaimResources(const CompositorFrameAck* ack) override;
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(const base::Closure& callback) override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              const gfx::Rect& clip,
              bool resourceless_software_draw) override;

  // LayerTreeMutatorClient.
  void SetNeedsMutate() override;

  // Called from LayerTreeImpl.
  void OnCanDrawStateChangedForTree();

  // Implementation.
  int id() const { return id_; }
  bool CanDraw() const;
  OutputSurface* output_surface() const { return output_surface_; }
  void ReleaseOutputSurface();

  std::string LayerTreeAsJson() const;

  void FinishAllRendering();
  int RequestedMSAASampleCount() const;

  virtual bool InitializeRenderer(OutputSurface* output_surface);
  TileManager* tile_manager() { return &tile_manager_; }

  void SetHasGpuRasterizationTrigger(bool flag) {
    has_gpu_rasterization_trigger_ = flag;
    UpdateGpuRasterizationStatus();
  }
  void SetContentIsSuitableForGpuRasterization(bool flag) {
    content_is_suitable_for_gpu_rasterization_ = flag;
    UpdateGpuRasterizationStatus();
  }
  bool CanUseGpuRasterization();
  void UpdateTreeResourcesForGpuRasterizationIfNeeded();
  bool use_gpu_rasterization() const { return use_gpu_rasterization_; }
  bool use_msaa() const { return use_msaa_; }

  GpuRasterizationStatus gpu_rasterization_status() const {
    return gpu_rasterization_status_;
  }

  bool create_low_res_tiling() const {
    return settings_.create_low_res_tiling && !use_gpu_rasterization_;
  }
  ResourcePool* resource_pool() { return resource_pool_.get(); }
  Renderer* renderer() { return renderer_.get(); }
  ImageDecodeController* image_decode_controller() {
    return image_decode_controller_.get();
  }
  const RendererCapabilitiesImpl& GetRendererCapabilities() const;

  virtual bool SwapBuffers(const FrameData& frame);
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
  LayerTreeImpl* sync_tree() {
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

  int scroll_layer_id_when_mouse_over_scrollbar() const {
    return scroll_layer_id_when_mouse_over_scrollbar_;
  }
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

  size_t memory_allocation_limit_bytes() const;

  void SetViewportSize(const gfx::Size& device_viewport_size);
  gfx::Size device_viewport_size() const { return device_viewport_size_; }

  const gfx::Transform& DrawTransform() const;

  std::unique_ptr<BeginFrameCallbackList> ProcessLayerTreeMutations();

  std::unique_ptr<ScrollAndScaleSet> ProcessScrollDeltas();

  void set_max_memory_needed_bytes(size_t bytes) {
    max_memory_needed_bytes_ = bytes;
  }

  FrameRateCounter* fps_counter() {
    return fps_counter_.get();
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

  TaskRunnerProvider* task_runner_provider() const {
    return task_runner_provider_;
  }

  AnimationHost* animation_host() const { return animation_host_.get(); }

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
    gfx::Size size;
    bool opaque;
  };

  // Returns the amount of delta that can be applied to scroll_node, taking
  // page scale into account.
  gfx::Vector2dF ComputeScrollDelta(ScrollNode* scroll_node,
                                    const gfx::Vector2dF& delta);

  void ScheduleMicroBenchmark(std::unique_ptr<MicroBenchmarkImpl> benchmark);

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
                             const gfx::Vector2dF& scroll_amount);

  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator);
  LayerTreeMutator* mutator() { return mutator_.get(); }

 protected:
  LayerTreeHostImpl(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<AnimationHost> animation_host,
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

  void CreateAndSetRenderer();
  void CleanUpTileManagerAndUIResources();
  void CreateTileManagerResources();
  void ReleaseTreeResources();
  void RecreateTreeResources();

  void AnimateInternal(bool active_tree);

  void UpdateGpuRasterizationStatus();

  Viewport* viewport() { return viewport_.get(); }

  // Scroll by preferring to move the outer viewport first, only moving the
  // inner if the outer is at its scroll extents.
  void ScrollViewportBy(gfx::Vector2dF scroll_delta);
  // Scroll by preferring to move the inner viewport first, only moving the
  // outer if the inner is at its scroll extents.
  void ScrollViewportInnerFirst(gfx::Vector2dF scroll_delta);

  InputHandler::ScrollStatus ScrollBeginImpl(
      ScrollState* scroll_state,
      LayerImpl* scrolling_layer_impl,
      InputHandler::ScrollInputType type);
  void DistributeScrollDelta(ScrollState* scroll_state);

  bool AnimatePageScale(base::TimeTicks monotonic_time);
  bool AnimateScrollbars(base::TimeTicks monotonic_time);
  bool AnimateTopControls(base::TimeTicks monotonic_time);

  void TrackDamageForAllSurfaces(
      const LayerImplList& render_surface_layer_list);

  void UpdateTileManagerMemoryPolicy(const ManagedMemoryPolicy& policy);

  // This function should only be called from PrepareToDraw, as DidDrawAllLayers
  // must be called if this helper function is called.  Returns DRAW_SUCCESS if
  // the frame should be drawn.
  DrawResult CalculateRenderPasses(FrameData* frame);

  void ClearCurrentlyScrollingLayer();

  void HandleMouseOverScrollbar(LayerImpl* layer_impl);

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
                                   const gfx::Vector2dF& scroll_delta);

  using UIResourceMap = std::unordered_map<UIResourceId, UIResourceData>;
  UIResourceMap ui_resource_map_;

  // Resources that were evicted by EvictAllUIResources. Resources are removed
  // from this when they are touched by a create or destroy from the UI resource
  // request queue.
  std::set<UIResourceId> evicted_ui_resources_;

  OutputSurface* output_surface_;

  std::unique_ptr<ResourceProvider> resource_provider_;
  bool content_is_suitable_for_gpu_rasterization_;
  bool has_gpu_rasterization_trigger_;
  bool use_gpu_rasterization_;
  bool use_msaa_;
  GpuRasterizationStatus gpu_rasterization_status_;
  bool tree_resources_for_gpu_rasterization_dirty_;
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  std::unique_ptr<TileTaskManager> tile_task_manager_;
  std::unique_ptr<ResourcePool> resource_pool_;
  std::unique_ptr<Renderer> renderer_;
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
  int scroll_layer_id_when_mouse_over_scrollbar_;
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

  std::unique_ptr<TopControlsManager> top_controls_manager_;

  std::unique_ptr<PageScaleAnimation> page_scale_animation_;

  std::unique_ptr<FrameRateCounter> fps_counter_;
  std::unique_ptr<MemoryHistory> memory_history_;
  std::unique_ptr<DebugRectHistory> debug_rect_history_;

  std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter_;

  // The maximum memory that would be used by the prioritized resource
  // manager, if there were no limit on memory usage.
  size_t max_memory_needed_bytes_;

  // Viewport size passed in from the main thread, in physical pixels.  This
  // value is the default size for all concepts of physical viewport (draw
  // viewport, scrolling viewport and device viewport), but it can be
  // overridden.
  gfx::Size device_viewport_size_;

  // Optional top-level constraints that can be set by the OutputSurface.
  // - external_transform_ applies a transform above the root layer
  // - external_viewport_ is used DrawProperties, tile management and
  // glViewport/window projection matrix.
  // - external_clip_ specifies a top-level clip rect
  // - viewport_rect_for_tile_priority_ is the rect in view space used for
  // tiling priority.
  gfx::Transform external_transform_;
  gfx::Rect external_viewport_;
  gfx::Rect external_clip_;
  gfx::Rect viewport_rect_for_tile_priority_;
  bool resourceless_software_draw_;

  gfx::Rect viewport_damage_rect_;

  std::unique_ptr<AnimationHost> animation_host_;
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

  SharedBitmapManager* shared_bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  TaskGraphRunner* task_graph_runner_;
  int id_;

  std::set<SwapPromiseMonitor*> swap_promise_monitor_;

  bool requires_high_res_to_draw_;
  bool is_likely_to_require_a_draw_;

  std::unique_ptr<Viewport> viewport_;

  std::unique_ptr<LayerTreeMutator> mutator_;

  std::unique_ptr<PendingTreeDurationHistogramTimer>
      pending_tree_duration_timer_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHostImpl);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_IMPL_H_
