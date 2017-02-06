// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/render_widget_input_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "build/build_config.h"
#include "cc/trees/swap_promise_monitor.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/common/input/input_event_ack.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/point_conversions.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#endif

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

// TODO(brianderson): Replace the hard-coded threshold with a fraction of
// the BeginMainFrame interval.
// 4166us will allow 1/4 of a 60Hz interval or 1/2 of a 120Hz interval to
// be spent in input hanlders before input starts getting throttled.
const int kInputHandlingTimeThrottlingThresholdMicroseconds = 4166;

int64_t GetEventLatencyMicros(double event_timestamp, base::TimeTicks now) {
  return (now - base::TimeDelta::FromSecondsD(event_timestamp))
      .ToInternalValue();
}

void LogInputEventLatencyUma(const WebInputEvent& event, base::TimeTicks now) {
  WebInputEvent::Type event_type = event.type;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Event.AggregatedLatency.Renderer2",
      GetEventLatencyMicros(event.timeStampSeconds, now), 1, 10000000, 100);

#define CASE_TYPE(t)                                                           \
  case WebInputEvent::t:                                                       \
    UMA_HISTOGRAM_CUSTOM_COUNTS(                                               \
        "Event.Latency.Renderer2." #t,                                         \
        GetEventLatencyMicros(event.timeStampSeconds, now), 1, 10000000, 100); \
    break;

  switch (event_type) {
    CASE_TYPE(Undefined);
    CASE_TYPE(MouseDown);
    CASE_TYPE(MouseUp);
    CASE_TYPE(MouseMove);
    CASE_TYPE(MouseEnter);
    CASE_TYPE(MouseLeave);
    CASE_TYPE(ContextMenu);
    CASE_TYPE(MouseWheel);
    CASE_TYPE(RawKeyDown);
    CASE_TYPE(KeyDown);
    CASE_TYPE(KeyUp);
    CASE_TYPE(Char);
    CASE_TYPE(GestureScrollBegin);
    CASE_TYPE(GestureScrollEnd);
    CASE_TYPE(GestureScrollUpdate);
    CASE_TYPE(GestureFlingStart);
    CASE_TYPE(GestureFlingCancel);
    CASE_TYPE(GestureShowPress);
    CASE_TYPE(GestureTap);
    CASE_TYPE(GestureTapUnconfirmed);
    CASE_TYPE(GestureTapDown);
    CASE_TYPE(GestureTapCancel);
    CASE_TYPE(GestureDoubleTap);
    CASE_TYPE(GestureTwoFingerTap);
    CASE_TYPE(GestureLongPress);
    CASE_TYPE(GestureLongTap);
    CASE_TYPE(GesturePinchBegin);
    CASE_TYPE(GesturePinchEnd);
    CASE_TYPE(GesturePinchUpdate);
    CASE_TYPE(TouchStart);
    CASE_TYPE(TouchMove);
    CASE_TYPE(TouchEnd);
    CASE_TYPE(TouchCancel);
    CASE_TYPE(TouchScrollStarted);
    default:
      // Must include default to let blink::WebInputEvent add new event types
      // before they're added here.
      DLOG(WARNING) << "Unhandled WebInputEvent type: " << event_type;
      break;
  }

#undef CASE_TYPE
}

void LogPassiveEventListenersUma(WebInputEventResult result,
                                 WebInputEvent::DispatchType dispatch_type,
                                 double event_timestamp,
                                 const ui::LatencyInfo& latency_info) {
  enum {
    PASSIVE_LISTENER_UMA_ENUM_PASSIVE,
    PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
    PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED,
    PASSIVE_LISTENER_UMA_ENUM_CANCELABLE,
    PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED,
    PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING,
    PASSIVE_LISTENER_UMA_ENUM_COUNT
  };

  int enum_value;
  switch (dispatch_type) {
    case WebInputEvent::ListenersForcedNonBlockingPassive:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING;
      break;
    case WebInputEvent::ListenersNonBlockingPassive:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_PASSIVE;
      break;
    case WebInputEvent::EventNonBlocking:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE;
      break;
    case WebInputEvent::Blocking:
      if (result == WebInputEventResult::HandledApplication)
        enum_value = PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED;
      else if (result == WebInputEventResult::HandledSuppressed)
        enum_value = PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED;
      else
        enum_value = PASSIVE_LISTENER_UMA_ENUM_CANCELABLE;
      break;
    default:
      NOTREACHED();
      return;
  }

  UMA_HISTOGRAM_ENUMERATION("Event.PassiveListeners", enum_value,
                            PASSIVE_LISTENER_UMA_ENUM_COUNT);

  if (base::TimeTicks::IsHighResolution()) {
    if (enum_value == PASSIVE_LISTENER_UMA_ENUM_CANCELABLE) {
      base::TimeTicks now = base::TimeTicks::Now();
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.PassiveListeners.Latency",
                                  GetEventLatencyMicros(event_timestamp, now),
                                  1, 10000000, 100);
    } else if (enum_value == PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING) {
      base::TimeTicks now = base::TimeTicks::Now();
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.PassiveListeners.ForcedNonBlockingLatency",
          GetEventLatencyMicros(event_timestamp, now), 1, 10000000, 100);
    }
  }
}

}  // namespace

