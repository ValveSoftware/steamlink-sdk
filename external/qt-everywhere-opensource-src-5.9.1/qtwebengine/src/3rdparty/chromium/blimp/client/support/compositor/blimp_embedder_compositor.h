// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SUPPORT_COMPOSITOR_BLIMP_EMBEDDER_COMPOSITOR_H_
#define BLIMP_CLIENT_SUPPORT_COMPOSITOR_BLIMP_EMBEDDER_COMPOSITOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/layers/layer.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class AnimationHost;
class ContextProvider;
class Display;
class LayerTreeHost;
class SurfaceManager;
}  // namespace cc

namespace blimp {
namespace client {
class CompositorDependencies;

// The parent compositor that embeds the content from the BlimpCompositor for
// the current page and draws it to a display.
class BlimpEmbedderCompositor : public cc::LayerTreeHostClient,
                                public cc::LayerTreeHostSingleThreadClient {
 public:
  explicit BlimpEmbedderCompositor(
      CompositorDependencies* compositor_dependencies);
  ~BlimpEmbedderCompositor() override;

  // Sets the layer with the content from the renderer compositor.
  void SetContentLayer(scoped_refptr<cc::Layer> content_layer);

  // Sets the size for the display. Should be in physical pixels.
  void SetSize(const gfx::Size& size_in_px);

 protected:
  void SetContextProvider(scoped_refptr<cc::ContextProvider> context_provider);

  scoped_refptr<cc::Layer> root_layer() { return root_layer_; }
  CompositorDependencies* compositor_dependencies() {
    return compositor_dependencies_;
  }

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void UpdateLayerTreeHost() override {}
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewCompositorFrameSink() override;
  void DidInitializeCompositorFrameSink() override;
  void DidFailToInitializeCompositorFrameSink() override;
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {}
  void DidReceiveCompositorFrameAck() override {}
  void DidCompletePageScaleAnimation() override {}

  // LayerTreeHostSingleThreadClient implementation.
  void DidSubmitCompositorFrame() override {}
  void DidLoseCompositorFrameSink() override {}

 private:
  void HandlePendingCompositorFrameSinkRequest();

  CompositorDependencies* compositor_dependencies_;

  cc::FrameSinkId frame_sink_id_;

  scoped_refptr<cc::ContextProvider> context_provider_;

  bool compositor_frame_sink_request_pending_;
  std::unique_ptr<cc::Display> display_;

  gfx::Size viewport_size_in_px_;

  std::unique_ptr<cc::AnimationHost> animation_host_;
  std::unique_ptr<cc::LayerTreeHost> host_;
  scoped_refptr<cc::Layer> root_layer_;

  base::Closure did_complete_swap_buffers_;

  DISALLOW_COPY_AND_ASSIGN(BlimpEmbedderCompositor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SUPPORT_COMPOSITOR_BLIMP_EMBEDDER_COMPOSITOR_H_
