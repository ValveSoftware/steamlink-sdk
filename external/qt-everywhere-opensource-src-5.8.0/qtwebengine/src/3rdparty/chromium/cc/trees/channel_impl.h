// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_CHANNEL_IMPL_H_
#define CC_TREES_CHANNEL_IMPL_H_

#include "cc/base/cc_export.h"
#include "cc/output/renderer_capabilities.h"
#include "cc/trees/proxy_common.h"

namespace cc {

class AnimationEvents;

// Channel used to send commands to and receive commands from ProxyMain.
// The ChannelImpl implementation creates and owns ProxyImpl on receiving the
// InitializeImpl call from ChannelMain.
// See channel_main.h
class CC_EXPORT ChannelImpl {
 public:
  // Interface for commands sent to ProxyMain
  virtual void DidCompleteSwapBuffers() = 0;
  virtual void SetRendererCapabilitiesMainCopy(
      const RendererCapabilities& capabilities) = 0;
  virtual void BeginMainFrameNotExpectedSoon() = 0;
  virtual void DidCommitAndDrawFrame() = 0;
  virtual void SetAnimationEvents(std::unique_ptr<AnimationEvents> queue) = 0;
  virtual void DidLoseOutputSurface() = 0;
  virtual void RequestNewOutputSurface() = 0;
  virtual void DidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities) = 0;
  virtual void DidCompletePageScaleAnimation() = 0;
  virtual void BeginMainFrame(
      std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) = 0;

 protected:
  virtual ~ChannelImpl() {}
};

}  // namespace cc

#endif  // CC_TREES_CHANNEL_IMPL_H_
