// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COMPOSITOR_FRAME_SINK_CLIENT_H_
#define CC_OUTPUT_COMPOSITOR_FRAME_SINK_CLIENT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/resources/returned_resource.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Transform;
}

namespace cc {

class BeginFrameSource;
struct ManagedMemoryPolicy;

class CC_EXPORT CompositorFrameSinkClient {
 public:
  // Pass the begin frame source for the client to observe.  Client does not own
  // the BeginFrameSource.  CompositorFrameSink should call this once after
  // binding to the client and then call again with a null while detaching.
  virtual void SetBeginFrameSource(BeginFrameSource* source) = 0;

  // Returns resources sent to SubmitCompositorFrame to be reused or freed.
  virtual void ReclaimResources(const ReturnedResourceArray& resources) = 0;

  // If set, |callback| will be called subsequent to each new tree activation,
  // regardless of the compositor visibility or damage. |callback| must remain
  // valid for the lifetime of the CompositorFrameSinkClient or until
  // unregistered by giving a null base::Closure.
  virtual void SetTreeActivationCallback(const base::Closure& callback) = 0;

  // Notification that the previous CompositorFrame given to
  // SubmitCompositorFrame() has been processed and that another frame
  // can be submitted. This provides backpressure from the display compositor
  // so that frames are submitted only at the rate it can handle them.
  virtual void DidReceiveCompositorFrameAck() = 0;

  // The CompositorFrameSink is lost when the ContextProviders held by it
  // encounter an error. In this case the CompositorFrameSink (and the
  // ContextProviders) must be recreated.
  virtual void DidLoseCompositorFrameSink() = 0;

  // For SynchronousCompositor (WebView) to ask the layer compositor to submit
  // a new CompositorFrame synchronously.
  virtual void OnDraw(const gfx::Transform& transform,
                      const gfx::Rect& viewport,
                      bool resourceless_software_draw) = 0;

  // For SynchronousCompositor (WebView) to set how much memory the compositor
  // can use without changing visibility.
  virtual void SetMemoryPolicy(const ManagedMemoryPolicy& policy) = 0;

  // For SynchronousCompositor (WebView) to change which tiles should be
  // included in submitted CompositorFrames independently of what the viewport
  // is.
  virtual void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) = 0;

 protected:
  virtual ~CompositorFrameSinkClient() {}
};

}  // namespace cc

#endif  // CC_OUTPUT_COMPOSITOR_FRAME_SINK_CLIENT_H_
