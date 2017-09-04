// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLayerTreeViewImplForTesting_h
#define WebLayerTreeViewImplForTesting_h

#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "public/platform/WebLayerTreeView.h"
#include <memory>

namespace cc {
class AnimationHost;
class LayerTreeHost;
class LayerTreeSettings;
}

namespace blink {

class WebLayer;

// Dummy WeblayerTeeView that does not support any actual compositing.
class WebLayerTreeViewImplForTesting
    : public blink::WebLayerTreeView,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient {
  WTF_MAKE_NONCOPYABLE(WebLayerTreeViewImplForTesting);

 public:
  WebLayerTreeViewImplForTesting();
  explicit WebLayerTreeViewImplForTesting(const cc::LayerTreeSettings&);
  ~WebLayerTreeViewImplForTesting() override;

  static cc::LayerTreeSettings defaultLayerTreeSettings();
  cc::LayerTreeHost* layerTreeHost() { return m_layerTreeHost.get(); }
  bool hasLayer(const WebLayer&);

  // blink::WebLayerTreeView implementation.
  void setRootLayer(const blink::WebLayer&) override;
  void clearRootLayer() override;
  void attachCompositorAnimationTimeline(cc::AnimationTimeline*) override;
  void detachCompositorAnimationTimeline(cc::AnimationTimeline*) override;
  virtual void setViewportSize(const blink::WebSize& unusedDeprecated,
                               const blink::WebSize& deviceViewportSize);
  void setViewportSize(const blink::WebSize&) override;
  WebSize getViewportSize() const override;
  void setDeviceScaleFactor(float) override;
  void setBackgroundColor(blink::WebColor) override;
  void setHasTransparentBackground(bool) override;
  void setVisible(bool) override;
  void setPageScaleFactorAndLimits(float pageScaleFactor,
                                   float minimum,
                                   float maximum) override;
  void startPageScaleAnimation(const blink::WebPoint& destination,
                               bool useAnchor,
                               float newPageScale,
                               double durationSec) override;
  void setNeedsBeginFrame() override;
  void setNeedsCompositorUpdate() override;
  void didStopFlinging() override;
  void setDeferCommits(bool) override;
  void registerViewportLayers(
      const blink::WebLayer* overscrollElasticityLayer,
      const blink::WebLayer* pageScaleLayerLayer,
      const blink::WebLayer* innerViewportScrollLayer,
      const blink::WebLayer* outerViewportScrollLayer) override;
  void clearViewportLayers() override;
  void registerSelection(const blink::WebSelection&) override;
  void clearSelection() override;
  void setEventListenerProperties(blink::WebEventListenerClass eventClass,
                                  blink::WebEventListenerProperties) override;
  blink::WebEventListenerProperties eventListenerProperties(
      blink::WebEventListenerClass eventClass) const override;
  void setHaveScrollEventHandlers(bool) override;
  bool haveScrollEventHandlers() const override;

  // cc::LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& innerDelta,
                           const gfx::Vector2dF& outerDelta,
                           const gfx::Vector2dF& elasticOverscrollDelta,
                           float pageScale,
                           float browserControlsDelta) override;
  void RequestNewCompositorFrameSink() override;
  void DidInitializeCompositorFrameSink() override {}
  void DidFailToInitializeCompositorFrameSink() override;
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {}
  void DidReceiveCompositorFrameAck() override {}
  void DidCompletePageScaleAnimation() override {}

  // cc::LayerTreeHostSingleThreadClient implementation.
  void DidSubmitCompositorFrame() override {}
  void DidLoseCompositorFrameSink() override {}

 private:
  cc::TestTaskGraphRunner m_taskGraphRunner;
  std::unique_ptr<cc::AnimationHost> m_animationHost;
  std::unique_ptr<cc::LayerTreeHost> m_layerTreeHost;
};

}  // namespace blink

#endif  // WebLayerTreeViewImplForTesting_h
