// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

#include <math.h>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/touch_action.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_sender.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

const char* GetEventAckName(InputEventAckState ack_result) {
  switch(ack_result) {
    case INPUT_EVENT_ACK_STATE_UNKNOWN: return "UNKNOWN";
    case INPUT_EVENT_ACK_STATE_CONSUMED: return "CONSUMED";
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED: return "NOT_CONSUMED";
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS: return "NO_CONSUMER_EXISTS";
    case INPUT_EVENT_ACK_STATE_IGNORED: return "IGNORED";
  }
  DLOG(WARNING) << "Unhandled InputEventAckState in GetEventAckName.";
  return "";
}

} // namespace

InputRouterImpl::Config::Config() {
}

InputRouterImpl::InputRouterImpl(IPC::Sender* sender,
                                 InputRouterClient* client,
                                 InputAckHandler* ack_handler,
                                 int routing_id,
                                 const Config& config)
    : sender_(sender),
      client_(client),
      ack_handler_(ack_handler),
      routing_id_(routing_id),
      select_range_pending_(false),
      move_caret_pending_(false),
      mouse_move_pending_(false),
      mouse_wheel_pending_(false),
      current_view_flags_(0),
      current_ack_source_(ACK_SOURCE_NONE),
      flush_requested_(false),
      touch_event_queue_(this, config.touch_config),
      gesture_event_queue_(this, this, config.gesture_config) {
  DCHECK(sender);
  DCHECK(client);
  DCHECK(ack_handler);
  UpdateTouchAckTimeoutEnabled();
}

InputRouterImpl::~InputRouterImpl() {}

void InputRouterImpl::Flush() {
  flush_requested_ = true;
  SignalFlushedIfNecessary();
}

bool InputRouterImpl::SendInput(scoped_ptr<IPC::Message> message) {
  DCHECK(IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart);
  switch (message->type()) {
    // Check for types that require an ACK.
    case InputMsg_SelectRange::ID:
      return SendSelectRange(message.Pass());
    case InputMsg_MoveCaret::ID:
      return SendMoveCaret(message.Pass());
    case InputMsg_HandleInputEvent::ID:
      NOTREACHED() << "WebInputEvents should never be sent via SendInput.";
      return false;
    default:
      return Send(message.release());
  }
}

void InputRouterImpl::SendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event) {
  if (mouse_event.event.type == WebInputEvent::MouseDown &&
      gesture_event_queue_.GetTouchpadTapSuppressionController()->
          ShouldDeferMouseDown(mouse_event))
      return;
  if (mouse_event.event.type == WebInputEvent::MouseUp &&
      gesture_event_queue_.GetTouchpadTapSuppressionController()->
          ShouldSuppressMouseUp())
      return;

  SendMouseEventImmediately(mouse_event);
}

void InputRouterImpl::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  SendWheelEvent(QueuedWheelEvent(wheel_event, false));
}

void InputRouterImpl::SendWheelEvent(const QueuedWheelEvent& wheel_event) {
  if (mouse_wheel_pending_) {
    // If there's already a mouse wheel event waiting to be sent to the
    // renderer, add the new deltas to that event. Not doing so (e.g., by
    // dropping the old event, as for mouse moves) results in very slow
    // scrolling on the Mac (on which many, very small wheel events are sent).
    // Note that we can't coalesce wheel events for pinches because the GEQ
    // expects one ACK for each (but it's fine to coalesce non-pinch wheels
    // into a pinch one).  Note that the GestureEventQueue ensures we only
    // ever have a single pinch event queued here.
    if (coalesced_mouse_wheel_events_.empty() ||
        wheel_event.synthesized_from_pinch ||
        !coalesced_mouse_wheel_events_.back().event.CanCoalesceWith(
            wheel_event.event)) {
      coalesced_mouse_wheel_events_.push_back(wheel_event);
    } else {
      coalesced_mouse_wheel_events_.back().event.CoalesceWith(
          wheel_event.event);
    }
    return;
  }

  mouse_wheel_pending_ = true;
  current_wheel_event_ = wheel_event;

  HISTOGRAM_COUNTS_100("Renderer.WheelQueueSize",
                       coalesced_mouse_wheel_events_.size());

  FilterAndSendWebInputEvent(
      wheel_event.event.event, wheel_event.event.latency, false);
}

