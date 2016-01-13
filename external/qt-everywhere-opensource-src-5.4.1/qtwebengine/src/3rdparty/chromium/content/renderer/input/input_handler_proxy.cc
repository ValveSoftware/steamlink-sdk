// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_handler_proxy.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/renderer/input/input_handler_proxy_client.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/geometry/point_conversions.h"

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPoint;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace {

// Maximum time between a fling event's timestamp and the first |Animate| call
// for the fling curve to use the fling timestamp as the initial animation time.
// Two frames allows a minor delay between event creation and the first animate.
const double kMaxSecondsFromFlingTimestampToFirstAnimate = 2. / 60.;

// Threshold for determining whether a fling scroll delta should have caused the
// client to scroll.
const float kScrollEpsilon = 0.1f;

// Minimum fling velocity required for the active fling and new fling for the
// two to accumulate.
const double kMinBoostFlingSpeedSquare = 350. * 350.;

// Minimum velocity for the active touch scroll to preserve (boost) an active
// fling for which cancellation has been deferred.
const double kMinBoostTouchScrollSpeedSquare = 150 * 150.;

// Timeout window after which the active fling will be cancelled if no scrolls
// or flings of sufficient velocity relative to the current fling are received.
// The default value on Android native views is 40ms, but we use a slightly
// increased value to accomodate small IPC message delays.
const double kFlingBoostTimeoutDelaySeconds = 0.045;

gfx::Vector2dF ToClientScrollIncrement(const WebFloatSize& increment) {
  return gfx::Vector2dF(-increment.width, -increment.height);
}

double InSecondsF(const base::TimeTicks& time) {
  return (time - base::TimeTicks()).InSecondsF();
}

bool ShouldSuppressScrollForFlingBoosting(
    const gfx::Vector2dF& current_fling_velocity,
    const WebGestureEvent& scroll_update_event,
    double time_since_last_boost_event) {
  DCHECK_EQ(WebInputEvent::GestureScrollUpdate, scroll_update_event.type);

  gfx::Vector2dF dx(scroll_update_event.data.scrollUpdate.deltaX,
                    scroll_update_event.data.scrollUpdate.deltaY);
  if (gfx::DotProduct(current_fling_velocity, dx) < 0)
    return false;

  if (time_since_last_boost_event < 0.001)
    return true;

  // TODO(jdduke): Use |scroll_update_event.data.scrollUpdate.velocity{X,Y}|.
  // The scroll must be of sufficient velocity to maintain the active fling.
  const gfx::Vector2dF scroll_velocity =
      gfx::ScaleVector2d(dx, 1. / time_since_last_boost_event);
  if (scroll_velocity.LengthSquared() < kMinBoostTouchScrollSpeedSquare)
    return false;

  return true;
}

bool ShouldBoostFling(const gfx::Vector2dF& current_fling_velocity,
                      const WebGestureEvent& fling_start_event) {
  DCHECK_EQ(WebInputEvent::GestureFlingStart, fling_start_event.type);

  gfx::Vector2dF new_fling_velocity(
      fling_start_event.data.flingStart.velocityX,
      fling_start_event.data.flingStart.velocityY);

  if (gfx::DotProduct(current_fling_velocity, new_fling_velocity) < 0)
    return false;

  if (current_fling_velocity.LengthSquared() < kMinBoostFlingSpeedSquare)
    return false;

  if (new_fling_velocity.LengthSquared() < kMinBoostFlingSpeedSquare)
    return false;

  return true;
}

WebGestureEvent ObtainGestureScrollBegin(const WebGestureEvent& event) {
  WebGestureEvent scroll_begin_event = event;
  scroll_begin_event.type = WebInputEvent::GestureScrollBegin;
  scroll_begin_event.data.scrollBegin.deltaXHint = 0;
  scroll_begin_event.data.scrollBegin.deltaYHint = 0;
  return scroll_begin_event;
}

void SendScrollLatencyUma(const WebInputEvent& event,
                          const ui::LatencyInfo& latency_info) {
  if (!(event.type == WebInputEvent::GestureScrollBegin ||
        event.type == WebInputEvent::GestureScrollUpdate ||
        event.type == WebInputEvent::GestureScrollUpdateWithoutPropagation))
    return;

  ui::LatencyInfo::LatencyMap::const_iterator it =
      latency_info.latency_components.find(std::make_pair(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0));

  if (it == latency_info.latency_components.end())
    return;

  base::TimeDelta delta = base::TimeTicks::HighResNow() - it->second.event_time;
  for (size_t i = 0; i < it->second.event_count; ++i) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.Latency.RendererImpl.GestureScroll2",
        delta.InMicroseconds(),
        1,
        1000000,
        100);
  }
}  // namespace

}