RenderWidgetInputHandler::RenderWidgetInputHandler(
    RenderWidgetInputHandlerDelegate* delegate,
    RenderWidget* widget)
    : delegate_(delegate),
      widget_(widget),
      handling_input_event_(false),
      handling_event_overscroll_(nullptr),
      handling_event_type_(WebInputEvent::Undefined),
      context_menu_source_type_(ui::MENU_SOURCE_MOUSE),
      suppress_next_char_events_(false),
      ignore_ack_for_mouse_move_from_debugger_(false) {
  DCHECK(delegate);
  DCHECK(widget);
  delegate->SetInputHandler(this);
}

RenderWidgetInputHandler::~RenderWidgetInputHandler() {}

void RenderWidgetInputHandler::HandleInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  base::AutoReset<bool> handling_input_event_resetter(&handling_input_event_,
                                                      true);
  base::AutoReset<WebInputEvent::Type> handling_event_type_resetter(
      &handling_event_type_, input_event.type);

  // Calls into |didOverscroll()| while handling this event will populate
  // |event_overscroll|, which in turn will be bundled with the event ack.
  std::unique_ptr<DidOverscrollParams> event_overscroll;
  base::AutoReset<std::unique_ptr<DidOverscrollParams>*>
      handling_event_overscroll_resetter(&handling_event_overscroll_,
                                         &event_overscroll);

#if defined(OS_ANDROID)
  bool from_ime = false;

  // For most keyboard events, we want the change source to be FROM_IME because
  // we don't need to update IME states in ReplicaInputConnection.
  if (!widget_->IsUsingImeThread() &&
      WebInputEvent::isKeyboardEventType(input_event.type)) {
    const WebKeyboardEvent& key_event =
        *static_cast<const WebKeyboardEvent*>(&input_event);
    // TODO(changwan): this if-condition is a stop-gap solution to update IME
    // states in ReplicaInputConnection when using DPAD navigation. This is not
    // a correct solution because InputConnection#getTextBeforeCursor()
    // immediately after InputConnection#sendKeyEvent() will not return the
    // correct value. The correct solution is either redesign the architecture
    // or emulate the DPAD behavior in ReplicaInputConnection, either is
    // non-trivial.
    if (key_event.nativeKeyCode != AKEYCODE_TAB &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_CENTER &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_LEFT &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_RIGHT &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_UP &&
        key_event.nativeKeyCode != AKEYCODE_DPAD_DOWN) {
      from_ime = true;
    }
  }

  ImeEventGuard guard(widget_);
  guard.set_from_ime(from_ime);