void InputRouterImpl::SendKeyboardEvent(const NativeWebKeyboardEvent& key_event,
                                        const ui::LatencyInfo& latency_info,
                                        bool is_keyboard_shortcut) {
  // Put all WebKeyboardEvent objects in a queue since we can't trust the
  // renderer and we need to give something to the HandleKeyboardEvent
  // handler.
  key_queue_.push_back(key_event);
  HISTOGRAM_COUNTS_100("Renderer.KeyboardQueueSize", key_queue_.size());

  gesture_event_queue_.FlingHasBeenHalted();

  // Only forward the non-native portions of our event.
  FilterAndSendWebInputEvent(key_event, latency_info, is_keyboard_shortcut);
}

void InputRouterImpl::SendGestureEvent(
    const GestureEventWithLatencyInfo& original_gesture_event) {
  input_stream_validator_.Validate(original_gesture_event.event);

  GestureEventWithLatencyInfo gesture_event(original_gesture_event);

  if (touch_action_filter_.FilterGestureEvent(&gesture_event.event))
    return;

  if (gesture_event.event.sourceDevice == blink::WebGestureDeviceTouchscreen)
    touch_event_queue_.OnGestureScrollEvent(gesture_event);

  if (!gesture_event_queue_.ShouldForward(gesture_event))
    return;

  SendGestureEventImmediately(gesture_event);
}

void InputRouterImpl::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  input_stream_validator_.Validate(touch_event.event);
  touch_event_queue_.QueueEvent(touch_event);
}

// Forwards MouseEvent without passing it through
// TouchpadTapSuppressionController.
void InputRouterImpl::SendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event) {
  // Avoid spamming the renderer with mouse move events.  It is important
  // to note that WM_MOUSEMOVE events are anyways synthetic, but since our
  // thread is able to rapidly consume WM_MOUSEMOVE events, we may get way
  // more WM_MOUSEMOVE events than we wish to send to the renderer.
  if (mouse_event.event.type == WebInputEvent::MouseMove) {
    if (mouse_move_pending_) {
      if (!next_mouse_move_)
        next_mouse_move_.reset(new MouseEventWithLatencyInfo(mouse_event));
      else
        next_mouse_move_->CoalesceWith(mouse_event);
      return;
    }
    mouse_move_pending_ = true;
  }

  FilterAndSendWebInputEvent(mouse_event.event, mouse_event.latency, false);
}

void InputRouterImpl::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event) {
  if (WebTouchEventTraits::IsTouchSequenceStart(touch_event.event)) {
    touch_action_filter_.ResetTouchAction();
    // Note that if the previous touch-action was TOUCH_ACTION_NONE, enabling
    // the timeout here will not take effect until the *following* touch
    // sequence.  This is a desirable side-effect, giving the renderer a chance
    // to send a touch-action response without racing against the ack timeout.
    UpdateTouchAckTimeoutEnabled();
  }

  FilterAndSendWebInputEvent(touch_event.event, touch_event.latency, false);
}

void InputRouterImpl::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (gesture_event.event.type == WebInputEvent::GesturePinchUpdate &&
      gesture_event.event.sourceDevice == blink::WebGestureDeviceTouchpad) {
    SendSyntheticWheelEventForPinch(gesture_event);
    return;
  }

  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency, false);
}

const NativeWebKeyboardEvent* InputRouterImpl::GetLastKeyboardEvent() const {
  if (key_queue_.empty())
    return NULL;
  return &key_queue_.front();
}

bool InputRouterImpl::ShouldForwardTouchEvent() const {
  // Always send a touch event if the renderer has a touch-event handler or
  // there are pending touch events.
  return touch_event_queue_.has_handlers() || !touch_event_queue_.empty();
}

void InputRouterImpl::OnViewUpdated(int view_flags) {
  current_view_flags_ = view_flags;

  // A fixed page scale or mobile viewport should disable the touch ack timeout.
  UpdateTouchAckTimeoutEnabled();
}

