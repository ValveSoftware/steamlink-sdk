// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_
#define CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/input/top_controls_state.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/swap_promise.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/remote_proto_channel.h"
#include "cc/trees/swap_promise_monitor.h"
#include "content/common/content_export.h"
#include "content/renderer/gpu/compositor_dependencies.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class CommandLine;
}

namespace cc {
class CopyOutputRequest;
class InputHandler;
class Layer;
class LayerTreeHost;
namespace proto {
class CompositorMessage;
}
}

namespace ui {
class LatencyInfo;
}

namespace content {

class RenderWidgetCompositorDelegate;

class CONTENT_EXPORT RenderWidgetCompositor
    : NON_EXPORTED_BASE(public blink::WebLayerTreeView),
      NON_EXPORTED_BASE(public cc::LayerTreeHostClient),
      NON_EXPORTED_BASE(public cc::LayerTreeHostSingleThreadClient),
      NON_EXPORTED_BASE(public cc::RemoteProtoChannel) {
 public:
  // Attempt to construct and initialize a compositor instance for the widget
  // with the given settings. Returns NULL if initialization fails.
  static std::unique_ptr<RenderWidgetCompositor> Create(
      RenderWidgetCompositorDelegate* delegate,
      float device_scale_factor,
      CompositorDependencies* compositor_deps);

  ~RenderWidgetCompositor() override;

  static cc::LayerTreeSettings GenerateLayerTreeSettings(
      const base::CommandLine& cmd,
      CompositorDependencies* compositor_deps,
      float device_scale_factor);
  static cc::ManagedMemoryPolicy GetGpuMemoryPolicy(
      const cc::ManagedMemoryPolicy& policy);

  void SetNeverVisible();
  const base::WeakPtr<cc::InputHandler>& GetInputHandler();
  bool BeginMainFrameRequested() const;
  void SetNeedsDisplayOnAllLayers();
  void SetRasterizeOnlyVisibleContent();
  void SetNeedsRedrawRect(gfx::Rect damage_rect);
  // Like setNeedsRedraw but forces the frame to be drawn, without early-outs.
  // Redraw will be forced after the next commit
  void SetNeedsForcedRedraw();
  // Calling CreateLatencyInfoSwapPromiseMonitor() to get a scoped
  // LatencyInfoSwapPromiseMonitor. During the life time of the
  // LatencyInfoSwapPromiseMonitor, if SetNeedsCommit() or
  // SetNeedsUpdateLayers() is called on LayerTreeHost, the original latency
  // info will be turned into a LatencyInfoSwapPromise.
  std::unique_ptr<cc::SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency);
  // Calling QueueSwapPromise() to directly queue a SwapPromise into
  // LayerTreeHost.
  void QueueSwapPromise(std::unique_ptr<cc::SwapPromise> swap_promise);
  int GetSourceFrameNumber() const;
  void SetNeedsUpdateLayers();
  void SetNeedsCommit();
  void NotifyInputThrottledUntilCommit();
  const cc::Layer* GetRootLayer() const;
  int ScheduleMicroBenchmark(
      const std::string& name,
      std::unique_ptr<base::Value> value,
      const base::Callback<void(std::unique_ptr<base::Value>)>& callback);
  bool SendMessageToMicroBenchmark(int id, std::unique_ptr<base::Value> value);
  void SetSurfaceIdNamespace(uint32_t surface_id_namespace);
  void OnHandleCompositorProto(const std::vector<uint8_t>& proto);
  void SetPaintedDeviceScaleFactor(float device_scale);