#endif

  base::TimeTicks start_time;
  if (base::TimeTicks::IsHighResolution())
    start_time = base::TimeTicks::Now();

  TRACE_EVENT1("renderer,benchmark",
               "RenderWidgetInputHandler::OnHandleInputEvent", "event",
               WebInputEventTraits::GetName(input_event.type));
  TRACE_EVENT_SYNTHETIC_DELAY_BEGIN("blink.HandleInputEvent");
  TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "HandleInputEventMain");

  // If we don't have a high res timer, these metrics won't be accurate enough
  // to be worth collecting. Note that this does introduce some sampling bias.
  if (!start_time.is_null())
    LogInputEventLatencyUma(input_event, start_time);

  std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  ui::LatencyInfo swap_latency_info(latency_info);
  swap_latency_info.AddLatencyNumber(
      ui::LatencyComponentType::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0,
      0);
  if (widget_->compositor()) {
    latency_info_swap_promise_monitor =
        widget_->compositor()->CreateLatencyInfoSwapPromiseMonitor(
            &swap_latency_info);
  }

  bool prevent_default = false;
  if (WebInputEvent::isMouseEventType(input_event.type)) {
    const WebMouseEvent& mouse_event =
        static_cast<const WebMouseEvent&>(input_event);
    TRACE_EVENT2("renderer", "HandleMouseMove", "x", mouse_event.x, "y",
                 mouse_event.y);
    context_menu_source_type_ = ui::MENU_SOURCE_MOUSE;
    prevent_default = delegate_->WillHandleMouseEvent(mouse_event);
  }

  if (WebInputEvent::isKeyboardEventType(input_event.type)) {
    context_menu_source_type_ = ui::MENU_SOURCE_KEYBOARD;
#if defined(OS_ANDROID)
    // The DPAD_CENTER key on Android has a dual semantic: (1) in the general
    // case it should behave like a select key (i.e. causing a click if a button
    // is focused). However, if a text field is focused (2), its intended
    // behavior is to just show the IME and don't propagate the key.
    // A typical use case is a web form: the DPAD_CENTER should bring up the IME
    // when clicked on an input text field and cause the form submit if clicked
    // when the submit button is focused, but not vice-versa.
    // The UI layer takes care of translating DPAD_CENTER into a RETURN key,
    // but at this point we have to swallow the event for the scenario (2).
    const WebKeyboardEvent& key_event =
        static_cast<const WebKeyboardEvent&>(input_event);
    if (key_event.nativeKeyCode == AKEYCODE_DPAD_CENTER &&
        widget_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      widget_->showImeIfNeeded();
      prevent_default = true;
    }
#endif
  }

  if (WebInputEvent::isGestureEventType(input_event.type)) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    if (input_event.type == WebInputEvent::GestureLongPress) {
      context_menu_source_type_ = ui::MENU_SOURCE_LONG_PRESS;
    } else if (input_event.type == WebInputEvent::GestureLongTap) {
      context_menu_source_type_ = ui::MENU_SOURCE_LONG_TAP;
    } else {
      context_menu_source_type_ = ui::MENU_SOURCE_TOUCH;
    }
    prevent_default =
        prevent_default || delegate_->WillHandleGestureEvent(gesture_event);
  }

  WebInputEventResult processed = prevent_default
                                      ? WebInputEventResult::HandledSuppressed
                                      : WebInputEventResult::NotHandled;
  if (input_event.type != WebInputEvent::Char || !suppress_next_char_events_) {
    suppress_next_char_events_ = false;
    if (processed == WebInputEventResult::NotHandled && widget_->webwidget())
      processed = widget_->webwidget()->handleInputEvent(input_event);
  }

  // TODO(dtapuska): Use the input_event.timeStampSeconds as the start
  // ideally this should be when the event was sent by the compositor to the
  // renderer. crbug.com/565348
  if (input_event.type == WebInputEvent::TouchStart ||
      input_event.type == WebInputEvent::TouchMove ||
      input_event.type == WebInputEvent::TouchEnd) {
    const WebTouchEvent& touch = static_cast<const WebTouchEvent&>(input_event);

    LogPassiveEventListenersUma(processed, touch.dispatchType,
                                input_event.timeStampSeconds, latency_info);

    if (input_event.type == WebInputEvent::TouchStart &&
        touch.dispatchType == WebInputEvent::Blocking &&
        base::TimeTicks::IsHighResolution()) {
      base::TimeTicks now = base::TimeTicks::Now();
      if (touch.dispatchedDuringFling) {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Event.Touch.TouchStartLatencyDuringFling",
            GetEventLatencyMicros(input_event.timeStampSeconds, now), 1,
            100000000, 50);
      } else {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Event.Touch.TouchStartLatencyOutsideFling",
            GetEventLatencyMicros(input_event.timeStampSeconds, now), 1,
            100000000, 50);
      }
    }
  } else if (input_event.type == WebInputEvent::MouseWheel) {
    LogPassiveEventListenersUma(
        processed,
        static_cast<const WebMouseWheelEvent&>(input_event).dispatchType,
        input_event.timeStampSeconds, latency_info);
  }

  // If this RawKeyDown event corresponds to a browser keyboard shortcut and
  // it's not processed by webkit, then we need to suppress the upcoming Char
  // events.
  bool is_keyboard_shortcut =
      input_event.type == WebInputEvent::RawKeyDown &&
      static_cast<const WebKeyboardEvent&>(input_event).isBrowserShortcut;
  if (processed == WebInputEventResult::NotHandled && is_keyboard_shortcut)
    suppress_next_char_events_ = true;

  InputEventAckState ack_result = processed == WebInputEventResult::NotHandled
                                      ? INPUT_EVENT_ACK_STATE_NOT_CONSUMED
                                      : INPUT_EVENT_ACK_STATE_CONSUMED;
  if (processed == WebInputEventResult::NotHandled &&
      input_event.type == WebInputEvent::TouchStart) {
    const WebTouchEvent& touch_event =
        static_cast<const WebTouchEvent&>(input_event);
    // Hit-test for all the pressed touch points. If there is a touch-handler
    // for any of the touch points, then the renderer should continue to receive
    // touch events.
    ack_result = INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    for (size_t i = 0; i < touch_event.touchesLength; ++i) {
      if (touch_event.touches[i].state == WebTouchPoint::StatePressed &&
          delegate_->HasTouchEventHandlersAt(
              gfx::ToFlooredPoint(touch_event.touches[i].position))) {
        ack_result = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
        break;
      }
    }
  }

  // Send gesture scroll events and their dispositions to the compositor thread,
  // so that they can be used to produce the elastic overscroll effect on Mac.
  if (input_event.type == WebInputEvent::GestureScrollBegin ||
      input_event.type == WebInputEvent::GestureScrollEnd ||
      input_event.type == WebInputEvent::GestureScrollUpdate) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    if (gesture_event.sourceDevice == blink::WebGestureDeviceTouchpad) {
      delegate_->ObserveGestureEventAndResult(
          gesture_event,
          event_overscroll ? event_overscroll->latest_overscroll_delta
                           : gfx::Vector2dF(),
          processed != WebInputEventResult::NotHandled);
    }
  }

  bool frame_pending =
      widget_->compositor() && widget_->compositor()->BeginMainFrameRequested();

  // If we don't have a fast and accurate Now(), we assume the input handlers
  // are heavy and rate limit them.
  bool rate_limiting_wanted = input_event.type == WebInputEvent::MouseMove ||
                              input_event.type == WebInputEvent::MouseWheel;
  if (rate_limiting_wanted && !start_time.is_null()) {
    base::TimeTicks end_time = base::TimeTicks::Now();
    total_input_handling_time_this_frame_ += (end_time - start_time);
    rate_limiting_wanted =
        total_input_handling_time_this_frame_.InMicroseconds() >
        kInputHandlingTimeThrottlingThresholdMicroseconds;
  }

  TRACE_EVENT_SYNTHETIC_DELAY_END("blink.HandleInputEvent");

  // Note that we can't use handling_event_type_ here since it will be overriden
  // by reentrant calls for events after the paused one.
  bool can_send_ack = !(ignore_ack_for_mouse_move_from_debugger_ &&
                        input_event.type == WebInputEvent::MouseMove);
  if (dispatch_type == DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN ||
      dispatch_type == DISPATCH_TYPE_NON_BLOCKING_NOTIFY_MAIN) {
    // |non_blocking| means it was ack'd already by the InputHandlerProxy
    // so let the delegate know the event has been handled.
    delegate_->NotifyInputEventHandled(input_event.type, ack_result);
  }

  if ((dispatch_type == DISPATCH_TYPE_BLOCKING ||
       dispatch_type == DISPATCH_TYPE_BLOCKING_NOTIFY_MAIN) &&
      can_send_ack) {
    std::unique_ptr<InputEventAck> response(new InputEventAck(
        input_event.type, ack_result, swap_latency_info,
        std::move(event_overscroll),
        WebInputEventTraits::GetUniqueTouchEventId(input_event)));
    if (rate_limiting_wanted && frame_pending && !widget_->is_hidden()) {
      // We want to rate limit the input events in this case, so we'll wait for
      // painting to finish before ACKing this message.
      TRACE_EVENT_INSTANT0(
          "renderer",
          "RenderWidgetInputHandler::OnHandleInputEvent ack throttled",
          TRACE_EVENT_SCOPE_THREAD);
      if (pending_input_event_ack_) {
        TRACE_EVENT_ASYNC_END0(
            "input", "RenderWidgetInputHandler::ThrottledInputEventAck",
            pending_input_event_ack_.get());
        // As two different kinds of events could cause us to postpone an ack
        // we send it now, if we have one pending. The Browser should never
        // send us the same kind of event we are delaying the ack for.
        delegate_->OnInputEventAck(std::move(pending_input_event_ack_));
      }
      pending_input_event_ack_ = std::move(response);
      TRACE_EVENT_ASYNC_BEGIN0(
          "input", "RenderWidgetInputHandler::ThrottledInputEventAck",
          pending_input_event_ack_.get());
      if (widget_->compositor())
        widget_->compositor()->NotifyInputThrottledUntilCommit();
    } else {
      delegate_->OnInputEventAck(std::move(response));
    }
  } else {
    DCHECK(!event_overscroll) << "Unexpected overscroll for un-acked event";
  }
  if (can_send_ack && RenderThreadImpl::current()) {
    RenderThreadImpl::current()
        ->GetRendererScheduler()
        ->DidHandleInputEventOnMainThread(input_event);
  }
  if (input_event.type == WebInputEvent::MouseMove)
    ignore_ack_for_mouse_move_from_debugger_ = false;