bool InputRouterImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(InputRouterImpl, message)
    IPC_MESSAGE_HANDLER(InputHostMsg_HandleInputEvent_ACK, OnInputEventAck)
    IPC_MESSAGE_HANDLER(InputHostMsg_DidOverscroll, OnDidOverscroll)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MoveCaret_ACK, OnMsgMoveCaretAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectRange_ACK, OnSelectRangeAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_HANDLER(InputHostMsg_SetTouchAction,
                        OnSetTouchAction)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void InputRouterImpl::OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                                      InputEventAckState ack_result) {
  // Touchstart events sent to the renderer indicate a new touch sequence, but
  // in some cases we may filter out sending the touchstart - catch those here.
  if (WebTouchEventTraits::IsTouchSequenceStart(event.event) &&
      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS) {
    touch_action_filter_.ResetTouchAction();
    UpdateTouchAckTimeoutEnabled();
  }
  ack_handler_->OnTouchEventAck(event, ack_result);
}

void InputRouterImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  touch_event_queue_.OnGestureEventAck(event, ack_result);
  ack_handler_->OnGestureEventAck(event, ack_result);
}

bool InputRouterImpl::SendSelectRange(scoped_ptr<IPC::Message> message) {
  DCHECK(message->type() == InputMsg_SelectRange::ID);
  if (select_range_pending_) {
    next_selection_range_ = message.Pass();
    return true;
  }

  select_range_pending_ = true;
  return Send(message.release());
}

bool InputRouterImpl::SendMoveCaret(scoped_ptr<IPC::Message> message) {
  DCHECK(message->type() == InputMsg_MoveCaret::ID);
  if (move_caret_pending_) {
    next_move_caret_ = message.Pass();
    return true;
  }

  move_caret_pending_ = true;
  return Send(message.release());
}

bool InputRouterImpl::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void InputRouterImpl::FilterAndSendWebInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    bool is_keyboard_shortcut) {
  TRACE_EVENT1("input",
               "InputRouterImpl::FilterAndSendWebInputEvent",
               "type",
               WebInputEventTraits::GetName(input_event.type));

  // Any input event cancels a pending mouse move event.
  next_mouse_move_.reset();

  OfferToHandlers(input_event, latency_info, is_keyboard_shortcut);
}

void InputRouterImpl::OfferToHandlers(const WebInputEvent& input_event,
                                      const ui::LatencyInfo& latency_info,
                                      bool is_keyboard_shortcut) {
  output_stream_validator_.Validate(input_event);

  if (OfferToClient(input_event, latency_info))
    return;

  OfferToRenderer(input_event, latency_info, is_keyboard_shortcut);

  // Touch events should always indicate in the event whether they are
  // cancelable (respect ACK disposition) or not.
  bool ignores_ack = WebInputEventTraits::IgnoresAckDisposition(input_event);
  if (WebInputEvent::isTouchEventType(input_event.type)) {
    DCHECK(!ignores_ack ==
           static_cast<const blink::WebTouchEvent&>(input_event).cancelable);
  }

  // If we don't care about the ack disposition, send the ack immediately.
  if (ignores_ack) {
    ProcessInputEventAck(input_event.type,
                         INPUT_EVENT_ACK_STATE_IGNORED,
                         latency_info,
                         IGNORING_DISPOSITION);
  }
}

bool InputRouterImpl::OfferToClient(const WebInputEvent& input_event,
                                    const ui::LatencyInfo& latency_info) {
  bool consumed = false;

  InputEventAckState filter_ack =
      client_->FilterInputEvent(input_event, latency_info);
  switch (filter_ack) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
      // Send the ACK and early exit.
      next_mouse_move_.reset();
      ProcessInputEventAck(input_event.type, filter_ack, latency_info, CLIENT);
      // WARNING: |this| may be deleted at this point.
      consumed = true;
      break;
    case INPUT_EVENT_ACK_STATE_UNKNOWN:
      // Simply drop the event.
      consumed = true;
      break;
    default:
      break;
  }

  return consumed;
}