namespace content {

InputHandlerProxy::InputHandlerProxy(cc::InputHandler* input_handler,
                                     InputHandlerProxyClient* client)
    : client_(client),
      input_handler_(input_handler),
      deferred_fling_cancel_time_seconds_(0),
#ifndef NDEBUG
      expect_scroll_update_end_(false),
#endif
      gesture_scroll_on_impl_thread_(false),
      gesture_pinch_on_impl_thread_(false),
      fling_may_be_active_on_main_thread_(false),
      disallow_horizontal_fling_scroll_(false),
      disallow_vertical_fling_scroll_(false),
      has_fling_animation_started_(false) {
  DCHECK(client);
  input_handler_->BindToClient(this);
}

InputHandlerProxy::~InputHandlerProxy() {}

void InputHandlerProxy::WillShutdown() {
  input_handler_ = NULL;
  client_->WillShutdown();
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::HandleInputEventWithLatencyInfo(
    const WebInputEvent& event,
    ui::LatencyInfo* latency_info) {
  DCHECK(input_handler_);

  SendScrollLatencyUma(event, *latency_info);

  TRACE_EVENT_FLOW_STEP0(
      "input",
      "LatencyInfo.Flow",
      TRACE_ID_DONT_MANGLE(latency_info->trace_id),
      "HanldeInputEventImpl");

  scoped_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor =
      input_handler_->CreateLatencyInfoSwapPromiseMonitor(latency_info);
  InputHandlerProxy::EventDisposition disposition = HandleInputEvent(event);
  return disposition;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleInputEvent(
    const WebInputEvent& event) {
  DCHECK(input_handler_);
  TRACE_EVENT1("input", "InputHandlerProxy::HandleInputEvent",
               "type", WebInputEventTraits::GetName(event.type));

  if (FilterInputEventForFlingBoosting(event))
    return DID_HANDLE;

  if (event.type == WebInputEvent::MouseWheel) {
    const WebMouseWheelEvent& wheel_event =
        *static_cast<const WebMouseWheelEvent*>(&event);
    if (wheel_event.scrollByPage) {
      // TODO(jamesr): We don't properly handle scroll by page in the compositor
      // thread, so punt it to the main thread. http://crbug.com/236639
      return DID_NOT_HANDLE;
    }
    if (wheel_event.modifiers & WebInputEvent::ControlKey) {
      // Wheel events involving the control key never trigger scrolling, only
      // event handlers.  Forward to the main thread.
      return DID_NOT_HANDLE;
    }
    cc::InputHandler::ScrollStatus scroll_status = input_handler_->ScrollBegin(
        gfx::Point(wheel_event.x, wheel_event.y), cc::InputHandler::Wheel);
    switch (scroll_status) {
      case cc::InputHandler::ScrollStarted: {
        TRACE_EVENT_INSTANT2(
            "input",
            "InputHandlerProxy::handle_input wheel scroll",
            TRACE_EVENT_SCOPE_THREAD,
            "deltaX",
            -wheel_event.deltaX,
            "deltaY",
            -wheel_event.deltaY);
        bool did_scroll = input_handler_->ScrollBy(
            gfx::Point(wheel_event.x, wheel_event.y),
            gfx::Vector2dF(-wheel_event.deltaX, -wheel_event.deltaY));
        input_handler_->ScrollEnd();
        return did_scroll ? DID_HANDLE : DROP_EVENT;
      }
      case cc::InputHandler::ScrollIgnored:
        // TODO(jamesr): This should be DROP_EVENT, but in cases where we fail
        // to properly sync scrollability it's safer to send the event to the
        // main thread. Change back to DROP_EVENT once we have synchronization
        // bugs sorted out.
        return DID_NOT_HANDLE;
      case cc::InputHandler::ScrollUnknown:
      case cc::InputHandler::ScrollOnMainThread:
        return DID_NOT_HANDLE;
      case cc::InputHandler::ScrollStatusCount:
        NOTREACHED();
        break;
    }
  } else if (event.type == WebInputEvent::GestureScrollBegin) {
    DCHECK(!gesture_scroll_on_impl_thread_);
#ifndef NDEBUG
    DCHECK(!expect_scroll_update_end_);
    expect_scroll_update_end_ = true;
#endif
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    cc::InputHandler::ScrollStatus scroll_status = input_handler_->ScrollBegin(
        gfx::Point(gesture_event.x, gesture_event.y),
        cc::InputHandler::Gesture);
    UMA_HISTOGRAM_ENUMERATION("Renderer4.CompositorScrollHitTestResult",
                              scroll_status,
                              cc::InputHandler::ScrollStatusCount);
    switch (scroll_status) {
      case cc::InputHandler::ScrollStarted:
        TRACE_EVENT_INSTANT0("input",
                             "InputHandlerProxy::handle_input gesture scroll",
                             TRACE_EVENT_SCOPE_THREAD);
        gesture_scroll_on_impl_thread_ = true;
        return DID_HANDLE;
      case cc::InputHandler::ScrollUnknown:
      case cc::InputHandler::ScrollOnMainThread:
        return DID_NOT_HANDLE;
      case cc::InputHandler::ScrollIgnored:
        return DROP_EVENT;
      case cc::InputHandler::ScrollStatusCount:
        NOTREACHED();
        break;
    }
  } else if (event.type == WebInputEvent::GestureScrollUpdate) {
#ifndef NDEBUG
    DCHECK(expect_scroll_update_end_);
#endif

    if (!gesture_scroll_on_impl_thread_ && !gesture_pinch_on_impl_thread_)
      return DID_NOT_HANDLE;

    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    bool did_scroll = input_handler_->ScrollBy(
        gfx::Point(gesture_event.x, gesture_event.y),
        gfx::Vector2dF(-gesture_event.data.scrollUpdate.deltaX,
                       -gesture_event.data.scrollUpdate.deltaY));
    return did_scroll ? DID_HANDLE : DROP_EVENT;
  } else if (event.type == WebInputEvent::GestureScrollEnd) {
#ifndef NDEBUG
    DCHECK(expect_scroll_update_end_);
    expect_scroll_update_end_ = false;
#endif
    input_handler_->ScrollEnd();

    if (!gesture_scroll_on_impl_thread_)
      return DID_NOT_HANDLE;

    gesture_scroll_on_impl_thread_ = false;
    return DID_HANDLE;
  } else if (event.type == WebInputEvent::GesturePinchBegin) {
    input_handler_->PinchGestureBegin();
    DCHECK(!gesture_pinch_on_impl_thread_);
    gesture_pinch_on_impl_thread_ = true;
    return DID_HANDLE;
  } else if (event.type == WebInputEvent::GesturePinchEnd) {
    DCHECK(gesture_pinch_on_impl_thread_);
    gesture_pinch_on_impl_thread_ = false;
    input_handler_->PinchGestureEnd();
    return DID_HANDLE;
  } else if (event.type == WebInputEvent::GesturePinchUpdate) {
    DCHECK(gesture_pinch_on_impl_thread_);
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    input_handler_->PinchGestureUpdate(
        gesture_event.data.pinchUpdate.scale,
        gfx::Point(gesture_event.x, gesture_event.y));
    return DID_HANDLE;
  } else if (event.type == WebInputEvent::GestureFlingStart) {
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    return HandleGestureFling(gesture_event);
  } else if (event.type == WebInputEvent::GestureFlingCancel) {
    if (CancelCurrentFling(true))
      return DID_HANDLE;
    else if (!fling_may_be_active_on_main_thread_)
      return DROP_EVENT;
  } else if (event.type == WebInputEvent::TouchStart) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(&event);
    for (size_t i = 0; i < touch_event.touchesLength; ++i) {
      if (touch_event.touches[i].state != WebTouchPoint::StatePressed)
        continue;
      if (input_handler_->HaveTouchEventHandlersAt(
              gfx::Point(touch_event.touches[i].position.x,
                         touch_event.touches[i].position.y))) {
        return DID_NOT_HANDLE;
      }
    }
    return DROP_EVENT;
  } else if (WebInputEvent::isKeyboardEventType(event.type)) {
    // Only call |CancelCurrentFling()| if a fling was active, as it will
    // otherwise disrupt an in-progress touch scroll.
    if (fling_curve_)
      CancelCurrentFling(true);
  } else if (event.type == WebInputEvent::MouseMove) {
    const WebMouseEvent& mouse_event =
        *static_cast<const WebMouseEvent*>(&event);
    // TODO(tony): Ignore when mouse buttons are down?
    // TODO(davemoore): This should never happen, but bug #326635 showed some
    // surprising crashes.
    CHECK(input_handler_);
    input_handler_->MouseMoveAt(gfx::Point(mouse_event.x, mouse_event.y));
  }

  return DID_NOT_HANDLE;
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::HandleGestureFling(
    const WebGestureEvent& gesture_event) {
  cc::InputHandler::ScrollStatus scroll_status;

  if (gesture_event.sourceDevice == blink::WebGestureDeviceTouchpad) {
    scroll_status = input_handler_->ScrollBegin(
        gfx::Point(gesture_event.x, gesture_event.y),
        cc::InputHandler::NonBubblingGesture);
  } else {
    if (!gesture_scroll_on_impl_thread_)
      scroll_status = cc::InputHandler::ScrollOnMainThread;
    else
      scroll_status = input_handler_->FlingScrollBegin();
  }

#ifndef NDEBUG
  expect_scroll_update_end_ = false;
#endif

  switch (scroll_status) {
    case cc::InputHandler::ScrollStarted: {
      if (gesture_event.sourceDevice == blink::WebGestureDeviceTouchpad)
        input_handler_->ScrollEnd();

      const float vx = gesture_event.data.flingStart.velocityX;
      const float vy = gesture_event.data.flingStart.velocityY;
      current_fling_velocity_ = gfx::Vector2dF(vx, vy);
      fling_curve_.reset(client_->CreateFlingAnimationCurve(
          gesture_event.sourceDevice,
          WebFloatPoint(vx, vy),
          blink::WebSize()));
      disallow_horizontal_fling_scroll_ = !vx;
      disallow_vertical_fling_scroll_ = !vy;
      TRACE_EVENT_ASYNC_BEGIN2("input",
                               "InputHandlerProxy::HandleGestureFling::started",
                               this,
                               "vx",
                               vx,
                               "vy",
                               vy);
      // Note that the timestamp will only be used to kickstart the animation if
      // its sufficiently close to the timestamp of the first call |Animate()|.
      has_fling_animation_started_ = false;
      fling_parameters_.startTime = gesture_event.timeStampSeconds;
      fling_parameters_.delta = WebFloatPoint(vx, vy);
      fling_parameters_.point = WebPoint(gesture_event.x, gesture_event.y);
      fling_parameters_.globalPoint =
          WebPoint(gesture_event.globalX, gesture_event.globalY);
      fling_parameters_.modifiers = gesture_event.modifiers;
      fling_parameters_.sourceDevice = gesture_event.sourceDevice;
      input_handler_->SetNeedsAnimate();
      return DID_HANDLE;
    }
    case cc::InputHandler::ScrollUnknown:
    case cc::InputHandler::ScrollOnMainThread: {
      TRACE_EVENT_INSTANT0("input",
                           "InputHandlerProxy::HandleGestureFling::"
                           "scroll_on_main_thread",
                           TRACE_EVENT_SCOPE_THREAD);
      fling_may_be_active_on_main_thread_ = true;
      return DID_NOT_HANDLE;
    }
    case cc::InputHandler::ScrollIgnored: {
      TRACE_EVENT_INSTANT0(
          "input",
          "InputHandlerProxy::HandleGestureFling::ignored",
          TRACE_EVENT_SCOPE_THREAD);
      if (gesture_event.sourceDevice == blink::WebGestureDeviceTouchpad) {
        // We still pass the curve to the main thread if there's nothing
        // scrollable, in case something
        // registers a handler before the curve is over.
        return DID_NOT_HANDLE;
      }
      return DROP_EVENT;
    }
    case cc::InputHandler::ScrollStatusCount:
      NOTREACHED();
      break;
  }
  return DID_NOT_HANDLE;
}

bool InputHandlerProxy::FilterInputEventForFlingBoosting(
    const WebInputEvent& event) {
  if (!WebInputEvent::isGestureEventType(event.type))
    return false;

  if (!fling_curve_) {
    DCHECK(!deferred_fling_cancel_time_seconds_);
    return false;
  }

  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(event);
  if (gesture_event.type == WebInputEvent::GestureFlingCancel) {
    if (current_fling_velocity_.LengthSquared() < kMinBoostFlingSpeedSquare)
      return false;

    TRACE_EVENT_INSTANT0("input",
                         "InputHandlerProxy::FlingBoostStart",
                         TRACE_EVENT_SCOPE_THREAD);
    deferred_fling_cancel_time_seconds_ =
        event.timeStampSeconds + kFlingBoostTimeoutDelaySeconds;
    return true;
  }

  // A fling is either inactive or is "free spinning", i.e., has yet to be
  // interrupted by a touch gesture, in which case there is nothing to filter.
  if (!deferred_fling_cancel_time_seconds_)
    return false;

  // Gestures from a different source should immediately interrupt the fling.
  if (gesture_event.sourceDevice != fling_parameters_.sourceDevice) {
    FlingBoostCancelAndResumeScrollingIfNecessary();
    return false;
  }

  switch (gesture_event.type) {
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTapDown:
      return false;

    case WebInputEvent::GestureScrollBegin:
      if (!input_handler_->IsCurrentlyScrollingLayerAt(
              gfx::Point(gesture_event.x, gesture_event.y),
              fling_parameters_.sourceDevice == blink::WebGestureDeviceTouchpad
                  ? cc::InputHandler::NonBubblingGesture
                  : cc::InputHandler::Gesture)) {
        CancelCurrentFling(true);
        return false;
      }

      // TODO(jdduke): Use |gesture_event.data.scrollBegin.delta{X,Y}Hint| to
      // determine if the ScrollBegin should immediately cancel the fling.
      FlingBoostExtend(gesture_event);
      return true;

    case WebInputEvent::GestureScrollUpdate: {
      const double time_since_last_boost_event =
          event.timeStampSeconds - last_fling_boost_event_.timeStampSeconds;
      if (ShouldSuppressScrollForFlingBoosting(current_fling_velocity_,
                                               gesture_event,
                                               time_since_last_boost_event)) {
        FlingBoostExtend(gesture_event);
        return true;
      }

      FlingBoostCancelAndResumeScrollingIfNecessary();
      return false;
    }

    case WebInputEvent::GestureScrollEnd:
      CancelCurrentFling(true);
      return true;

    case WebInputEvent::GestureFlingStart: {
      DCHECK_EQ(fling_parameters_.sourceDevice, gesture_event.sourceDevice);

      bool fling_boosted =
          fling_parameters_.modifiers == gesture_event.modifiers &&
          ShouldBoostFling(current_fling_velocity_, gesture_event);

      gfx::Vector2dF new_fling_velocity(
          gesture_event.data.flingStart.velocityX,
          gesture_event.data.flingStart.velocityY);

      if (fling_boosted)
        current_fling_velocity_ += new_fling_velocity;
      else
        current_fling_velocity_ = new_fling_velocity;

      WebFloatPoint velocity(current_fling_velocity_.x(),
                             current_fling_velocity_.y());
      deferred_fling_cancel_time_seconds_ = 0;
      last_fling_boost_event_ = WebGestureEvent();
      fling_curve_.reset(client_->CreateFlingAnimationCurve(
          gesture_event.sourceDevice,
          velocity,
          blink::WebSize()));
      fling_parameters_.startTime = gesture_event.timeStampSeconds;
      fling_parameters_.delta = velocity;
      fling_parameters_.point = WebPoint(gesture_event.x, gesture_event.y);
      fling_parameters_.globalPoint =
          WebPoint(gesture_event.globalX, gesture_event.globalY);

      TRACE_EVENT_INSTANT2("input",
                           fling_boosted ? "InputHandlerProxy::FlingBoosted"
                                         : "InputHandlerProxy::FlingReplaced",
                           TRACE_EVENT_SCOPE_THREAD,
                           "vx",
                           current_fling_velocity_.x(),
                           "vy",
                           current_fling_velocity_.y());

      // The client expects balanced calls between a consumed GestureFlingStart
      // and |DidStopFlinging()|. TODO(jdduke): Provide a count parameter to
      // |DidStopFlinging()| and only send after the accumulated fling ends.
      client_->DidStopFlinging();
      return true;
    }

    default:
      // All other types of gestures (taps, presses, etc...) will complete the
      // deferred fling cancellation.
      FlingBoostCancelAndResumeScrollingIfNecessary();
      return false;
  }
}

void InputHandlerProxy::FlingBoostExtend(const blink::WebGestureEvent& event) {
  TRACE_EVENT_INSTANT0(
      "input", "InputHandlerProxy::FlingBoostExtend", TRACE_EVENT_SCOPE_THREAD);
  deferred_fling_cancel_time_seconds_ =
      event.timeStampSeconds + kFlingBoostTimeoutDelaySeconds;
  last_fling_boost_event_ = event;
}

void InputHandlerProxy::FlingBoostCancelAndResumeScrollingIfNecessary() {
  TRACE_EVENT_INSTANT0(
      "input", "InputHandlerProxy::FlingBoostCancel", TRACE_EVENT_SCOPE_THREAD);
  DCHECK(deferred_fling_cancel_time_seconds_);

  // Note: |last_fling_boost_event_| is cleared by |CancelCurrentFling()|.
  WebGestureEvent last_fling_boost_event = last_fling_boost_event_;

  CancelCurrentFling(true);

  if (last_fling_boost_event.type == WebInputEvent::GestureScrollBegin ||
      last_fling_boost_event.type == WebInputEvent::GestureScrollUpdate) {
    // Synthesize a GestureScrollBegin, as the original was suppressed.
    HandleInputEvent(ObtainGestureScrollBegin(last_fling_boost_event));
  }
}

void InputHandlerProxy::Animate(base::TimeTicks time) {
  if (!fling_curve_)
    return;

  double monotonic_time_sec = InSecondsF(time);

  if (deferred_fling_cancel_time_seconds_ &&
      monotonic_time_sec > deferred_fling_cancel_time_seconds_) {
    FlingBoostCancelAndResumeScrollingIfNecessary();
    return;
  }

  if (!has_fling_animation_started_) {
    has_fling_animation_started_ = true;
    // Guard against invalid, future or sufficiently stale start times, as there
    // are no guarantees fling event and animation timestamps are compatible.
    if (!fling_parameters_.startTime ||
        monotonic_time_sec <= fling_parameters_.startTime ||
        monotonic_time_sec >= fling_parameters_.startTime +
                                  kMaxSecondsFromFlingTimestampToFirstAnimate) {
      fling_parameters_.startTime = monotonic_time_sec;
      input_handler_->SetNeedsAnimate();
      return;
    }
  }

  bool fling_is_active =
      fling_curve_->apply(monotonic_time_sec - fling_parameters_.startTime,
                          this);

  if (disallow_vertical_fling_scroll_ && disallow_horizontal_fling_scroll_)
    fling_is_active = false;

  if (fling_is_active) {
    input_handler_->SetNeedsAnimate();
  } else {
    TRACE_EVENT_INSTANT0("input",
                         "InputHandlerProxy::animate::flingOver",
                         TRACE_EVENT_SCOPE_THREAD);
    CancelCurrentFling(true);
  }
}

void InputHandlerProxy::MainThreadHasStoppedFlinging() {
  fling_may_be_active_on_main_thread_ = false;
  client_->DidStopFlinging();
}

void InputHandlerProxy::DidOverscroll(
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::Vector2dF& latest_overscroll_delta) {
  DCHECK(client_);

  TRACE_EVENT2("input",
               "InputHandlerProxy::DidOverscroll",
               "dx",
               latest_overscroll_delta.x(),
               "dy",
               latest_overscroll_delta.y());

  DidOverscrollParams params;
  params.accumulated_overscroll = accumulated_overscroll;
  params.latest_overscroll_delta = latest_overscroll_delta;
  params.current_fling_velocity =
      ToClientScrollIncrement(current_fling_velocity_);

  if (fling_curve_) {
    static const int kFlingOverscrollThreshold = 1;
    disallow_horizontal_fling_scroll_ |=
        std::abs(params.accumulated_overscroll.x()) >=
        kFlingOverscrollThreshold;
    disallow_vertical_fling_scroll_ |=
        std::abs(params.accumulated_overscroll.y()) >=
        kFlingOverscrollThreshold;
  }

  client_->DidOverscroll(params);
}

bool InputHandlerProxy::CancelCurrentFling(
    bool send_fling_stopped_notification) {
  bool had_fling_animation = fling_curve_;
  if (had_fling_animation &&
      fling_parameters_.sourceDevice == blink::WebGestureDeviceTouchscreen) {
    input_handler_->ScrollEnd();
    TRACE_EVENT_ASYNC_END0(
        "input",
        "InputHandlerProxy::HandleGestureFling::started",
        this);
  }

  TRACE_EVENT_INSTANT1("input",
                       "InputHandlerProxy::CancelCurrentFling",
                       TRACE_EVENT_SCOPE_THREAD,
                       "had_fling_animation",
                       had_fling_animation);
  fling_curve_.reset();
  has_fling_animation_started_ = false;
  gesture_scroll_on_impl_thread_ = false;
  current_fling_velocity_ = gfx::Vector2dF();
  fling_parameters_ = blink::WebActiveWheelFlingParameters();
  deferred_fling_cancel_time_seconds_ = 0;
  last_fling_boost_event_ = WebGestureEvent();
  if (send_fling_stopped_notification && had_fling_animation)
    client_->DidStopFlinging();
  return had_fling_animation;
}

bool InputHandlerProxy::TouchpadFlingScroll(
    const WebFloatSize& increment) {
  WebMouseWheelEvent synthetic_wheel;
  synthetic_wheel.type = WebInputEvent::MouseWheel;
  synthetic_wheel.deltaX = increment.width;
  synthetic_wheel.deltaY = increment.height;
  synthetic_wheel.hasPreciseScrollingDeltas = true;
  synthetic_wheel.x = fling_parameters_.point.x;
  synthetic_wheel.y = fling_parameters_.point.y;
  synthetic_wheel.globalX = fling_parameters_.globalPoint.x;
  synthetic_wheel.globalY = fling_parameters_.globalPoint.y;
  synthetic_wheel.modifiers = fling_parameters_.modifiers;

  InputHandlerProxy::EventDisposition disposition =
      HandleInputEvent(synthetic_wheel);
  switch (disposition) {
    case DID_HANDLE:
      return true;
    case DROP_EVENT:
      break;
    case DID_NOT_HANDLE:
      TRACE_EVENT_INSTANT0("input",
                           "InputHandlerProxy::scrollBy::AbortFling",
                           TRACE_EVENT_SCOPE_THREAD);
      // If we got a DID_NOT_HANDLE, that means we need to deliver wheels on the
      // main thread. In this case we need to schedule a commit and transfer the
      // fling curve over to the main thread and run the rest of the wheels from
      // there. This can happen when flinging a page that contains a scrollable
      // subarea that we can't scroll on the thread if the fling starts outside
      // the subarea but then is flung "under" the pointer.
      client_->TransferActiveWheelFlingAnimation(fling_parameters_);
      fling_may_be_active_on_main_thread_ = true;
      CancelCurrentFling(false);
      break;
  }

  return false;
}

bool InputHandlerProxy::scrollBy(const WebFloatSize& increment,
                                 const WebFloatSize& velocity) {
  WebFloatSize clipped_increment;
  WebFloatSize clipped_velocity;
  if (!disallow_horizontal_fling_scroll_) {
    clipped_increment.width = increment.width;
    clipped_velocity.width = velocity.width;
  }
  if (!disallow_vertical_fling_scroll_) {
    clipped_increment.height = increment.height;
    clipped_velocity.height = velocity.height;
  }

  current_fling_velocity_ = clipped_velocity;

  // Early out if the increment is zero, but avoid early terimination if the
  // velocity is still non-zero.
  if (clipped_increment == WebFloatSize())
    return clipped_velocity != WebFloatSize();

  TRACE_EVENT2("input",
               "InputHandlerProxy::scrollBy",
               "x",
               clipped_increment.width,
               "y",
               clipped_increment.height);

  bool did_scroll = false;

  switch (fling_parameters_.sourceDevice) {
    case blink::WebGestureDeviceTouchpad:
      did_scroll = TouchpadFlingScroll(clipped_increment);
      break;
    case blink::WebGestureDeviceTouchscreen:
      clipped_increment = ToClientScrollIncrement(clipped_increment);
      did_scroll = input_handler_->ScrollBy(fling_parameters_.point,
                                            clipped_increment);
      break;
  }

  if (did_scroll) {
    fling_parameters_.cumulativeScroll.width += clipped_increment.width;
    fling_parameters_.cumulativeScroll.height += clipped_increment.height;
  }

  // It's possible the provided |increment| is sufficiently small as to not
  // trigger a scroll, e.g., with a trivial time delta between fling updates.
  // Return true in this case to prevent early fling termination.
  if (std::abs(clipped_increment.width) < kScrollEpsilon &&
      std::abs(clipped_increment.height) < kScrollEpsilon)
    return true;

  return did_scroll;
}

}  // namespace content
