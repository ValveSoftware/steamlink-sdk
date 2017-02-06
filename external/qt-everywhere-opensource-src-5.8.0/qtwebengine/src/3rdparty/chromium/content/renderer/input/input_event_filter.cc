// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_event_filter.h"

#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/geometry/vector2d_f.h"

using blink::WebInputEvent;

#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(name, ...) \
  case name::ID:                    \
    return #name;

const char* GetInputMessageTypeName(const IPC::Message& message) {
  switch (message.type()) {
#include "content/common/input_messages.h"
    default:
      NOTREACHED() << "Invalid message type: " << message.type();
      break;
  };
  return "NonInputMsgType";
}

namespace content {

InputEventFilter::InputEventFilter(
    const base::Callback<void(const IPC::Message&)>& main_listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& target_task_runner)
    : main_task_runner_(main_task_runner),
      main_listener_(main_listener),
      sender_(NULL),
      target_task_runner_(target_task_runner),
      current_overscroll_params_(NULL) {
  DCHECK(target_task_runner_.get());
}

void InputEventFilter::SetBoundHandler(const Handler& handler) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  handler_ = handler;
}

void InputEventFilter::SetIsFlingingInMainThreadEventQueue(int routing_id,
                                                           bool is_flinging) {
  RouteQueueMap::iterator iter = route_queues_.find(routing_id);
  if (iter == route_queues_.end() || !iter->second)
    return;

  iter->second->set_is_flinging(is_flinging);
}

void InputEventFilter::RegisterRoutingID(int routing_id) {
  base::AutoLock locked(routes_lock_);
  routes_.insert(routing_id);
  route_queues_[routing_id].reset(new MainThreadEventQueue(routing_id, this));
}

void InputEventFilter::UnregisterRoutingID(int routing_id) {
  base::AutoLock locked(routes_lock_);
  routes_.erase(routing_id);
  route_queues_.erase(routing_id);
}

void InputEventFilter::DidOverscroll(int routing_id,
                                     const DidOverscrollParams& params) {
  if (current_overscroll_params_) {
    current_overscroll_params_->reset(new DidOverscrollParams(params));
    return;
  }

  SendMessage(std::unique_ptr<IPC::Message>(
      new InputHostMsg_DidOverscroll(routing_id, params)));
}

void InputEventFilter::DidStartFlinging(int routing_id) {
  SetIsFlingingInMainThreadEventQueue(routing_id, true);
}

void InputEventFilter::DidStopFlinging(int routing_id) {
  SetIsFlingingInMainThreadEventQueue(routing_id, false);
  SendMessage(base::WrapUnique(new InputHostMsg_DidStopFlinging(routing_id)));
}

void InputEventFilter::NotifyInputEventHandled(int routing_id,
                                               blink::WebInputEvent::Type type,
                                               InputEventAckState ack_result) {
  DCHECK(target_task_runner_->BelongsToCurrentThread());
  RouteQueueMap::iterator iter = route_queues_.find(routing_id);
  if (iter == route_queues_.end() || !iter->second)
    return;

  iter->second->EventHandled(type, ack_result);
}

void InputEventFilter::OnFilterAdded(IPC::Sender* sender) {
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  sender_ = sender;
}

void InputEventFilter::OnFilterRemoved() {
  sender_ = NULL;
}

void InputEventFilter::OnChannelClosing() {
  sender_ = NULL;
}

// This function returns true if the IPC message is one that the compositor
// thread can handle *or* one that needs to preserve relative order with
// messages that the compositor thread can handle. Returning true for a message
// type means that the message will go through an extra copy and thread hop, so
// use with care.
static bool RequiresThreadBounce(const IPC::Message& message) {
  return IPC_MESSAGE_ID_CLASS(message.type()) == InputMsgStart;
}

bool InputEventFilter::OnMessageReceived(const IPC::Message& message) {
  if (!RequiresThreadBounce(message))
    return false;

  TRACE_EVENT0("input", "InputEventFilter::OnMessageReceived::InputMessage");

  {
    base::AutoLock locked(routes_lock_);
    if (routes_.find(message.routing_id()) == routes_.end())
      return false;
  }

  target_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&InputEventFilter::ForwardToHandler, this, message));
  return true;
}