bool InputRouterImpl::OfferToRenderer(const WebInputEvent& input_event,
                                      const ui::LatencyInfo& latency_info,
                                      bool is_keyboard_shortcut) {
  if (Send(new InputMsg_HandleInputEvent(
          routing_id(), &input_event, latency_info, is_keyboard_shortcut))) {
    // Ack messages for ignored ack event types should never be sent by the
    // renderer. Consequently, such event types should not affect event time
    // or in-flight event count metrics.
    if (!WebInputEventTraits::IgnoresAckDisposition(input_event)) {
      input_event_start_time_ = TimeTicks::Now();
      client_->IncrementInFlightEventCount();
    }
    return true;
  }
  return false;
}

void InputRouterImpl::SendSyntheticWheelEventForPinch(
    const GestureEventWithLatencyInfo& pinch_event) {
  // We match typical trackpad behavior on Windows by sending fake wheel events
  // with the ctrl modifier set when we see trackpad pinch gestures.  Ideally
  // we'd someday get a standard 'pinch' event and send that instead.

  WebMouseWheelEvent wheelEvent;
  wheelEvent.type = WebInputEvent::MouseWheel;
  wheelEvent.timeStampSeconds = pinch_event.event.timeStampSeconds;
  wheelEvent.windowX = wheelEvent.x = pinch_event.event.x;
  wheelEvent.windowY = wheelEvent.y = pinch_event.event.y;
  wheelEvent.globalX = pinch_event.event.globalX;
  wheelEvent.globalY = pinch_event.event.globalY;
  wheelEvent.modifiers =
      pinch_event.event.modifiers | WebInputEvent::ControlKey;
  wheelEvent.deltaX = 0;
  // The function to convert scales to deltaY values is designed to be
  // compatible with websites existing use of wheel events, and with existing
  // Windows trackpad behavior.  In particular, we want:
  //  - deltas should accumulate via addition: f(s1*s2)==f(s1)+f(s2)
  //  - deltas should invert via negation: f(1/s) == -f(s)
  //  - zoom in should be positive: f(s) > 0 iff s > 1
  //  - magnitude roughly matches wheels: f(2) > 25 && f(2) < 100
  //  - a formula that's relatively easy to use from JavaScript
  // Note that 'wheel' event deltaY values have their sign inverted.  So to
  // convert a wheel deltaY back to a scale use Math.exp(-deltaY/100).
  DCHECK_GT(pinch_event.event.data.pinchUpdate.scale, 0);
  wheelEvent.deltaY = 100.0f * log(pinch_event.event.data.pinchUpdate.scale);
  wheelEvent.hasPreciseScrollingDeltas = true;
  wheelEvent.wheelTicksX = 0;
  wheelEvent.wheelTicksY =
      pinch_event.event.data.pinchUpdate.scale > 1 ? 1 : -1;

  SendWheelEvent(QueuedWheelEvent(
      MouseWheelEventWithLatencyInfo(wheelEvent, pinch_event.latency), true));
}

void InputRouterImpl::OnInputEventAck(
    const InputHostMsg_HandleInputEvent_ACK_Params& ack) {
  client_->DecrementInFlightEventCount();

  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES("MPArch.IIR_InputEventDelta", delta);

  if (ack.overscroll) {
    DCHECK(ack.type == WebInputEvent::MouseWheel ||
           ack.type == WebInputEvent::GestureScrollUpdate);
    OnDidOverscroll(*ack.overscroll);
  }

  ProcessInputEventAck(ack.type, ack.state, ack.latency, RENDERER);
  // WARNING: |this| may be deleted at this point.

  // This is used only for testing, and the other end does not use the
  // source object.  On linux, specifying
  // Source<RenderWidgetHost> results in a very strange
  // runtime error in the epilogue of the enclosing
  // (ProcessInputEventAck) method, but not on other platforms; using
  // 'void' instead is just as safe (since NotificationSource
  // is not actually typesafe) and avoids this error.
  int type = static_cast<int>(ack.type);
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_INPUT_EVENT_ACK,
      Source<void>(this),
      Details<int>(&type));
}

void InputRouterImpl::OnDidOverscroll(const DidOverscrollParams& params) {
  client_->DidOverscroll(params);
}

void InputRouterImpl::OnMsgMoveCaretAck() {
  move_caret_pending_ = false;
  if (next_move_caret_)
    SendMoveCaret(next_move_caret_.Pass());
}

