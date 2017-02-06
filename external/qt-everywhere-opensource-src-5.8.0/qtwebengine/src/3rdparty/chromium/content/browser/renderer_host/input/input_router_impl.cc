// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

#include <math.h>

#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
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
#include "ui/events/blink/blink_event_util.h"
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
using blink::WebTouchEvent;

namespace content {
namespace {

const char* GetEventAckName(InputEventAckState ack_result) {
  switch(ack_result) {
    case INPUT_EVENT_ACK_STATE_UNKNOWN: return "UNKNOWN";
    case INPUT_EVENT_ACK_STATE_CONSUMED: return "CONSUMED";
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED: return "NOT_CONSUMED";
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS: return "NO_CONSUMER_EXISTS";
    case INPUT_EVENT_ACK_STATE_IGNORED: return "IGNORED";
    case INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING:
      return "SET_NON_BLOCKING";
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
      frame_tree_node_id_(-1),
      select_message_pending_(false),
      move_caret_pending_(false),
      mouse_move_pending_(false),
      current_ack_source_(ACK_SOURCE_NONE),
      flush_requested_(false),
      active_renderer_fling_count_(0),
      touch_scroll_started_sent_(false),
      wheel_event_queue_(this, kDefaultWheelScrollTransactionMs),
      touch_event_queue_(this, config.touch_config),
      gesture_event_queue_(this, this, config.gesture_config),
      device_scale_factor_(1.f) {
  DCHECK(sender);
  DCHECK(client);
  DCHECK(ack_handler);
  UpdateTouchAckTimeoutEnabled();
}

InputRouterImpl::~InputRouterImpl() {
  STLDeleteElements(&pending_select_messages_);
}

bool InputRouterImpl::SendInput(std::unique_ptr<IPC::Message> message) {
  DCHECK(IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart);
  switch (message->type()) {
    // Check for types that require an ACK.
    case InputMsg_SelectRange::ID:
    case InputMsg_MoveRangeSelectionExtent::ID:
      return SendSelectMessage(std::move(message));
    case InputMsg_MoveCaret::ID:
      return SendMoveCaret(std::move(message));
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
  wheel_event_queue_.QueueEvent(wheel_event);
}

void InputRouterImpl::SendKeyboardEvent(
    const NativeWebKeyboardEventWithLatencyInfo& key_event) {
  // Put all WebKeyboardEvent objects in a queue since we can't trust the
  // renderer and we need to give something to the HandleKeyboardEvent
  // handler.
  key_queue_.push_back(key_event);
  LOCAL_HISTOGRAM_COUNTS_100("Renderer.KeyboardQueueSize", key_queue_.size());

  gesture_event_queue_.FlingHasBeenHalted();

  // Only forward the non-native portions of our event.
  FilterAndSendWebInputEvent(key_event.event, key_event.latency);
}

void InputRouterImpl::SendGestureEvent(
    const GestureEventWithLatencyInfo& original_gesture_event) {
  input_stream_validator_.Validate(original_gesture_event.event);

  GestureEventWithLatencyInfo gesture_event(original_gesture_event);

  if (touch_action_filter_.FilterGestureEvent(&gesture_event.event))
    return;

  wheel_event_queue_.OnGestureScrollEvent(gesture_event);

  if (gesture_event.event.sourceDevice == blink::WebGestureDeviceTouchscreen) {
    if (gesture_event.event.type == blink::WebInputEvent::GestureScrollBegin) {
      touch_scroll_started_sent_ = false;
    } else if(!touch_scroll_started_sent_
        && gesture_event.event.type ==
            blink::WebInputEvent::GestureScrollUpdate) {
      // A touch scroll hasn't really started until the first
      // GestureScrollUpdate event.  Eg. if the page consumes all touchmoves
      // then no scrolling really ever occurs (even though we still send
      // GestureScrollBegin).
      touch_event_queue_.PrependTouchScrollNotification();
      touch_scroll_started_sent_ = true;
    }
    touch_event_queue_.OnGestureScrollEvent(gesture_event);
  }

  gesture_event_queue_.QueueEvent(gesture_event);
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
    current_mouse_move_ = mouse_event;
  }

  FilterAndSendWebInputEvent(mouse_event.event, mouse_event.latency);
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

  FilterAndSendWebInputEvent(touch_event.event, touch_event.latency);
}

void InputRouterImpl::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency);
}

const NativeWebKeyboardEvent* InputRouterImpl::GetLastKeyboardEvent() const {
  if (key_queue_.empty())
    return NULL;
  return &key_queue_.front().event;
}

void InputRouterImpl::NotifySiteIsMobileOptimized(bool is_mobile_optimized) {
  touch_event_queue_.SetIsMobileOptimizedSite(is_mobile_optimized);
}

void InputRouterImpl::RequestNotificationWhenFlushed() {
  flush_requested_ = true;
  SignalFlushedIfNecessary();
}

bool InputRouterImpl::HasPendingEvents() const {
  return !touch_event_queue_.empty() || !gesture_event_queue_.empty() ||
         !key_queue_.empty() || mouse_move_pending_ ||
         wheel_event_queue_.has_pending() || select_message_pending_ ||
         move_caret_pending_ || active_renderer_fling_count_ > 0;
}

void InputRouterImpl::SetDeviceScaleFactor(float device_scale_factor) {
  device_scale_factor_ = device_scale_factor;
}

bool InputRouterImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(InputRouterImpl, message)
    IPC_MESSAGE_HANDLER(InputHostMsg_HandleInputEvent_ACK, OnInputEventAck)
    IPC_MESSAGE_HANDLER(InputHostMsg_DidOverscroll, OnDidOverscroll)
    IPC_MESSAGE_HANDLER(InputHostMsg_MoveCaret_ACK, OnMsgMoveCaretAck)
    IPC_MESSAGE_HANDLER(InputHostMsg_SelectRange_ACK, OnSelectMessageAck)
    IPC_MESSAGE_HANDLER(InputHostMsg_MoveRangeSelectionExtent_ACK,
                        OnSelectMessageAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_HANDLER(InputHostMsg_SetTouchAction,
                        OnSetTouchAction)
    IPC_MESSAGE_HANDLER(InputHostMsg_DidStopFlinging, OnDidStopFlinging)
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

void InputRouterImpl::OnFilteringTouchEvent(
    const WebTouchEvent& touch_event) {
  // The event stream given to the renderer is not guaranteed to be
  // valid based on the current TouchEventStreamValidator rules. This event will
  // never be given to the renderer, but in order to ensure that the event
  // stream |output_stream_validator_| sees is valid, we give events which are
  // filtered out to the validator. crbug.com/589111 proposes adding an
  // additional validator for the events which are actually sent to the
  // renderer.
  output_stream_validator_.Validate(touch_event);
}

void InputRouterImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  touch_event_queue_.OnGestureEventAck(event, ack_result);
  ack_handler_->OnGestureEventAck(event, ack_result);
}

void InputRouterImpl::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency_info) {
  client_->ForwardGestureEventWithLatencyInfo(event, latency_info);
}

void InputRouterImpl::SendMouseWheelEventImmediately(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  FilterAndSendWebInputEvent(wheel_event.event, wheel_event.latency);
}

void InputRouterImpl::OnMouseWheelEventAck(
    const MouseWheelEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  ack_handler_->OnWheelEventAck(event, ack_result);
}

bool InputRouterImpl::SendSelectMessage(std::unique_ptr<IPC::Message> message) {
  DCHECK(message->type() == InputMsg_SelectRange::ID ||
         message->type() == InputMsg_MoveRangeSelectionExtent::ID);

  // TODO(jdduke): Factor out common logic between selection and caret-related
  //               IPC messages.
  if (select_message_pending_) {
    if (!pending_select_messages_.empty() &&
        pending_select_messages_.back()->type() == message->type()) {
      delete pending_select_messages_.back();
      pending_select_messages_.pop_back();
    }

    pending_select_messages_.push_back(message.release());
    return true;
  }

  select_message_pending_ = true;
  return Send(message.release());
}

bool InputRouterImpl::SendMoveCaret(std::unique_ptr<IPC::Message> message) {
  DCHECK(message->type() == InputMsg_MoveCaret::ID);
  if (move_caret_pending_) {
    next_move_caret_ = std::move(message);
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
    const ui::LatencyInfo& latency_info) {
  TRACE_EVENT1("input",
               "InputRouterImpl::FilterAndSendWebInputEvent",
               "type",
               WebInputEventTraits::GetName(input_event.type));
  TRACE_EVENT_WITH_FLOW2("input,benchmark,devtools.timeline",
                         "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "SendInputEventUI",
                         "frameTreeNodeId", frame_tree_node_id_);

  // Any input event cancels a pending mouse move event.
  next_mouse_move_.reset();

  OfferToHandlers(input_event, latency_info);
}

void InputRouterImpl::OfferToHandlers(const WebInputEvent& input_event,
                                      const ui::LatencyInfo& latency_info) {
  output_stream_validator_.Validate(input_event);

  if (OfferToClient(input_event, latency_info))
    return;

  bool should_block = WebInputEventTraits::ShouldBlockEventStream(input_event);
  OfferToRenderer(input_event, latency_info,
                  should_block
                      ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
                      : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING);

  // Generate a synthetic ack if the event was sent so it doesn't block.
  if (!should_block) {
    ProcessInputEventAck(
        input_event.type, INPUT_EVENT_ACK_STATE_IGNORED, latency_info,
        WebInputEventTraits::GetUniqueTouchEventId(input_event),
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
      ProcessInputEventAck(
          input_event.type, filter_ack, latency_info,
          WebInputEventTraits::GetUniqueTouchEventId(input_event), CLIENT);
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
                                      InputEventDispatchType dispatch_type) {
  DCHECK(input_event.type != blink::WebInputEvent::GestureFlingStart ||
         static_cast<const blink::WebGestureEvent&>(input_event)
                 .data.flingStart.velocityX != 0.0 ||
         static_cast<const blink::WebGestureEvent&>(input_event)
                 .data.flingStart.velocityY != 0.0);

  // This conversion is temporary. WebInputEvent should be generated
  // directly from ui::Event with the viewport coordinates. See
  // crbug.com/563730.
  std::unique_ptr<blink::WebInputEvent> event_in_viewport =
      ui::ScaleWebInputEvent(input_event, device_scale_factor_);
  const WebInputEvent* event_to_send =
      event_in_viewport ? event_in_viewport.get() : &input_event;

  if (Send(new InputMsg_HandleInputEvent(routing_id(), event_to_send,
                                         latency_info, dispatch_type))) {
    // Ack messages for ignored ack event types should never be sent by the
    // renderer. Consequently, such event types should not affect event time
    // or in-flight event count metrics.
    if (dispatch_type == InputEventDispatchType::DISPATCH_TYPE_BLOCKING) {
      input_event_start_time_ = TimeTicks::Now();
      client_->IncrementInFlightEventCount();
    }
    return true;
  }
  return false;
}

void InputRouterImpl::OnInputEventAck(const InputEventAck& ack) {
  client_->DecrementInFlightEventCount();
  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES("MPArch.IIR_InputEventDelta", delta);

  if (ack.overscroll) {
    DCHECK(ack.type == WebInputEvent::MouseWheel ||
           ack.type == WebInputEvent::GestureScrollUpdate);
    OnDidOverscroll(*ack.overscroll);
  }

  ProcessInputEventAck(ack.type, ack.state, ack.latency,
                       ack.unique_touch_event_id, RENDERER);
}

void InputRouterImpl::OnDidOverscroll(const DidOverscrollParams& params) {
  client_->DidOverscroll(params);
}

void InputRouterImpl::OnMsgMoveCaretAck() {
  move_caret_pending_ = false;
  if (next_move_caret_)
    SendMoveCaret(std::move(next_move_caret_));
}

void InputRouterImpl::OnSelectMessageAck() {
  select_message_pending_ = false;
  if (!pending_select_messages_.empty()) {
    std::unique_ptr<IPC::Message> next_message =
        base::WrapUnique(pending_select_messages_.front());
    pending_select_messages_.pop_front();

    SendSelectMessage(std::move(next_message));
  }
}

void InputRouterImpl::OnHasTouchEventHandlers(bool has_handlers) {
  TRACE_EVENT1("input", "InputRouterImpl::OnHasTouchEventHandlers",
               "has_handlers", has_handlers);

  // Lack of a touch handler indicates that the page either has no touch-action
  // modifiers or that all its touch-action modifiers are auto. Resetting the
  // touch-action here allows forwarding of subsequent gestures even if the
  // underlying touches never reach the router.
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

void InputRouterImpl::OnDidStopFlinging() {
  DCHECK_GT(active_renderer_fling_count_, 0);
  // Note that we're only guaranteed to get a fling end notification from the
  // renderer, not from any other consumers. Consequently, the GestureEventQueue
  // cannot use this bookkeeping for logic like tap suppression.
  --active_renderer_fling_count_;
  SignalFlushedIfNecessary();

  client_->DidStopFlinging();
}

void InputRouterImpl::ProcessInputEventAck(WebInputEvent::Type event_type,
                                           InputEventAckState ack_result,
                                           const ui::LatencyInfo& latency_info,
                                           uint32_t unique_touch_event_id,
                                           AckSource ack_source) {
  TRACE_EVENT2("input", "InputRouterImpl::ProcessInputEventAck",
               "type", WebInputEventTraits::GetName(event_type),
               "ack", GetEventAckName(ack_result));

  // Note: The keyboard ack must be treated carefully, as it may result in
  // synchronous destruction of |this|. Handling immediately guards against
  // future references to |this|, as with |auto_reset_current_ack_source| below.
  if (WebInputEvent::isKeyboardEventType(event_type)) {
    ProcessKeyboardAck(event_type, ack_result, latency_info);
    // WARNING: |this| may be deleted at this point.
    return;
  }

  base::AutoReset<AckSource> auto_reset_current_ack_source(
      &current_ack_source_, ack_source);

  if (WebInputEvent::isMouseEventType(event_type)) {
    ProcessMouseAck(event_type, ack_result, latency_info);
  } else if (event_type == WebInputEvent::MouseWheel) {
    ProcessWheelAck(ack_result, latency_info);
  } else if (WebInputEvent::isTouchEventType(event_type)) {
    ProcessTouchAck(ack_result, latency_info, unique_touch_event_id);
  } else if (WebInputEvent::isGestureEventType(event_type)) {
    ProcessGestureAck(event_type, ack_result, latency_info);
  } else if (event_type != WebInputEvent::Undefined) {
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::BAD_ACK_MESSAGE);
  }

  SignalFlushedIfNecessary();
}

void InputRouterImpl::ProcessKeyboardAck(blink::WebInputEvent::Type type,
                                         InputEventAckState ack_result,
                                         const ui::LatencyInfo& latency) {
  if (key_queue_.empty()) {
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::UNEXPECTED_ACK);
  } else if (key_queue_.front().event.type != type) {
    // Something must be wrong. Clear the |key_queue_| and char event
    // suppression so that we can resume from the error.
    key_queue_.clear();
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::UNEXPECTED_EVENT_TYPE);
  } else {
    NativeWebKeyboardEventWithLatencyInfo front_item = key_queue_.front();
    front_item.latency.AddNewLatencyFrom(latency);
    key_queue_.pop_front();

    ack_handler_->OnKeyboardEventAck(front_item, ack_result);
    // WARNING: This InputRouterImpl can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this InputRouterImpl).
    // TODO(jdduke): crbug.com/274029 - Make ack-triggered shutdown async.
  }
}

void InputRouterImpl::ProcessMouseAck(blink::WebInputEvent::Type type,
                                      InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency) {
  if (type != WebInputEvent::MouseMove)
    return;

  current_mouse_move_.latency.AddNewLatencyFrom(latency);
  ack_handler_->OnMouseEventAck(current_mouse_move_, ack_result);

  DCHECK(mouse_move_pending_);
  mouse_move_pending_ = false;

  if (next_mouse_move_) {
    DCHECK(next_mouse_move_->event.type == WebInputEvent::MouseMove);
    std::unique_ptr<MouseEventWithLatencyInfo> next_mouse_move =
        std::move(next_mouse_move_);
    SendMouseEvent(*next_mouse_move);
  }
}

void InputRouterImpl::ProcessWheelAck(InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency) {
  wheel_event_queue_.ProcessMouseWheelAck(ack_result, latency);
}

void InputRouterImpl::ProcessGestureAck(WebInputEvent::Type type,
                                        InputEventAckState ack_result,
                                        const ui::LatencyInfo& latency) {
  if (type == blink::WebInputEvent::GestureFlingStart &&
      ack_result == INPUT_EVENT_ACK_STATE_CONSUMED) {
    ++active_renderer_fling_count_;
  }

  // |gesture_event_queue_| will forward to OnGestureEventAck when appropriate.
  gesture_event_queue_.ProcessGestureAck(ack_result, type, latency);
}

void InputRouterImpl::ProcessTouchAck(InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency,
                                      uint32_t unique_touch_event_id) {
  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_.ProcessTouchAck(ack_result, latency,
                                     unique_touch_event_id);
}

void InputRouterImpl::UpdateTouchAckTimeoutEnabled() {
  // TOUCH_ACTION_NONE will prevent scrolling, in which case the timeout serves
  // little purpose. It's also a strong signal that touch handling is critical
  // to page functionality, so the timeout could do more harm than good.
  const bool touch_ack_timeout_enabled =
      touch_action_filter_.allowed_touch_action() != TOUCH_ACTION_NONE;
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

void InputRouterImpl::SetFrameTreeNodeId(int frameTreeNodeId) {
  frame_tree_node_id_ = frameTreeNodeId;
}

}  // namespace content
