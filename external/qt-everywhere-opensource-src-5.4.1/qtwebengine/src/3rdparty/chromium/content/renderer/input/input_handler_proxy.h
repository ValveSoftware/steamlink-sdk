// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_HANDLER_PROXY_H_
#define CONTENT_RENDERER_INPUT_INPUT_HANDLER_PROXY_H_

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/input/input_handler.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebGestureCurveTarget.h"
#include "third_party/WebKit/public/web/WebActiveWheelFlingParameters.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

class InputHandlerProxyClient;

// This class is a proxy between the content input event filtering and the
// compositor's input handling logic. InputHandlerProxy instances live entirely
// on the compositor thread. Each InputHandler instance handles input events
// intended for a specific WebWidget.
class CONTENT_EXPORT InputHandlerProxy
    : public cc::InputHandlerClient,
      public NON_EXPORTED_BASE(blink::WebGestureCurveTarget) {
 public:
  InputHandlerProxy(cc::InputHandler* input_handler,
                    InputHandlerProxyClient* client);
  virtual ~InputHandlerProxy();

  enum EventDisposition {
    DID_HANDLE,
    DID_NOT_HANDLE,
    DROP_EVENT
  };
  EventDisposition HandleInputEventWithLatencyInfo(
      const blink::WebInputEvent& event,
      ui::LatencyInfo* latency_info);
  EventDisposition HandleInputEvent(const blink::WebInputEvent& event);

  // cc::InputHandlerClient implementation.
  virtual void WillShutdown() OVERRIDE;
  virtual void Animate(base::TimeTicks time) OVERRIDE;
  virtual void MainThreadHasStoppedFlinging() OVERRIDE;
  virtual void DidOverscroll(const gfx::Vector2dF& accumulated_overscroll,
                             const gfx::Vector2dF& latest_overscroll_delta)
      OVERRIDE;

  // blink::WebGestureCurveTarget implementation.
  virtual bool scrollBy(const blink::WebFloatSize& offset,
                        const blink::WebFloatSize& velocity);

  bool gesture_scroll_on_impl_thread_for_testing() const {
    return gesture_scroll_on_impl_thread_;
  }

 private:
  EventDisposition HandleGestureFling(const blink::WebGestureEvent& event);

  // Returns true if the event should be suppressed due to to an active,
  // boost-enabled fling, in which case further processing should cease.
  bool FilterInputEventForFlingBoosting(const blink::WebInputEvent& event);

  // Schedule a time in the future after which a boost-enabled fling will
  // terminate without further momentum from the user (see |Animate()|).
  void FlingBoostExtend(const blink::WebGestureEvent& event);

  // Cancel the current fling and insert a GestureScrollBegin if necessary.
  void FlingBoostCancelAndResumeScrollingIfNecessary();

  // Returns true if we scrolled by the increment.
  bool TouchpadFlingScroll(const blink::WebFloatSize& increment);

  // Returns true if we actually had an active fling to cancel.
  bool CancelCurrentFling(bool send_fling_stopped_notification);

  scoped_ptr<blink::WebGestureCurve> fling_curve_;
  // Parameters for the active fling animation, stored in case we need to
  // transfer it out later.
  blink::WebActiveWheelFlingParameters fling_parameters_;

  InputHandlerProxyClient* client_;
  cc::InputHandler* input_handler_;

  // Time at which an active fling should expire due to a deferred cancellation
  // event. A call to |Animate()| after this time will end the fling.
  double deferred_fling_cancel_time_seconds_;

  // The last event that extended the lifetime of the boosted fling. If the
  // event was a scroll gesture, a GestureScrollBegin will be inserted if the
  // fling terminates (via |FlingBoostCancelAndResumeScrollingIfNecessary()|).
  blink::WebGestureEvent last_fling_boost_event_;

#ifndef NDEBUG
  bool expect_scroll_update_end_;
#endif
  bool gesture_scroll_on_impl_thread_;
  bool gesture_pinch_on_impl_thread_;
  // This is always false when there are no flings on the main thread, but
  // conservative in the sense that we might not be actually flinging when it is
  // true.
  bool fling_may_be_active_on_main_thread_;
  // The axes on which the current fling is allowed to scroll.  If a given fling
  // has overscrolled on a particular axis, further fling scrolls on that axis
  // will be disabled.
  bool disallow_horizontal_fling_scroll_;
  bool disallow_vertical_fling_scroll_;

  // Whether an active fling has seen an |Animate()| call. This is useful for
  // determining if the fling start time should be re-initialized.
  bool has_fling_animation_started_;

  // Non-zero only within the scope of |scrollBy|.
  gfx::Vector2dF current_fling_velocity_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_HANDLER_PROXY_H_