#if defined(OS_ANDROID)
  // Allow the IME to be shown when the focus changes as a consequence
  // of a processed touch end event.
  if (input_event.type == WebInputEvent::TouchEnd &&
      processed != WebInputEventResult::NotHandled) {
    delegate_->UpdateTextInputState(ShowIme::IF_NEEDED,
                                    ChangeSource::FROM_NON_IME);
  }
#elif defined(USE_AURA)
  // Show the virtual keyboard if enabled and a user gesture triggers a focus
  // change.
  if (processed != WebInputEventResult::NotHandled &&
      (input_event.type == WebInputEvent::TouchEnd ||
       input_event.type == WebInputEvent::MouseUp)) {
    delegate_->UpdateTextInputState(ShowIme::IF_NEEDED, ChangeSource::FROM_IME);
  }
#endif

  if (!prevent_default && WebInputEvent::isKeyboardEventType(input_event.type))
    delegate_->OnDidHandleKeyEvent();

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  // Virtual keyboard is not supported, so react to focus change immediately.
  if (processed != WebInputEventResult::NotHandled &&
      (input_event.type == WebInputEvent::TouchEnd ||
       input_event.type == WebInputEvent::MouseUp)) {
    delegate_->FocusChangeComplete();
  }
#endif
}

void RenderWidgetInputHandler::DidOverscrollFromBlink(
    const WebFloatSize& overscrollDelta,
    const WebFloatSize& accumulatedOverscroll,
    const WebFloatPoint& position,
    const WebFloatSize& velocity) {
  std::unique_ptr<DidOverscrollParams> params(new DidOverscrollParams());
  params->accumulated_overscroll = gfx::Vector2dF(
      accumulatedOverscroll.width, accumulatedOverscroll.height);
  params->latest_overscroll_delta =
      gfx::Vector2dF(overscrollDelta.width, overscrollDelta.height);
  // TODO(sataya.m): don't negate velocity once http://crbug.com/499743 is
  // fixed.
  params->current_fling_velocity =
      gfx::Vector2dF(-velocity.width, -velocity.height);
  params->causal_event_viewport_point = gfx::PointF(position.x, position.y);

  // If we're currently handling an event, stash the overscroll data such that
  // it can be bundled in the event ack.
  if (handling_event_overscroll_) {
    *handling_event_overscroll_ = std::move(params);
    return;
  }

  delegate_->OnDidOverscroll(*params);
}

bool RenderWidgetInputHandler::SendAckForMouseMoveFromDebugger() {
  if (handling_event_type_ == WebInputEvent::MouseMove) {
    // If we pause multiple times during a single mouse move event, we should
    // only send ACK once.
    if (!ignore_ack_for_mouse_move_from_debugger_) {
      std::unique_ptr<InputEventAck> ack(new InputEventAck(
          handling_event_type_, INPUT_EVENT_ACK_STATE_CONSUMED));
      delegate_->OnInputEventAck(std::move(ack));
      return true;
    }
  }
  return false;
}

void RenderWidgetInputHandler::IgnoreAckForMouseMoveFromDebugger() {
  ignore_ack_for_mouse_move_from_debugger_ = true;
}

void RenderWidgetInputHandler::FlushPendingInputEventAck() {
  if (pending_input_event_ack_) {
    TRACE_EVENT_ASYNC_END0("input",
                           "RenderWidgetInputHandler::ThrottledInputEventAck",
                           pending_input_event_ack_.get());
    delegate_->OnInputEventAck(std::move(pending_input_event_ack_));
  }
  total_input_handling_time_this_frame_ = base::TimeDelta();
}

}  // namespace content