void InputRouterImpl::OnSelectRangeAck() {
  select_range_pending_ = false;
  if (next_selection_range_)
    SendSelectRange(next_selection_range_.Pass());
}

void InputRouterImpl::OnHasTouchEventHandlers(bool has_handlers) {
  TRACE_EVENT1("input", "InputRouterImpl::OnHasTouchEventHandlers",
               "has_handlers", has_handlers);

  // Lack of a touch handler indicates that the page either has no touch-action
  // modifiers or that all its touch-action modifiers are auto. Resetting the
  // touch-action here allows forwarding of subsequent gestures even if the
  // underlying touches never reach the router.
  // TODO(jdduke): Reset touch-action only at the end of a touch sequence to
  // prevent potentially strange mid-sequence behavior, crbug.com/375940.
  if (!has_handlers)
    touch_action_filter_.ResetTouchAction();

  touch_event_queue_.OnHasTouchEventHandlers(has_handlers);
  client_->OnHasTouchEventHandlers(has_handlers);
}

void InputRouterImpl::OnSetTouchAction(TouchAction touch_action) {
  // Synthetic touchstart events should get filtered out in RenderWidget.
  DCHECK(touch_event_queue_.IsPendingAckTouchStart());
  TRACE_EVENT1("input", "InputRouterImpl::OnSetTouchAction",
               "action", touch_action);

  touch_action_filter_.OnSetTouchAction(touch_action);

  // TOUCH_ACTION_NONE should disable the touch ack timeout.
  UpdateTouchAckTimeoutEnabled();
}

void InputRouterImpl::ProcessInputEventAck(
    WebInputEvent::Type event_type,
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency_info,
    AckSource ack_source) {
  TRACE_EVENT2("input", "InputRouterImpl::ProcessInputEventAck",
               "type", WebInputEventTraits::GetName(event_type),
               "ack", GetEventAckName(ack_result));

  // Note: The keyboard ack must be treated carefully, as it may result in
  // synchronous destruction of |this|. Handling immediately guards against
  // future references to |this|, as with |auto_reset_current_ack_source| below.
  if (WebInputEvent::isKeyboardEventType(event_type)) {
    ProcessKeyboardAck(event_type, ack_result);
    // WARNING: |this| may be deleted at this point.
    return;
  }

  base::AutoReset<AckSource> auto_reset_current_ack_source(
      &current_ack_source_, ack_source);

  if (WebInputEvent::isMouseEventType(event_type)) {
    ProcessMouseAck(event_type, ack_result);
  } else if (event_type == WebInputEvent::MouseWheel) {
    ProcessWheelAck(ack_result, latency_info);
  } else if (WebInputEvent::isTouchEventType(event_type)) {
    ProcessTouchAck(ack_result, latency_info);
  } else if (WebInputEvent::isGestureEventType(event_type)) {
    ProcessGestureAck(event_type, ack_result, latency_info);
  } else if (event_type != WebInputEvent::Undefined) {
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::BAD_ACK_MESSAGE);
  }

  SignalFlushedIfNecessary();
}

void InputRouterImpl::ProcessKeyboardAck(blink::WebInputEvent::Type type,
                                         InputEventAckState ack_result) {
  if (key_queue_.empty()) {
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::UNEXPECTED_ACK);
  } else if (key_queue_.front().type != type) {
    // Something must be wrong. Clear the |key_queue_| and char event
    // suppression so that we can resume from the error.
    key_queue_.clear();
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::UNEXPECTED_EVENT_TYPE);
  } else {
    NativeWebKeyboardEvent front_item = key_queue_.front();
    key_queue_.pop_front();

    ack_handler_->OnKeyboardEventAck(front_item, ack_result);
    // WARNING: This InputRouterImpl can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this InputRouterImpl).
    // TODO(jdduke): crbug.com/274029 - Make ack-triggered shutdown async.
  }
}

void InputRouterImpl::ProcessMouseAck(blink::WebInputEvent::Type type,
                                      InputEventAckState ack_result) {
  if (type != WebInputEvent::MouseMove)
    return;

  DCHECK(mouse_move_pending_);
  mouse_move_pending_ = false;

  if (next_mouse_move_) {
    DCHECK(next_mouse_move_->event.type == WebInputEvent::MouseMove);
    scoped_ptr<MouseEventWithLatencyInfo> next_mouse_move
        = next_mouse_move_.Pass();
    SendMouseEvent(*next_mouse_move);
  }
}

