// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_H_
#define CC_TREES_LAYER_TREE_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/animation/target_property.h"
#include "cc/base/cc_export.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/engine_picture_cache.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/debug/micro_benchmark_controller.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/input_handler.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/scrollbar.h"
#include "cc/input/top_controls_state.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/output/output_surface.h"
#include "cc/output/renderer_capabilities.h"
#include "cc/output/swap_promise.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/trees/compositor_mode.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/proxy.h"
#include "cc/trees/swap_promise_monitor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace cc {

class AnimationEvents;
class AnimationHost;
class BeginFrameSource;
class HeadsUpDisplayLayer;
class ImageSerializationProcessor;
class Layer;
class LayerTreeHostImpl;
class LayerTreeHostImplClient;
class LayerTreeHostSingleThreadClient;
class LayerTreeMutator;
class PropertyTrees;
class Region;
class RemoteProtoChannel;
class RenderingStatsInstrumentation;
class ResourceProvider;
class ResourceUpdateQueue;
class SharedBitmapManager;
class TaskGraphRunner;
class TopControlsManager;
class UIResourceRequest;
struct PendingPageScaleAnimation;
struct RenderingStats;
struct ScrollAndScaleSet;

namespace proto {
class LayerTreeHost;
}

class CC_EXPORT LayerTreeHost : public MutatorHostClient {
 public:
  // TODO(sad): InitParams should be a movable type so that it can be
  // std::move()d to the Create* functions.
  struct CC_EXPORT InitParams {
    LayerTreeHostClient* client = nullptr;
    SharedBitmapManager* shared_bitmap_manager = nullptr;
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr;
    TaskGraphRunner* task_graph_runner = nullptr;
    LayerTreeSettings const* settings = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner;
    std::unique_ptr<BeginFrameSource> external_begin_frame_source;
    ImageSerializationProcessor* image_serialization_processor = nullptr;
    std::unique_ptr<AnimationHost> animation_host;

    InitParams();
    ~InitParams();
  };

