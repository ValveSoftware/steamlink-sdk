// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_CLIENT_H_
#define CC_TREES_LAYER_TREE_HOST_CLIENT_H_

#include <memory>

#include "base/memory/ref_counted.h"

namespace gfx {
class Vector2dF;
}

namespace cc {
class CompositorFrameSink;
struct BeginFrameArgs;

class LayerTreeHostClient {
 public:
  virtual void WillBeginMainFrame() = 0;
  // Marks finishing compositing-related tasks on the main thread. In threaded
  // mode, this corresponds to DidCommit().
  virtual void BeginMainFrame(const BeginFrameArgs& args) = 0;
  virtual void BeginMainFrameNotExpectedSoon() = 0;
  virtual void DidBeginMainFrame() = 0;
  // A LayerTreeHost is bound to a LayerTreeHostClient. Visual frame-based
  // updates to the state of the LayerTreeHost are expected to happen only in
  // calls to LayerTreeHostClient::UpdateLayerTreeHost, which should
  // mutate/invalidate the layer tree or other page parameters as appropriate.
  //
  // An example of a LayerTreeHostClient is (via additional indirections) Blink,
  // which inside of LayerTreeHostClient::UpdateLayerTreeHost will update
  // (Blink's notions of) style, layout, paint invalidation and compositing
  // state. (The "compositing state" will result in a mutated layer tree on the
  // LayerTreeHost via additional interface indirections which lead back to
  // mutations on the LayerTreeHost.)
  virtual void UpdateLayerTreeHost() = 0;
  virtual void ApplyViewportDeltas(
      const gfx::Vector2dF& inner_delta,
      const gfx::Vector2dF& outer_delta,
      const gfx::Vector2dF& elastic_overscroll_delta,
      float page_scale,
      float top_controls_delta) = 0;
  // Request an CompositorFrameSink from the client. When the client has one it
  // should call LayerTreeHost::SetCompositorFrameSink.  This will result in
  // either DidFailToInitializeCompositorFrameSink or
  // DidInitializeCompositorFrameSink being called.
  virtual void RequestNewCompositorFrameSink() = 0;
  virtual void DidInitializeCompositorFrameSink() = 0;
  virtual void DidFailToInitializeCompositorFrameSink() = 0;
  virtual void WillCommit() = 0;
  virtual void DidCommit() = 0;
  virtual void DidCommitAndDrawFrame() = 0;
  virtual void DidReceiveCompositorFrameAck() = 0;
  virtual void DidCompletePageScaleAnimation() = 0;

 protected:
  virtual ~LayerTreeHostClient() {}
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_CLIENT_H_