InputEventFilter::~InputEventFilter() {
  DCHECK(!current_overscroll_params_);
}

void InputEventFilter::ForwardToHandler(const IPC::Message& message) {
  DCHECK(!handler_.is_null());
  DCHECK(target_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("input", "InputEventFilter::ForwardToHandler",
               "message_type", GetInputMessageTypeName(message));

  if (message.type() != InputMsg_HandleInputEvent::ID) {
    TRACE_EVENT_INSTANT0(
        "input",
        "InputEventFilter::ForwardToHandler::ForwardToMainListener",
        TRACE_EVENT_SCOPE_THREAD);
    main_task_runner_->PostTask(FROM_HERE, base::Bind(main_listener_, message));
    return;
  }

  int routing_id = message.routing_id();
  InputMsg_HandleInputEvent::Param params;
  if (!InputMsg_HandleInputEvent::Read(&message, &params))
    return;
  const WebInputEvent* event = std::get<0>(params);
  ui::LatencyInfo latency_info = std::get<1>(params);
  InputEventDispatchType dispatch_type = std::get<2>(params);
  DCHECK(event);
  DCHECK(dispatch_type == DISPATCH_TYPE_BLOCKING ||
         dispatch_type == DISPATCH_TYPE_NON_BLOCKING);

  bool send_ack = dispatch_type == DISPATCH_TYPE_BLOCKING;

  // Intercept |DidOverscroll| notifications, bundling any triggered overscroll
  // response with the input event ack.
  std::unique_ptr<DidOverscrollParams> overscroll_params;
  base::AutoReset<std::unique_ptr<DidOverscrollParams>*>
      auto_reset_current_overscroll_params(
          &current_overscroll_params_, send_ack ? &overscroll_params : NULL);

  InputEventAckState ack_state = handler_.Run(routing_id, event, &latency_info);

  if (ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
      ack_state == INPUT_EVENT_ACK_STATE_NOT_CONSUMED) {
    DCHECK(!overscroll_params);
    RouteQueueMap::iterator iter = route_queues_.find(routing_id);
    if (iter != route_queues_.end())
      send_ack &= iter->second->HandleEvent(event, latency_info, dispatch_type,
                                            ack_state);
  }

  if (!send_ack)
    return;

  InputEventAck ack(event->type, ack_state, latency_info,
                    std::move(overscroll_params),
                    WebInputEventTraits::GetUniqueTouchEventId(*event));
  SendMessage(std::unique_ptr<IPC::Message>(
      new InputHostMsg_HandleInputEvent_ACK(routing_id, ack)));
}

void InputEventFilter::SendMessage(std::unique_ptr<IPC::Message> message) {
  DCHECK(target_task_runner_->BelongsToCurrentThread());

  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&InputEventFilter::SendMessageOnIOThread, this,
                            base::Passed(&message)));
}

void InputEventFilter::SendMessageOnIOThread(
    std::unique_ptr<IPC::Message> message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!sender_)
    return;  // Filter was removed.

  sender_->Send(message.release());
}

void InputEventFilter::SendEventToMainThread(
    int routing_id,
    const blink::WebInputEvent* event,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  TRACE_EVENT_INSTANT0(
      "input", "InputEventFilter::ForwardToHandler::SendEventToMainThread",
      TRACE_EVENT_SCOPE_THREAD);
  IPC::Message new_msg =
      InputMsg_HandleInputEvent(routing_id, event, latency_info, dispatch_type);
  main_task_runner_->PostTask(FROM_HERE, base::Bind(main_listener_, new_msg));
}

void InputEventFilter::SendInputEventAck(int routing_id,
                                         blink::WebInputEvent::Type type,
                                         InputEventAckState ack_result,
                                         uint32_t touch_event_id) {
  InputEventAck ack(type, ack_result, touch_event_id);
  SendMessage(std::unique_ptr<IPC::Message>(
      new InputHostMsg_HandleInputEvent_ACK(routing_id, ack)));
}

}  // namespace content