void InputRouterImpl::ProcessWheelAck(InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency) {
  // TODO(miletus): Add renderer side latency to each uncoalesced mouse
  // wheel event and add terminal component to each of them.
  current_wheel_event_.event.latency.AddNewLatencyFrom(latency);

  if (current_wheel_event_.synthesized_from_pinch) {
    // Ack the GesturePinchUpdate event that generated this wheel event.
    ProcessInputEventAck(WebInputEvent::GesturePinchUpdate,
                         ack_result,
                         current_wheel_event_.event.latency,
                         current_ack_source_);
  } else {
    // Process the unhandled wheel event here before calling SendWheelEvent()
    // since it will mutate current_wheel_event_.
    ack_handler_->OnWheelEventAck(current_wheel_event_.event, ack_result);
  }

  // Mark the wheel event complete only after the ACKs have been handled above.
  // For example, ACKing the GesturePinchUpdate could cause another
  // GesturePinchUpdate to be sent, which should queue a wheel event rather than
  // send it immediately.
  mouse_wheel_pending_ = false;

  // Send the next (coalesced or synthetic) mouse wheel event.
  if (!coalesced_mouse_wheel_events_.empty()) {
    QueuedWheelEvent next_wheel_event = coalesced_mouse_wheel_events_.front();
    coalesced_mouse_wheel_events_.pop_front();
    SendWheelEvent(next_wheel_event);
  }
}

void InputRouterImpl::ProcessGestureAck(WebInputEvent::Type type,
                                        InputEventAckState ack_result,
                                        const ui::LatencyInfo& latency) {
  if (!gesture_event_queue_.ExpectingGestureAck())
    return;

  // |gesture_event_queue_| will forward to OnGestureEventAck when appropriate.
  gesture_event_queue_.ProcessGestureAck(ack_result, type, latency);
}

void InputRouterImpl::ProcessTouchAck(
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency) {
  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_.ProcessTouchAck(ack_result, latency);
}

void InputRouterImpl::UpdateTouchAckTimeoutEnabled() {
  // Mobile sites tend to be well-behaved with respect to touch handling, so
  // they have less need for the touch timeout fallback.
  const bool fixed_page_scale = (current_view_flags_ & FIXED_PAGE_SCALE) != 0;
  const bool mobile_viewport = (current_view_flags_ & MOBILE_VIEWPORT) != 0;

  // TOUCH_ACTION_NONE will prevent scrolling, in which case the timeout serves
  // little purpose. It's also a strong signal that touch handling is critical
  // to page functionality, so the timeout could do more harm than good.
  const bool touch_action_none =
      touch_action_filter_.allowed_touch_action() == TOUCH_ACTION_NONE;

  const bool touch_ack_timeout_enabled = !fixed_page_scale &&
                                         !mobile_viewport &&
                                         !touch_action_none;
  touch_event_queue_.SetAckTimeoutEnabled(touch_ack_timeout_enabled);
}

void InputRouterImpl::SignalFlushedIfNecessary() {
  if (!flush_requested_)
    return;

  if (HasPendingEvents())
    return;

  flush_requested_ = false;
  client_->DidFlush();
}

bool InputRouterImpl::HasPendingEvents() const {
  return !touch_event_queue_.empty() ||
         !gesture_event_queue_.empty() ||
         !key_queue_.empty() ||
         mouse_move_pending_ ||
         mouse_wheel_pending_ ||
         select_range_pending_ ||
         move_caret_pending_;
}

InputRouterImpl::QueuedWheelEvent::QueuedWheelEvent()
    : synthesized_from_pinch(false) {
}

InputRouterImpl::QueuedWheelEvent::QueuedWheelEvent(
    const MouseWheelEventWithLatencyInfo& event,
    bool synthesized_from_pinch)
    : event(event), synthesized_from_pinch(synthesized_from_pinch) {
}

InputRouterImpl::QueuedWheelEvent::~QueuedWheelEvent() {
}

}  // namespace content