  // The SharedBitmapManager will be used on the compositor thread.
  static std::unique_ptr<LayerTreeHost> CreateThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams* params);

  static std::unique_ptr<LayerTreeHost> CreateSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      InitParams* params);

  static std::unique_ptr<LayerTreeHost> CreateRemoteServer(
      RemoteProtoChannel* remote_proto_channel,
      InitParams* params);

  // The lifetime of this LayerTreeHost is tied to the lifetime of the remote
  // server LayerTreeHost. It should be created on receiving
  // CompositorMessageToImpl::InitializeImpl message and destroyed on receiving
  // a CompositorMessageToImpl::CloseImpl message from the server. This ensures
  // that the client will not send any compositor messages once the
  // LayerTreeHost on the server is destroyed.
  static std::unique_ptr<LayerTreeHost> CreateRemoteClient(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams* params);

  virtual ~LayerTreeHost();

  // LayerTreeHost interface to Proxy.
  void WillBeginMainFrame();
  void DidBeginMainFrame();
  void BeginMainFrame(const BeginFrameArgs& args);
  void BeginMainFrameNotExpectedSoon();
  void AnimateLayers(base::TimeTicks monotonic_frame_begin_time);
  void DidStopFlinging();
  void RequestMainFrameUpdate();
  void FinishCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void WillCommit();
  void CommitComplete();
  void SetOutputSurface(std::unique_ptr<OutputSurface> output_surface);
  std::unique_ptr<OutputSurface> ReleaseOutputSurface();
  void RequestNewOutputSurface();
  void DidInitializeOutputSurface();
  void DidFailToInitializeOutputSurface();
  virtual std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* client);
  void DidLoseOutputSurface();
  bool output_surface_lost() const { return output_surface_lost_; }
  void DidCommitAndDrawFrame() { client_->DidCommitAndDrawFrame(); }
  void DidCompleteSwapBuffers() { client_->DidCompleteSwapBuffers(); }
  bool UpdateLayers();

  LayerListIterator<Layer> begin() const;
  LayerListIterator<Layer> end() const;
  LayerListReverseIterator<Layer> rbegin();
  LayerListReverseIterator<Layer> rend();

  // Called when the compositor completed page scale animation.
  void DidCompletePageScaleAnimation();

  LayerTreeHostClient* client() { return client_; }
  const base::WeakPtr<InputHandler>& GetInputHandler() {
    return input_handler_weak_ptr_;
  }

  void NotifyInputThrottledUntilCommit();

  void LayoutAndUpdateLayers();
  void Composite(base::TimeTicks frame_begin_time);

  void FinishAllRendering();

  void SetDeferCommits(bool defer_commits);

  int source_frame_number() const { return source_frame_number_; }

  bool gpu_rasterization_histogram_recorded() const {
    return gpu_rasterization_histogram_recorded_;
  }

  void SetNeedsDisplayOnAllLayers();

  void CollectRenderingStats(RenderingStats* stats) const;

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const {
    return rendering_stats_instrumentation_.get();
  }

  virtual const RendererCapabilities& GetRendererCapabilities() const;

  void SetNeedsAnimate();
  virtual void SetNeedsUpdateLayers();
  virtual void SetNeedsCommit();
  virtual void SetNeedsFullTreeSync();
  virtual void SetNeedsMetaInfoRecomputation(
      bool needs_meta_info_recomputation);
  void SetNeedsRedraw();
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect);
  bool CommitRequested() const;
  bool BeginMainFrameRequested() const;

  void SetNextCommitWaitsForActivation();

  void SetNextCommitForcesRedraw();

  void SetAnimationEvents(std::unique_ptr<AnimationEvents> events);

  void SetRootLayer(scoped_refptr<Layer> root_layer);
  Layer* root_layer() { return root_layer_.get(); }
  const Layer* root_layer() const { return root_layer_.get(); }
  const Layer* overscroll_elasticity_layer() const {
    return overscroll_elasticity_layer_.get();
  }
  const Layer* page_scale_layer() const { return page_scale_layer_.get(); }
  void RegisterViewportLayers(scoped_refptr<Layer> overscroll_elasticity_layer,
                              scoped_refptr<Layer> page_scale_layer,
                              scoped_refptr<Layer> inner_viewport_scroll_layer,
                              scoped_refptr<Layer> outer_viewport_scroll_layer);
  Layer* inner_viewport_scroll_layer() const {
    return inner_viewport_scroll_layer_.get();
  }
  Layer* outer_viewport_scroll_layer() const {
    return outer_viewport_scroll_layer_.get();
  }

  void RegisterSelection(const LayerSelection& selection);

  bool have_scroll_event_handlers() const {
    return have_scroll_event_handlers_;
  }
  void SetHaveScrollEventHandlers(bool have_event_handlers);

  void SetEventListenerProperties(EventListenerClass event_class,
                                  EventListenerProperties event_properties);
  EventListenerProperties event_listener_properties(
      EventListenerClass event_class) const {
    return event_listener_properties_[static_cast<size_t>(event_class)];
  }

  const LayerTreeSettings& settings() const { return settings_; }

  void SetDebugState(const LayerTreeDebugState& debug_state);
  const LayerTreeDebugState& debug_state() const { return debug_state_; }

  bool has_gpu_rasterization_trigger() const {
    return has_gpu_rasterization_trigger_;
  }
  void SetHasGpuRasterizationTrigger(bool has_trigger);

  void SetViewportSize(const gfx::Size& device_viewport_size);
  void SetTopControlsHeight(float height, bool shrink);
  void SetTopControlsShownRatio(float ratio);

  gfx::Size device_viewport_size() const { return device_viewport_size_; }

  void ApplyPageScaleDeltaFromImplSide(float page_scale_delta);
  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);
  float page_scale_factor() const { return page_scale_factor_; }
  gfx::Vector2dF elastic_overscroll() const { return elastic_overscroll_; }

  SkColor background_color() const { return background_color_; }
  void set_background_color(SkColor color) { background_color_ = color; }

  void set_has_transparent_background(bool transparent) {
    has_transparent_background_ = transparent;
  }

  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool use_anchor,
                               float scale,
                               base::TimeDelta duration);
  bool HasPendingPageScaleAnimation() const;

  void ApplyScrollAndScale(ScrollAndScaleSet* info);
  void SetImplTransform(const gfx::Transform& transform);

  void SetDeviceScaleFactor(float device_scale_factor);
  void SetPaintedDeviceScaleFactor(float painted_device_scale_factor);

  float device_scale_factor() const { return device_scale_factor_; }

  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
                              bool animate);

  HeadsUpDisplayLayer* hud_layer() const { return hud_layer_.get(); }

  Proxy* proxy() const { return proxy_.get(); }
  TaskRunnerProvider* task_runner_provider() const {
    return task_runner_provider_.get();
  }
  AnimationHost* animation_host() const { return animation_host_.get(); }

  bool in_paint_layer_contents() const { return in_paint_layer_contents_; }

  // CreateUIResource creates a resource given a bitmap.  The bitmap is
  // generated via an interface function, which is called when initializing the
  // resource and when the resource has been lost (due to lost context).  The
  // parameter of the interface is a single boolean, which indicates whether the
  // resource has been lost or not.  CreateUIResource returns an Id of the
  // resource, which is always positive.
  virtual UIResourceId CreateUIResource(UIResourceClient* client);
  // Deletes a UI resource.  May safely be called more than once.
  virtual void DeleteUIResource(UIResourceId id);
  // Put the recreation of all UI resources into the resource queue after they
  // were evicted on the impl thread.
  void RecreateUIResources();

  virtual gfx::Size GetUIResourceSize(UIResourceId id) const;

  bool UsingSharedMemoryResources();
  int id() const { return id_; }

  // Returns the id of the benchmark on success, 0 otherwise.
  int ScheduleMicroBenchmark(const std::string& benchmark_name,
                             std::unique_ptr<base::Value> value,
                             const MicroBenchmark::DoneCallback& callback);
  // Returns true if the message was successfully delivered and handled.
  bool SendMessageToMicroBenchmark(int id, std::unique_ptr<base::Value> value);

  // When a SwapPromiseMonitor is created on the main thread, it calls
  // InsertSwapPromiseMonitor() to register itself with LayerTreeHost.
  // When the monitor is destroyed, it calls RemoveSwapPromiseMonitor()
  // to unregister itself.
  void InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor);
  void RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor);

  // Call this function when you expect there to be a swap buffer.
  // See swap_promise.h for how to use SwapPromise.
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  void BreakSwapPromises(SwapPromise::DidNotSwapReason reason);

  size_t num_queued_swap_promises() const { return swap_promise_list_.size(); }

  void set_surface_id_namespace(uint32_t id_namespace);
  SurfaceSequence CreateSurfaceSequence();

  PropertyTrees* property_trees() { return &property_trees_; }
  bool needs_meta_info_recomputation() {
    return needs_meta_info_recomputation_;
  }

  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator);

  Layer* LayerById(int id) const;

  Layer* LayerByElementId(ElementId element_id) const;
  void AddToElementMap(Layer* layer);
  void RemoveFromElementMap(Layer* layer);

  void AddLayerShouldPushProperties(Layer* layer);
  void RemoveLayerShouldPushProperties(Layer* layer);
  std::unordered_set<Layer*>& LayersThatShouldPushProperties();
  bool LayerNeedsPushPropertiesForTesting(Layer* layer);

  void RegisterLayer(Layer* layer);
  void UnregisterLayer(Layer* layer);
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
  void ScrollOffsetAnimationFinished() override {}
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

  bool ScrollOffsetAnimationWasInterrupted(const Layer* layer) const;
  bool IsAnimatingFilterProperty(const Layer* layer) const;
  bool IsAnimatingOpacityProperty(const Layer* layer) const;
  bool IsAnimatingTransformProperty(const Layer* layer) const;
  bool HasPotentiallyRunningFilterAnimation(const Layer* layer) const;
  bool HasPotentiallyRunningOpacityAnimation(const Layer* layer) const;
  bool HasPotentiallyRunningTransformAnimation(const Layer* layer) const;
  bool HasOnlyTranslationTransforms(const Layer* layer) const;
  bool MaximumTargetScale(const Layer* layer, float* max_scale) const;
  bool AnimationStartScale(const Layer* layer, float* start_scale) const;
  bool HasAnyAnimationTargetingProperty(const Layer* layer,
                                        TargetProperty::Type property) const;
  bool AnimationsPreserveAxisAlignment(const Layer* layer) const;
  bool HasAnyAnimation(const Layer* layer) const;
  bool HasActiveAnimationForTesting(const Layer* layer) const;

  // Serializes the parts of this LayerTreeHost that is needed for a commit to a
  // protobuf message. Not all members are serialized as they are not helpful
  // for remote usage.
  // The |swap_promise_list_| is transferred to the serializer in
  // |swap_promises|.
  void ToProtobufForCommit(
      proto::LayerTreeHost* proto,
      std::vector<std::unique_ptr<SwapPromise>>* swap_promises);

  // Deserializes the protobuf into this LayerTreeHost before a commit. The
  // expected input is a serialized remote LayerTreeHost. After deserializing
  // the protobuf, the normal commit-flow should continue.
  void FromProtobufForCommit(const proto::LayerTreeHost& proto);

  bool IsSingleThreaded() const;
  bool IsThreaded() const;
  bool IsRemoteServer() const;
  bool IsRemoteClient() const;
  void BuildPropertyTreesForTesting();

  void SetElementIdsForTesting();

  ImageSerializationProcessor* image_serialization_processor() const {
    return image_serialization_processor_;
  }

  EnginePictureCache* engine_picture_cache() const {
    return engine_picture_cache_ ? engine_picture_cache_.get() : nullptr;
  }

  ClientPictureCache* client_picture_cache() const {
    return client_picture_cache_ ? client_picture_cache_.get() : nullptr;
  }

 protected:
  LayerTreeHost(InitParams* params, CompositorMode mode);
  void InitializeThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);
  void InitializeSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);
  void InitializeRemoteServer(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  void InitializeRemoteClient(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  void InitializeForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider,
      std::unique_ptr<Proxy> proxy_for_testing,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);
  void InitializePictureCacheForTesting();
  void SetOutputSurfaceLostForTesting(bool is_lost) {
    output_surface_lost_ = is_lost;
  }
  void SetTaskRunnerProviderForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider);

  // shared_bitmap_manager(), gpu_memory_buffer_manager(), and
  // task_graph_runner() return valid values only until the LayerTreeHostImpl is
  // created in CreateLayerTreeHostImpl().
  SharedBitmapManager* shared_bitmap_manager() const {
    return shared_bitmap_manager_;
  }
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_;
  }
  TaskGraphRunner* task_graph_runner() const { return task_graph_runner_; }

  MicroBenchmarkController micro_benchmark_controller_;

  void OnCommitForSwapPromises();

  void RecordGpuRasterizationHistogram();

 private:
  friend class LayerTreeHostSerializationTest;

  void InitializeProxy(
      std::unique_ptr<Proxy> proxy,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);

  bool DoUpdateLayers(Layer* root_layer);
  void UpdateHudLayer();

  bool AnimateLayersRecursive(Layer* current, base::TimeTicks time);

  struct UIResourceClientData {
    UIResourceClient* client;
    gfx::Size size;
  };

  using UIResourceClientMap =
      std::unordered_map<UIResourceId, UIResourceClientData>;
  UIResourceClientMap ui_resource_client_map_;
  int next_ui_resource_id_;

  using UIResourceRequestQueue = std::vector<UIResourceRequest>;
  UIResourceRequestQueue ui_resource_request_queue_;

  void CalculateLCDTextMetricsCallback(Layer* layer);

  void NotifySwapPromiseMonitorsOfSetNeedsCommit();

  void SetPropertyTreesNeedRebuild();

  const CompositorMode compositor_mode_;

  bool needs_full_tree_sync_;
  bool needs_meta_info_recomputation_;

  LayerTreeHostClient* client_;
  std::unique_ptr<Proxy> proxy_;
  std::unique_ptr<TaskRunnerProvider> task_runner_provider_;

  int source_frame_number_;
  std::unique_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation_;

  // |current_output_surface_| can't be updated until we've successfully
  // initialized a new output surface. |new_output_surface_| contains the
  // new output surface that is currently being initialized. If initialization
  // is successful then |new_output_surface_| replaces
  // |current_output_surface_|.
  std::unique_ptr<OutputSurface> new_output_surface_;
  std::unique_ptr<OutputSurface> current_output_surface_;
  bool output_surface_lost_;

  scoped_refptr<Layer> root_layer_;
  scoped_refptr<HeadsUpDisplayLayer> hud_layer_;

  base::WeakPtr<InputHandler> input_handler_weak_ptr_;

  const LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;

  gfx::Size device_viewport_size_;
  bool top_controls_shrink_blink_size_;
  float top_controls_height_;
  float top_controls_shown_ratio_;
  float device_scale_factor_;
  float painted_device_scale_factor_;

  bool visible_;

  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  gfx::Vector2dF elastic_overscroll_;
  bool has_gpu_rasterization_trigger_;
  bool content_is_suitable_for_gpu_rasterization_;
  bool gpu_rasterization_histogram_recorded_;

  SkColor background_color_;
  bool has_transparent_background_;

  bool have_scroll_event_handlers_;
  EventListenerProperties event_listener_properties_[static_cast<size_t>(
      EventListenerClass::kNumClasses)];

  std::unique_ptr<AnimationHost> animation_host_;

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation_;

  // If set, then page scale animation has completed, but the client hasn't been
  // notified about it yet.
  bool did_complete_scale_animation_;

  bool in_paint_layer_contents_;

  int id_;
  bool next_commit_forces_redraw_;

  scoped_refptr<Layer> overscroll_elasticity_layer_;
  scoped_refptr<Layer> page_scale_layer_;
  scoped_refptr<Layer> inner_viewport_scroll_layer_;
  scoped_refptr<Layer> outer_viewport_scroll_layer_;

  LayerSelection selection_;

  SharedBitmapManager* shared_bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  TaskGraphRunner* task_graph_runner_;

  ImageSerializationProcessor* image_serialization_processor_;
  std::unique_ptr<EnginePictureCache> engine_picture_cache_;
  std::unique_ptr<ClientPictureCache> client_picture_cache_;

  std::vector<std::unique_ptr<SwapPromise>> swap_promise_list_;
  std::set<SwapPromiseMonitor*> swap_promise_monitor_;

  PropertyTrees property_trees_;

  using LayerIdMap = std::unordered_map<int, Layer*>;
  LayerIdMap layer_id_map_;

  using ElementLayersMap = std::unordered_map<ElementId, Layer*, ElementIdHash>;
  ElementLayersMap element_layers_map_;

  // Set of layers that need to push properties.
  std::unordered_set<Layer*> layers_that_should_push_properties_;

  uint32_t surface_id_namespace_;
  uint32_t next_surface_sequence_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHost);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_
