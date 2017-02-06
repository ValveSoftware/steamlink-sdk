// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace ui {
class LatencyInfo;
}

namespace content {

class RenderWidgetHostViewAuraOverscrollTest;
class OverscrollControllerDelegate;

// Indicates the direction that the scroll is heading in relative to the screen,
// with the top being NORTH.
enum OverscrollMode {
  OVERSCROLL_NONE,
  OVERSCROLL_NORTH,
  OVERSCROLL_SOUTH,
  OVERSCROLL_WEST,
  OVERSCROLL_EAST
};

// When a page is scrolled beyond the scrollable region, it will trigger an
// overscroll gesture. This controller receives the events that are dispatched
// to the renderer, and the ACKs of events, and updates the overscroll gesture
// status accordingly.
class OverscrollController {
 public:
  OverscrollController();
  virtual ~OverscrollController();

  // This must be called when dispatching any event from the
  // RenderWidgetHostView so that the state of the overscroll gesture can be
  // updated properly. Returns true if the event was handled, in which case
  // further processing should cease.
  bool WillHandleEvent(const blink::WebInputEvent& event);

  // This must be called when the ACK for any event comes in. This updates the
  // overscroll gesture status as appropriate.
  void ReceivedEventACK(const blink::WebInputEvent& event, bool processed);

  // This must be called when a gesture event is filtered out and not sent to
  // the renderer.
  void DiscardingGestureEvent(const blink::WebGestureEvent& event);

  OverscrollMode overscroll_mode() const { return overscroll_mode_; }

  void set_delegate(OverscrollControllerDelegate* delegate) {
    delegate_ = delegate;
  }

  // Resets internal states.
  void Reset();

  // Cancels any in-progress overscroll (and calls OnOverscrollModeChange on the
  // delegate if necessary), and resets internal states.
  void Cancel();

 private:
  friend class RenderWidgetHostViewAuraOverscrollTest;

  // Different scrolling states.
  enum ScrollState {
    STATE_UNKNOWN,
    STATE_PENDING,
    STATE_CONTENT_SCROLLING,
    STATE_OVERSCROLLING,
  };

  // Returns true if the event indicates that the in-progress overscroll gesture
  // can now be completed.
  bool DispatchEventCompletesAction(
      const blink::WebInputEvent& event) const;

  // Returns true to indicate that dispatching the event should reset the
  // overscroll gesture status.
  bool DispatchEventResetsState(const blink::WebInputEvent& event) const;

  // Processes an event to update the internal state for overscroll. Returns
  // true if the state is updated, false otherwise.
  bool ProcessEventForOverscroll(const blink::WebInputEvent& event);

  // Processes horizontal overscroll. This can update both the overscroll mode
  // and the over scroll amount (i.e. |overscroll_mode_|, |overscroll_delta_x_|
  // and |overscroll_delta_y_|). Returns true if overscroll was handled by the
  // delegate.
  bool ProcessOverscroll(float delta_x, float delta_y, bool is_touchpad);

  // Completes the desired action from the current gesture.
  void CompleteAction();

  // Sets the overscroll mode (and triggers callback in the delegate when
  // appropriate).
  void SetOverscrollMode(OverscrollMode new_mode);

  // Whether this event should be processed or not handled by the controller.
  bool ShouldProcessEvent(const blink::WebInputEvent& event);

  // The current state of overscroll gesture.
  OverscrollMode overscroll_mode_;

  // Used to keep track of the scrolling state.
  // If scrolling starts, and some scroll events are consumed at the beginning
  // of the scroll (i.e. some content on the web-page was scrolled), then do not
  // process any of the subsequent scroll events for generating overscroll
  // gestures.
  ScrollState scroll_state_;

  // The amount of overscroll in progress. These values are invalid when
  // |overscroll_mode_| is set to OVERSCROLL_NONE.
  float overscroll_delta_x_;
  float overscroll_delta_y_;

  // The delegate that receives the overscroll updates. The delegate is not
  // owned by this controller.
  OverscrollControllerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_H_