  // WebLayerTreeView implementation.
  void setRootLayer(const blink::WebLayer& layer) override;
  void clearRootLayer() override;
  void attachCompositorAnimationTimeline(
      cc::AnimationTimeline* compositor_timeline) override;
  void detachCompositorAnimationTimeline(
      cc::AnimationTimeline* compositor_timeline) override;
  void setViewportSize(const blink::WebSize& device_viewport_size) override;
  virtual blink::WebFloatPoint adjustEventPointForPinchZoom(
      const blink::WebFloatPoint& point) const;
  void setDeviceScaleFactor(float device_scale) override;
  void setBackgroundColor(blink::WebColor color) override;
  void setHasTransparentBackground(bool transparent) override;
  void setVisible(bool visible) override;
  void setPageScaleFactorAndLimits(float page_scale_factor,
                                   float minimum,
                                   float maximum) override;
  void startPageScaleAnimation(const blink::WebPoint& destination,
                               bool use_anchor,
                               float new_page_scale,
                               double duration_sec) override;
  bool hasPendingPageScaleAnimation() const override;
  void heuristicsForGpuRasterizationUpdated(bool matches_heuristics) override;
  void setNeedsAnimate() override;
  void setNeedsBeginFrame() override;
  void setNeedsCompositorUpdate() override;
  void didStopFlinging() override;
  void layoutAndPaintAsync(
      blink::WebLayoutAndPaintAsyncCallback* callback) override;
  void compositeAndReadbackAsync(
      blink::WebCompositeAndReadbackAsyncCallback* callback) override;
  void setDeferCommits(bool defer_commits) override;
  void registerViewportLayers(
      const blink::WebLayer* overscrollElasticityLayer,
      const blink::WebLayer* pageScaleLayer,
      const blink::WebLayer* innerViewportScrollLayer,
      const blink::WebLayer* outerViewportScrollLayer) override;
  void clearViewportLayers() override;
  void registerSelection(const blink::WebSelection& selection) override;
  void clearSelection() override;
  void setMutatorClient(
      std::unique_ptr<blink::WebCompositorMutatorClient>) override;
  void setEventListenerProperties(
      blink::WebEventListenerClass eventClass,
      blink::WebEventListenerProperties properties) override;
  blink::WebEventListenerProperties eventListenerProperties(
      blink::WebEventListenerClass eventClass) const override;
  void setHaveScrollEventHandlers(bool) override;
  bool haveScrollEventHandlers() const override;
  int layerTreeId() const override;
  void setShowFPSCounter(bool show) override;
  void setShowPaintRects(bool show) override;
  void setShowDebugBorders(bool show) override;
  void setShowScrollBottleneckRects(bool show) override;

  void updateTopControlsState(blink::WebTopControlsState constraints,
                              blink::WebTopControlsState current,
                              bool animate) override;
  void setTopControlsHeight(float height, bool shrink) override;
  void setTopControlsShownRatio(float) override;

  // cc::LayerTreeHostClient implementation.
  void WillBeginMainFrame() override;
  void DidBeginMainFrame() override;
  void BeginMainFrame(const cc::BeginFrameArgs& args) override;
  void BeginMainFrameNotExpectedSoon() override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override;
  void DidFailToInitializeOutputSurface() override;
  void WillCommit() override;
  void DidCommit() override;
  void DidCommitAndDrawFrame() override;
  void DidCompleteSwapBuffers() override;
  void DidCompletePageScaleAnimation() override;

  // cc::LayerTreeHostSingleThreadClient implementation.
  void RequestScheduleAnimation() override;
  void DidPostSwapBuffers() override;
  void DidAbortSwapBuffers() override;

  // cc::RemoteProtoChannel implementation.
  void SetProtoReceiver(ProtoReceiver* receiver) override;
  void SendCompositorProto(const cc::proto::CompositorMessage& proto) override;

  enum {
    OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK = 4,
    MAX_OUTPUT_SURFACE_RETRIES = 5,
  };

 protected:
  friend class RenderViewImplScaleFactorTest;

  RenderWidgetCompositor(RenderWidgetCompositorDelegate* delegate,
                         CompositorDependencies* compositor_deps);

  void Initialize(float device_scale_factor);
  cc::LayerTreeHost* layer_tree_host() { return layer_tree_host_.get(); }

 private:
  void LayoutAndUpdateLayers();
  void InvokeLayoutAndPaintCallback();
  bool CompositeIsSynchronous() const;
  void SynchronouslyComposite();

  int num_failed_recreate_attempts_;
  RenderWidgetCompositorDelegate* const delegate_;
  CompositorDependencies* const compositor_deps_;
  const bool threaded_;
  std::unique_ptr<cc::LayerTreeHost> layer_tree_host_;
  bool never_visible_;

  blink::WebLayoutAndPaintAsyncCallback* layout_and_paint_async_callback_;
  std::unique_ptr<cc::CopyOutputRequest> temporary_copy_output_request_;

  cc::RemoteProtoChannel::ProtoReceiver* remote_proto_channel_receiver_;

  base::WeakPtrFactory<RenderWidgetCompositor> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_RENDER_WIDGET_COMPOSITOR_H_
