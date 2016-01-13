// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_INPUT_HANDLER_H_
#define CC_INPUT_INPUT_HANDLER_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/base/swap_promise_monitor.h"
#include "cc/input/scrollbar.h"

namespace gfx {
class Point;
class PointF;
class Vector2d;
class Vector2dF;
}

namespace ui { struct LatencyInfo; }

namespace cc {

class LayerScrollOffsetDelegate;

class CC_EXPORT InputHandlerClient {
 public:
  virtual ~InputHandlerClient() {}

  virtual void WillShutdown() = 0;
  virtual void Animate(base::TimeTicks time) = 0;
  virtual void MainThreadHasStoppedFlinging() = 0;

  // Called when scroll deltas reaching the root scrolling layer go unused.
  // The accumulated overscroll is scoped by the most recent call to
  // InputHandler::ScrollBegin.
  virtual void DidOverscroll(const gfx::Vector2dF& accumulated_overscroll,
                             const gfx::Vector2dF& latest_overscroll_delta) = 0;

 protected:
  InputHandlerClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputHandlerClient);
};

// The InputHandler is a way for the embedders to interact with the impl thread
// side of the compositor implementation. There is one InputHandler per
// LayerTreeHost. To use the input handler, implement the InputHanderClient
// interface and bind it to the handler on the compositor thread.
class CC_EXPORT InputHandler {
 public:
  // Note these are used in a histogram. Do not reorder or delete existing
  // entries.
  enum ScrollStatus {
    ScrollOnMainThread = 0,
    ScrollStarted,
    ScrollIgnored,
    ScrollUnknown,
    // This must be the last entry.
    ScrollStatusCount
  };
  enum ScrollInputType { Gesture, Wheel, NonBubblingGesture };

  // Binds a client to this handler to receive notifications. Only one client
  // can be bound to an InputHandler. The client must live at least until the
  // handler calls WillShutdown() on the client.
  virtual void BindToClient(InputHandlerClient* client) = 0;

  // Selects a layer to be scrolled at a given point in viewport (logical
  // pixel) coordinates. Returns ScrollStarted if the layer at the coordinates
  // can be scrolled, ScrollOnMainThread if the scroll event should instead be
  // delegated to the main thread, or ScrollIgnored if there is nothing to be
  // scrolled at the given coordinates.
  virtual ScrollStatus ScrollBegin(const gfx::Point& viewport_point,
                                   ScrollInputType type) = 0;

  // Scroll the selected layer starting at the given position. If the scroll
  // type given to ScrollBegin was a gesture, then the scroll point and delta
  // should be in viewport (logical pixel) coordinates. Otherwise they are in
  // scrolling layer's (logical pixel) space. If there is no room to move the
  // layer in the requested direction, its first ancestor layer that can be
  // scrolled will be moved instead. If no layer can be moved in the requested
  // direction at all, then false is returned. If any layer is moved, then
  // true is returned.
  // If the scroll delta hits the root layer, and the layer can no longer move,
  // the root overscroll accumulated within this ScrollBegin() scope is reported
  // to the client.
  // Should only be called if ScrollBegin() returned ScrollStarted.
  virtual bool ScrollBy(const gfx::Point& viewport_point,
                        const gfx::Vector2dF& scroll_delta) = 0;

  virtual bool ScrollVerticallyByPage(const gfx::Point& viewport_point,
                                      ScrollDirection direction) = 0;

  // Returns ScrollStarted if a layer was being actively being scrolled,
  // ScrollIgnored if not.
  virtual ScrollStatus FlingScrollBegin() = 0;

  virtual void MouseMoveAt(const gfx::Point& mouse_position) = 0;

  // Stop scrolling the selected layer. Should only be called if ScrollBegin()
  // returned ScrollStarted.
  virtual void ScrollEnd() = 0;

  virtual void SetRootLayerScrollOffsetDelegate(
      LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate) = 0;

  // Called when the value returned by
  // LayerScrollOffsetDelegate.GetTotalScrollOffset has changed for reasons
  // other than a SetTotalScrollOffset call.
  // NOTE: This should only called after a valid delegate was set via a call to
  // SetRootLayerScrollOffsetDelegate.
  virtual void OnRootLayerDelegatedScrollOffsetChanged() = 0;

  virtual void PinchGestureBegin() = 0;
  virtual void PinchGestureUpdate(float magnify_delta,
                                  const gfx::Point& anchor) = 0;
  virtual void PinchGestureEnd() = 0;

  virtual void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                       bool anchor_point,
                                       float page_scale,
                                       base::TimeDelta duration) = 0;

  // Request another callback to InputHandlerClient::Animate().
  virtual void SetNeedsAnimate() = 0;

  // Whether the layer under |viewport_point| is the currently scrolling layer.
  virtual bool IsCurrentlyScrollingLayerAt(const gfx::Point& viewport_point,
                                           ScrollInputType type) = 0;

  virtual bool HaveTouchEventHandlersAt(const gfx::Point& viewport_point) = 0;

  // Calling CreateLatencyInfoSwapPromiseMonitor() to get a scoped
  // LatencyInfoSwapPromiseMonitor. During the life time of the
  // LatencyInfoSwapPromiseMonitor, if SetNeedsRedraw() or SetNeedsRedrawRect()
  // is called on LayerTreeHostImpl, the original latency info will be turned
  // into a LatencyInfoSwapPromise.
  virtual scoped_ptr<SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) = 0;

 protected:
  InputHandler() {}
  virtual ~InputHandler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace cc

#endif  // CC_INPUT_INPUT_HANDLER_H_
