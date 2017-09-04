// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_FRAME_SINK_PROXY_H_
#define BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_FRAME_SINK_PROXY_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/resources/returned_resource.h"

namespace cc {
class CompositorFrame;
}

namespace blimp {
namespace client {

class BlimpCompositorFrameSinkProxyClient {
 public:
  virtual ~BlimpCompositorFrameSinkProxyClient() {}

  // Sent when the compositor can reclaim the |resources| references in a
  // compositor frame.
  virtual void ReclaimCompositorResources(
      const cc::ReturnedResourceArray& resources) = 0;

  // Called when SubmitCompositorFrame has been processed and another frame
  // should be started.
  virtual void SubmitCompositorFrameAck() = 0;
};

// This class is meant to bounce CompositorFrames from the compositor thread's
// CompositorFrameSink to the main thread. As such, it should be used on the
// main thread only.
class BlimpCompositorFrameSinkProxy {
 public:
  virtual ~BlimpCompositorFrameSinkProxy() {}

  // Will be called before any frames are sent to the client. The weak ptr
  // provided here can be used to post messages to the CompositorFrameSink on
  // the compositor thread.
  virtual void BindToProxyClient(
      base::WeakPtr<BlimpCompositorFrameSinkProxyClient> client) = 0;

  // The implementation should take the contents of the compositor frame and
  // return the referenced resources when the frame is no longer being drawn
  // using BlimpCompositorFrameSinkProxyClient::ReclaimCompositorResources.
  virtual void SubmitCompositorFrame(cc::CompositorFrame frame) = 0;

  // Will be called when the use of the CompositorFrameSink is being terminated.
  // No calls from this output surface will be made to the client after this
  // point.
  virtual void UnbindProxyClient() = 0;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_COMPOSITOR_BLIMP_COMPOSITOR_FRAME_SINK_PROXY_H_
